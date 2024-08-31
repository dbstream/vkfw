/**
 * Xcb window backend functions
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/window.h>
#include <xcb/xcb.h>

typedef struct VKFWxcbwindow_T VKFWxcbwindow;

extern VKFWxcbwindow *vkfw_xcb_focus_window;

struct VKFWxcbwindow_T {
	VKFWwindow window;
	xcb_window_t wid;
	xcb_window_t parent;
	uint32_t pointer_mode;

	int last_x, last_y;
	int warp_x, warp_y;
};

VKFWxcbwindow *
vkfwXcbXIDToWindow (xcb_window_t wid);

VKFWwindow *
vkfwXcbAllocWindow (void);

VkResult
vkfwXcbCreateWindow (VKFWwindow *handle);

void
vkfwXcbDestroyWindow (VKFWwindow *handle);

VkResult
vkfwXcbCreateWindowSurface (VKFWwindow *handle, VkSurfaceKHR *surface);

VkResult
vkfwXcbShowWindow (VKFWwindow *handle);

VkResult
vkfwXcbHideWindow (VKFWwindow *handle);

VkResult
vkfwXcbSetWindowTitle (VKFWwindow *handle, const char *title);

void
vkfwXcbUpdatePointerMode (VKFWwindow *handle);
