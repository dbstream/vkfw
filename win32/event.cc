/**
 * Win32 event handling.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/vector.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include "event.h"
#include "win32.h"
#include "window.h"

/**
 *   WndProc, smooth resizing, and reentrancy.
 *
 * We run PeekMessageW etc. in a Win32 fiber (similar to an application-managed
 * thread) so we can "far-return" to the caller of vkfwGetEvent when we receive
 * window events we care about. Otherwise, the modal loop in DefWindowProcW will
 * block.
 *
 * Our WndProc can be called in a multitude of contexts:
 * - from the PeekMessageW loop executing in fiber context.
 * - recursively from DefWindowProcW.
 * - by CreateWindowExW.
 * - by the Vulkan or D3D12 implementation.
 * It needs to be able to handle all of these cases.
 */

static LPVOID main_fiber;
static LPVOID event_fiber;

static VKFWevent *current_event;
static int current_mode;
static uint64_t current_timeout;
static VkResult current_result;

static void WINAPI
event_loop (LPVOID fiber_parameter)
{
	(void) fiber_parameter;

	BOOL b;
	MSG msg;

	for (;; SwitchToFiber (main_fiber)) {
		if ((b = PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE)) < 0) {
			current_result = VK_ERROR_UNKNOWN;
			continue;
		}

		if (b) {
			DispatchMessageW (&msg);
			if (current_event->type == VKFW_EVENT_NONE)
				current_event->type = VKFW_EVENT_NULL;
			continue;
		}

		if (!current_timeout)
			continue;

		if (current_mode == VKFW_EVENT_MODE_DEADLINE) {
			uint64_t now = vkfwGetTime ();
			if (now >= current_timeout)
				continue;
			current_timeout -= now;
		}

		current_timeout = (current_timeout + 999) / 1000;

		MsgWaitForMultipleObjects (0, nullptr, TRUE, current_timeout, QS_ALLINPUT);
		if ((b = PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE)) < 0) {
			current_result = VK_ERROR_UNKNOWN;
			continue;
		}

		if (b) {
			DispatchMessageW (&msg);
			if (current_event->type == VKFW_EVENT_NONE)
				current_event->type = VKFW_EVENT_NULL;
		}
	}
}

VkResult
vkfwWin32InitEvents (void)
{
	main_fiber = ConvertThreadToFiberEx (nullptr, FIBER_FLAG_FLOAT_SWITCH);
	if (!main_fiber)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	event_fiber = CreateFiberEx (0, 0, FIBER_FLAG_FLOAT_SWITCH, event_loop, nullptr);
	if (!event_fiber)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	return VK_SUCCESS;
}

void
vkfwWin32TerminateEvents (void)
{
	DeleteFiber (event_fiber);
}

LRESULT CALLBACK
vkfwWin32WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (GetCurrentFiber () == main_fiber) {
		if (msg == WM_CREATE) {
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: WM_CREATE()\n");
			LPCREATESTRUCTW ci = (LPCREATESTRUCTW) lparam;

			SetWindowLongPtr (hwnd, 0, (LONG_PTR) ci->lpCreateParams);
			return 0;
		}

		if (msg == WM_DESTROY) {
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: WM_DESTROY()\n");
			SetWindowLongPtr (hwnd, 0, (LONG_PTR) nullptr);
			return 0;
		}

		return DefWindowProcW (hwnd, msg, wparam, lparam);
	}

	VKFWwin32window *w = (VKFWwin32window *) GetWindowLongPtr (hwnd, 0);
	if (!w) {
		/**
		 * SetWindowLongPtr is called on the main fiber at WM_CREATE
		 * time. Our WndProc being called on the event fiber without
		 * our VKFWwin32window pointer should never happen.
		 */
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: WndProc was called on the event fiber, but GetWindowLongPtr returned nullptr\n");
		return DefWindowProcW (hwnd, msg, wparam, lparam);
	}

	current_event->window = (VKFWwindow *) w;

	switch (msg) {
	case WM_CLOSE:
		current_event->type = VKFW_EVENT_WINDOW_CLOSE_REQUEST;
		SwitchToFiber (main_fiber);
		return 0;
	case WM_SIZE:
		current_event->type = VKFW_EVENT_WINDOW_RESIZE_NOTIFY;
		current_event->extent.width = LOWORD (lparam);
		current_event->extent.height = HIWORD (lparam);
		SwitchToFiber (main_fiber);
		return 0;
	default:
		return DefWindowProcW (hwnd, msg, wparam, lparam);
	}
}

VkResult
vkfwWin32GetEvent (VKFWevent *e, int mode, uint64_t timeout)
{
	current_result = VK_SUCCESS;
	current_event = e;
	current_mode = mode;
	current_timeout = timeout;

	SwitchToFiber (event_fiber);
	return current_result;
}
