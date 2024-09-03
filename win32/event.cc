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

static bool are_we_alive = true;

struct queued_message {
	VKFWwin32window *w;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;
};

static size_t queue_size = 0;
static size_t queue_offset = 0;
static VKFWvector<queued_message> message_queue;

/**
 * Enqueue a message onto the internal message queue.
 */
static void
message_enqueue (VKFWwin32window *w, UINT msg, WPARAM wparam, LPARAM lparam)
{
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: message_enqueue(0x%x)\n", msg);
	if (!are_we_alive)
		return;

	if (w->window.flags & VKFW_WINDOW_DELETED)
		return;

	if (queue_size == message_queue.size ()) {
		size_t old_size = queue_size;
		size_t new_size = queue_size ? 2 * queue_size : 16;
		if (!message_queue.resize (new_size)) {
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: OOM in message_enqueue\n");
			are_we_alive = false;
			return;
		}

		for (size_t i = 0; i < queue_offset; i++)
			message_queue[old_size + i] = message_queue[i];
	}

	vkfwRefWindow ((VKFWwindow *) w);
	message_queue[(queue_offset + queue_size) % message_queue.size ()] = {
		w, msg, wparam, lparam
	};
	queue_size++;
}

static queued_message
message_dequeue (void)
{
	queued_message m = message_queue[queue_offset];
	if (++queue_offset == message_queue.size ())
		queue_offset = 0;
	queue_size--;
	return m;
}

LRESULT CALLBACK
vkfwWin32WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	VKFWwin32window *w = (VKFWwin32window *) GetWindowLongPtr (hwnd, 0);
	if (!w) {
		if (msg == WM_CREATE) {
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: WM_CREATE()\n");
			LPCREATESTRUCTW ci = (LPCREATESTRUCTW) lparam;

			vkfwRefWindow ((VKFWwindow *) ci->lpCreateParams);
			SetWindowLongPtr (hwnd, 0, (LONG_PTR) ci->lpCreateParams);
			return 0;
		}

		/**
		 * Some unknown message was received prior to WM_CREATE. We
		 * better let DefWindowProcW handle it.
		 */
		return DefWindowProcW (hwnd, msg, wparam, lparam);
	}

	switch (msg) {
	case WM_DESTROY:
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: WM_DESTROY()\n");
		SetWindowLongPtr (hwnd, 0, (LONG_PTR) nullptr);
		vkfwUnrefWindow ((VKFWwindow *) w);
		return 0;
	case WM_CLOSE:
		message_enqueue (w, msg, wparam, lparam);
		return 0;
	default:
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Win32: unknown message=0x%x\n", msg);
		return DefWindowProcW (hwnd, msg, wparam, lparam);
	}
}

static VkResult
pump_events (int mode, uint64_t timeout)
{
	MSG msg;
	BOOL b;

	if (!are_we_alive)
		return VK_ERROR_DEVICE_LOST;

	if (timeout == UINT64_MAX) {
		if ((b = GetMessageW (&msg, nullptr, 0, 0)) <=0) {
			/** We received a WM_QUIT or an error occured. */
			are_we_alive = false;
			return VK_ERROR_DEVICE_LOST;
		}

		DispatchMessageW (&msg);
		return are_we_alive ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
	}

	if ((b = PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))) {
		if (b < 0) {
			are_we_alive = false;
			return VK_ERROR_DEVICE_LOST;
		}

		DispatchMessageW (&msg);
		return are_we_alive ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
	}

	if (mode == VKFW_EVENT_MODE_DEADLINE) {
		uint64_t now = vkfwGetTime ();
		if (timeout > now)
			timeout -= now;
		else
			timeout = 0;
	}

	timeout = (timeout + 999) / 1000;
	if (!timeout)
		return VK_SUCCESS;

	MsgWaitForMultipleObjects (0, nullptr, 0, timeout, QS_ALLINPUT);
	if ((b = PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))) {
		if (b < 0) {
			are_we_alive = false;
			return VK_ERROR_DEVICE_LOST;
		}

		DispatchMessageW (&msg);
		return are_we_alive ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
	}

	return VK_SUCCESS;
}

VkResult
vkfwWin32GetEvent (VKFWevent *e, int mode, uint64_t timeout)
{
	VkResult result;
	queued_message msg;

	if (!are_we_alive)
		return VK_ERROR_DEVICE_LOST;

	if (!queue_size) {
		result = pump_events (mode, timeout);
		if (result != VK_SUCCESS)
			return result;
	}

	if (!queue_size)
		return VK_SUCCESS;

	e->type = VKFW_EVENT_NULL;
	msg = message_dequeue ();
	if (msg.w->window.flags & VKFW_WINDOW_DELETED) {
		vkfwUnrefWindow ((VKFWwindow *) msg.w);
		return VK_SUCCESS;
	}

	vkfwUnrefWindow ((VKFWwindow *) msg.w);
	e->window = (VKFWwindow *) msg.w;

	if (msg.msg == WM_CLOSE) {
		e->type = VKFW_EVENT_WINDOW_CLOSE_REQUEST;
	}

	return VK_SUCCESS;
}
