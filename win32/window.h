/**
 * Win32 window
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <VKFW/window.h>

#include "win32.h"

typedef struct VKFWwin32window_T VKFWwin32window;

struct VKFWwin32window_T {
	VKFWwindow window;
	HWND hwnd;
};

VKFWwindow *
vkfwWin32AllocWindow (void);

VkResult
vkfwWin32CreateWindow (VKFWwindow *handle);

void
vkfwWin32DestroyWindow (VKFWwindow *handle);

VkResult
vkfwWin32CreateWindowSurface (VKFWwindow *handle, VkSurfaceKHR *surface);

VkResult
vkfwWin32ShowWindow (VKFWwindow *handle);

VkResult
vkfwWin32HideWindow (VKFWwindow *handle);

VkResult
vkfwWin32SetWindowTitle (VKFWwindow *handle, const char *title);
