/**
 * Window creation on Wayland.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <VKFW/window.h>
#include "wayland.h"
#include "window.h"

VkResult
vkfwWlCreateWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	w->surface = wl_compositor_create_surface (vkfwWlCompositor);
	if (!w->surface)
		return VK_ERROR_INITIALIZATION_FAILED;

	w->xdg = xdg_wm_base_get_xdg_surface (vkfwXdgWmBase, w->surface);
	if (!w->xdg) {
		wl_surface_destroy (w->surface);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	w->toplevel = xdg_surface_get_toplevel (w->xdg);
	if (!w->toplevel) {
		xdg_surface_destroy (w->xdg);
		wl_surface_destroy (w->surface);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	return VK_SUCCESS;
}

void
vkfwWlDestroyWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;
	xdg_toplevel_destroy (w->toplevel);
	xdg_surface_destroy (w->xdg);
	wl_surface_destroy (w->surface);
	wl_display_flush (vkfwWlDisplay);
}
