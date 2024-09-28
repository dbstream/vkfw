/**
 * Wayland window functions.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/window.h>
#include "wayland.h"

typedef struct VKFWwlwindow_T VKFWwlwindow;

struct VKFWwlwindow_T {
	VKFWwindow window;
	wl_surface *content_surface;
	wl_subsurface *content_subsurface;

	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	wl_surface *frame_surface;
	wp_viewport *frame_viewport;

	int32_t configured_width, configured_height;

	zxdg_toplevel_decoration_v1 *decoration_v1;

	bool visible;
	bool use_csd;
	bool has_csd;
	bool has_csd_buffer_attached;
};

VkResult
vkfwWlCreateWindow (VKFWwindow *window);

void
vkfwWlDestroyWindow (VKFWwindow *window);

VkResult
vkfwWlShowWindow (VKFWwindow *window);

VkResult
vkfwWlHideWindow (VKFWwindow *window);
