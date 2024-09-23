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
};

VkResult
vkfwWlCreateWindow (VKFWwindow *window);

void
vkfwWlDestroyWindow (VKFWwindow *window);
