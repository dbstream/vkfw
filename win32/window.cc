/**
 * Win32 window
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_WIN32_KHR

#include <VKFW/logging.h>
#include <VKFW/vkfw.h>
#include <VKFW/window.h>
#include <stdlib.h>

#include "utils.h"
#include "win32.h"
#include "window.h"

VKFWwindow *
vkfwWin32AllocWindow (void)
{
	return (VKFWwindow *) malloc (sizeof (VKFWwin32window));
}

VkResult
vkfwWin32CreateWindow (VKFWwindow *handle)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	w->hwnd = CreateWindowExW (
		0,
		L"VKFW window",
		L"<unnamed>",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		handle->extent.width,
		handle->extent.height,
		nullptr,
		nullptr,
		vkfwHInstance,
		w
	);

	if (!w->hwnd)
		return VK_ERROR_UNKNOWN;

	return VK_SUCCESS;
}

void
vkfwWin32DestroyWindow (VKFWwindow *handle)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	DestroyWindow (w->hwnd);
}

VkResult
vkfwWin32CreateWindowSurface (VKFWwindow *handle, VkSurfaceKHR *out)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	VkWin32SurfaceCreateInfoKHR ci {};
	ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	ci.hinstance = vkfwHInstance;
	ci.hwnd = w->hwnd;
	return vkCreateWin32SurfaceKHR (vkfwLoadedInstance, &ci, nullptr, out);
}

VkResult
vkfwWin32ShowWindow (VKFWwindow *handle)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	ShowWindow (w->hwnd, SW_SHOWNORMAL);
	return VK_SUCCESS;
}

VkResult
vkfwWin32HideWindow (VKFWwindow *handle)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	ShowWindow (w->hwnd, SW_HIDE);
	return VK_SUCCESS;
}

VkResult
vkfwWin32SetWindowTitle (VKFWwindow *handle, const char *title)
{
	VKFWwin32window *w = (VKFWwin32window *) handle;

	wchar_t *s = vkfwUTF8ToUTF16 (title);
	if (!s)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	SetWindowTextW (w->hwnd, s);
	free (s);
	return VK_SUCCESS;
}
