/**
 * Window creation on Wayland.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/event.h>
#include <VKFW/logging.h>
#include <VKFW/vkfw.h>
#include <VKFW/window.h>
#include "wayland.h"
#include "window.h"

static void
handle_xdg_surface_configure (void *window, xdg_surface *surface,
	uint32_t serial)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->configured_width > 0 && w->configured_height > 0) {
		VKFWevent e {};
		e.type = VKFW_EVENT_WINDOW_RESIZE_NOTIFY;
		e.window = (VKFWwindow *) w;
		e.extent.width = w->configured_width;
		e.extent.height = w->configured_height;

		xdg_surface_set_window_geometry (surface, 0, 0,
			w->configured_width, w->configured_height);
		xdg_surface_ack_configure (surface, serial);
		vkfwSendEventToApplication (&e);
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = handle_xdg_surface_configure
};

static void
handle_toplevel_configure (void *window, xdg_toplevel *toplevel,
	int32_t width, int32_t height, wl_array *states)
{
	(void) toplevel;
	(void) states;

	VKFWwlwindow *w = (VKFWwlwindow *) window;
	w->configured_width = width;
	w->configured_height = height;
}

static void
handle_toplevel_close (void *window, xdg_toplevel *toplevel)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	VKFWevent e {};
	e.type = VKFW_EVENT_WINDOW_CLOSE_REQUEST;
	e.window = (VKFWwindow *) w;
	vkfwSendEventToApplication (&e);
}

static void
handle_toplevel_configure_bounds (void *window, xdg_toplevel *toplevel,
	int32_t width, int32_t height)
{
	(void) window;
	(void) toplevel;
	(void) width;
	(void) height;
}

static void
handle_toplevel_wm_capabilities (void *window, xdg_toplevel *toplevel,
	wl_array *capabilities)
{
	(void) window;
	(void) toplevel;
	(void) capabilities;
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = handle_toplevel_configure,
	.close = handle_toplevel_close,
	.configure_bounds = handle_toplevel_configure_bounds,
	.wm_capabilities = handle_toplevel_wm_capabilities
};

VkResult
vkfwWlCreateWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	w->toplevel = nullptr;
	w->configured_width = window->extent.width;
	w->configured_height = window->extent.height;

	w->surface = wl_compositor_create_surface (vkfwWlCompositor);
	if (!w->surface)
		return VK_ERROR_INITIALIZATION_FAILED;

	w->xdg = xdg_wm_base_get_xdg_surface (vkfwXdgWmBase, w->surface);
	if (!w->xdg) {
		wl_surface_destroy (w->surface);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	xdg_surface_add_listener (w->xdg, &xdg_surface_listener, w);
	return VK_SUCCESS;
}

void
vkfwWlDestroyWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->toplevel)
		xdg_toplevel_destroy (w->toplevel);
	xdg_surface_destroy (w->xdg);
	wl_surface_destroy (w->surface);
}

VkResult
vkfwWlShowWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->toplevel)
		return VK_SUCCESS;

	w->toplevel = xdg_surface_get_toplevel (w->xdg);
	if (!w->toplevel)
		return VK_ERROR_UNKNOWN;

	xdg_toplevel_add_listener (w->toplevel, &toplevel_listener, w);
	return VK_SUCCESS;
}

VkResult
vkfwWlHideWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->toplevel) {
		xdg_toplevel_destroy (w->toplevel);
		w->toplevel = nullptr;
	}

	return VK_SUCCESS;
}
