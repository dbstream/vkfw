/**
 * Wayland window functions.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/window.h>
#include "wayland.h"

typedef struct VKFWwlwindow_T VKFWwlwindow;

struct VKFWwlwindow_T {
	VKFWwindow window;
	wl_surface *surface;
	xdg_surface *xdg;
	xdg_toplevel *toplevel;

	int32_t configured_width, configured_height;
};

VkResult
vkfwWlCreateWindow (VKFWwindow *window);

void
vkfwWlDestroyWindow (VKFWwindow *window);

VkResult
vkfwWlShowWindow (VKFWwindow *window);

VkResult
vkfwWlHideWindow (VKFWwindow *window);
