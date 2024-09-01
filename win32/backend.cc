/**
 * Win32 window and events backend.
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_WIN32_KHR

#include <VKFW/logging.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>

#include "event.h"
#include "window.h"

static VkResult
vkfwWin32RequestInstanceExtensions (void)
{
	VkResult result;

	result = vkfwRequestInstanceExtension (VK_KHR_SURFACE_EXTENSION_NAME, true);
	if (result != VK_SUCCESS)
		return result;

	result = vkfwRequestInstanceExtension (VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true);
	if (result != VK_SUCCESS)
		return result;

	vkfwPrintf (VKFW_LOG_BACKEND, "a\n");

	return VK_SUCCESS;
}

static VkResult
vkfwWin32QueryPresentSupport (VkPhysicalDevice device, uint32_t queue,
	VkBool32 *out)
{
	vkfwPrintf (VKFW_LOG_BACKEND, "before queryPresentSupport\n");
	*out = vkGetPhysicalDeviceWin32PresentationSupportKHR (device, queue);
	vkfwPrintf (VKFW_LOG_BACKEND, "after queryPresentSupport\n");
	return VK_SUCCESS;
}

static void
vkfwWin32CloseConnection (void)
{
	UnregisterClassW (L"VKFW window", vkfwHInstance);
}

/**
 * GCC allows struct declarations like
 *   VKFWplatform platform = {
 *     .foo = bar,
 *     .baz = xyz
 *   };
 * , but MSVC doesn't like this.  See comment in platform.cc.
 */

extern VKFWwindowbackend vkfwBackendWin32;

static VkResult
vkfwWin32OpenConnection (void)
{
	vkfwBackendWin32.close_connection = vkfwWin32CloseConnection;
	vkfwBackendWin32.request_instance_extensions = vkfwWin32RequestInstanceExtensions;
	vkfwBackendWin32.alloc_window = vkfwWin32AllocWindow;
	vkfwBackendWin32.create_window = vkfwWin32CreateWindow;
	vkfwBackendWin32.destroy_window = vkfwWin32DestroyWindow;
	vkfwBackendWin32.create_surface = vkfwWin32CreateWindowSurface;
	vkfwBackendWin32.query_present_support = vkfwWin32QueryPresentSupport;
	vkfwBackendWin32.show_window = vkfwWin32ShowWindow;
	vkfwBackendWin32.hide_window = vkfwWin32HideWindow;
	vkfwBackendWin32.set_title = vkfwWin32SetWindowTitle;

	WNDCLASSW wc = {};
	wc.lpfnWndProc = vkfwWin32WndProc;
	wc.hInstance = vkfwHInstance;
	wc.lpszClassName = L"VKFW window";
	if (!RegisterClassW (&wc))
		return VK_ERROR_INITIALIZATION_FAILED;

	return VK_SUCCESS;
}

VKFWwindowbackend vkfwBackendWin32 = { vkfwWin32OpenConnection };
