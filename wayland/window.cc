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

#include <stdlib.h>
#include <string.h>

enum {
	CSD_TOP = 25,
	CSD_BOTTOM = 5,
	CSD_LEFT = 5,
	CSD_RIGHT = 5,
	CSD_WIDTH = CSD_LEFT + CSD_RIGHT,
	CSD_HEIGHT = CSD_TOP + CSD_BOTTOM,
	MIN_WIDTH = 30 + CSD_WIDTH,
	MIN_HEIGHT = 30 + CSD_HEIGHT
};

static void
create_csd_decorations (VKFWwlwindow *w)
{
	w->close_button_surface = wl_compositor_create_surface (vkfwWlCompositor);
	if (!w->close_button_surface)
		return;

	wl_surface_set_user_data (w->close_button_surface, w);

	w->close_button_subsurface = wl_subcompositor_get_subsurface (vkfwWlSubcompositor,
		w->close_button_surface, w->frame_surface);
	if (!w->close_button_subsurface) {
		wl_surface_destroy (w->close_button_surface);
		return;
	}

	wl_subsurface_place_above (w->close_button_subsurface, w->frame_surface);
	wl_surface_attach (w->close_button_surface, vkfwWlCloseButtonBuffer, 0, 0);
	w->has_csd_decorations = true;
}

static void
destroy_csd_decorations (VKFWwlwindow *w)
{
	wl_subsurface_destroy (w->close_button_subsurface);
	wl_surface_destroy (w->close_button_surface);
	w->has_csd_decorations = false;
	return;
}

static void
handle_xdg_surface_configure (void *window, xdg_surface *surface,
	uint32_t serial)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (MIN_WIDTH > w->configured_width)
		w->configured_width = MIN_WIDTH;
	if (MIN_HEIGHT > w->configured_height)
		w->configured_height = MIN_HEIGHT;

	xdg_surface_set_window_geometry (surface, 0, 0,
		w->configured_width, w->configured_height);

	if (vkfwWlSupportCSD) {
		wp_viewport_set_destination (w->frame_viewport,
			w->configured_width, w->configured_height);

		if (!w->has_csd_buffer_attached) {
			wl_surface_attach (w->frame_surface, vkfwWlFrameBuffer, 0, 0);
			w->has_csd_buffer_attached = true;
		}
	}

	if (w->use_csd && !w->has_csd) {
		wl_subsurface_set_position (w->content_subsurface, CSD_LEFT, CSD_TOP);
		w->has_csd = true;
	} else if (!w->use_csd && w->has_csd) {
		wl_subsurface_set_position (w->content_subsurface, 0, 0);
		w->has_csd = false;
	}

	if (w->has_csd && !w->has_csd_decorations)
		create_csd_decorations (w);
	else if (!w->has_csd && w->has_csd_decorations)
		destroy_csd_decorations (w);

	if (w->has_csd_decorations) {
		wl_subsurface_set_position (w->close_button_subsurface, w->configured_width - 25, 5);
		wl_surface_commit (w->close_button_surface);
	}

	VKFWevent e {};
	e.type = VKFW_EVENT_WINDOW_RESIZE_NOTIFY;
	e.window = (VKFWwindow *) w;
	e.extent.width = w->configured_width;
	e.extent.height = w->configured_height;

	if (w->has_csd) {
		e.extent.width -= CSD_WIDTH;
		e.extent.height -= CSD_HEIGHT;
	}

	vkfwSendEventToApplication (&e);

	xdg_surface_ack_configure (surface, serial);
	wl_surface_commit (w->content_surface);
	if (vkfwWlSupportCSD)
		wl_surface_commit (w->frame_surface);
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
	if (width)	w->configured_width = width;
	if (height)	w->configured_height = height;
}

static void
handle_toplevel_close (void *window, xdg_toplevel *toplevel)
{
	(void) toplevel;

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

static void
handle_toplevel_decoration_v1_configure (void *window, zxdg_toplevel_decoration_v1 *deco,
	uint32_t mode)
{
	(void) deco;

	VKFWwlwindow *w = (VKFWwlwindow *) window;

	w->use_csd = vkfwWlSupportCSD && (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static const struct zxdg_toplevel_decoration_v1_listener toplevel_decoration_v1_listener = {
	.configure = handle_toplevel_decoration_v1_configure
};

VkResult
vkfwWlCreateWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	w->content_surface = nullptr;
	w->content_subsurface = nullptr;
	w->xdg_surface = nullptr;
	w->frame_surface = nullptr;
	w->xdg_toplevel = nullptr;
	w->decoration_v1 = nullptr;

	w->configured_width = window->extent.width;
	w->configured_height = window->extent.height;
	w->visible = false;
	w->use_csd = vkfwWlSupportCSD;
	w->has_csd = false;
	w->has_csd_buffer_attached = false;
	w->has_csd_decorations = false;

	w->title = nullptr;

	w->content_surface = wl_compositor_create_surface (vkfwWlCompositor);
	if (!w->content_surface)
		return VK_ERROR_INITIALIZATION_FAILED;

	wl_surface_set_user_data (w->content_surface, w);

	if (vkfwWlSupportCSD) {
		w->frame_surface = wl_compositor_create_surface (vkfwWlCompositor);
		if (!w->frame_surface) {
			wl_surface_destroy (w->content_surface);
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		wl_surface_set_user_data (w->frame_surface, w);

		w->xdg_surface = xdg_wm_base_get_xdg_surface (vkfwXdgWmBase,
			w->frame_surface);
	} else
		w->xdg_surface = xdg_wm_base_get_xdg_surface (vkfwXdgWmBase,
			w->content_surface);

	if (!w->xdg_surface) {
		if (vkfwWlSupportCSD)
			wl_surface_destroy (w->frame_surface);
		wl_surface_destroy (w->content_surface);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (vkfwWlSupportCSD) {
		w->content_subsurface = wl_subcompositor_get_subsurface (vkfwWlSubcompositor,
			w->content_surface, w->frame_surface);
		if (!w->content_subsurface) {
			xdg_surface_destroy (w->xdg_surface);
			wl_surface_destroy (w->frame_surface);
			wl_surface_destroy (w->content_surface);
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		wl_subsurface_place_above (w->content_subsurface, w->frame_surface);

		/**
		 * We have no way of intercepting surface commits by the
		 * application. Therefore, we must desync the content
		 * subsurface.
		 */
		wl_subsurface_set_desync (w->content_subsurface);
	}

	xdg_surface_add_listener (w->xdg_surface, &xdg_surface_listener, w);
	return VK_SUCCESS;
}

void
vkfwWlDestroyWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->has_csd_decorations)
		destroy_csd_decorations (w);

	if (w->xdg_toplevel) {
		if (w->decoration_v1)
			zxdg_toplevel_decoration_v1_destroy (w->decoration_v1);

		if (vkfwWlSupportCSD)
			wp_viewport_destroy (w->frame_viewport);

		xdg_toplevel_destroy (w->xdg_toplevel);
	}

	xdg_surface_destroy (w->xdg_surface);

	if (vkfwWlSupportCSD) {
		wl_subsurface_destroy (w->content_subsurface);
		wl_surface_destroy (w->frame_surface);
	}

	wl_surface_destroy (w->content_surface);
	wl_display_flush (vkfwWlDisplay);

	if (w->title)
		free (w->title);
}

VkResult
vkfwWlShowWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (w->visible)
		return VK_SUCCESS;

	w->use_csd = vkfwWlSupportCSD;

	w->xdg_toplevel = xdg_surface_get_toplevel (w->xdg_surface);
	if (!w->xdg_toplevel)
		return VK_ERROR_INITIALIZATION_FAILED;

	xdg_toplevel_add_listener (w->xdg_toplevel, &toplevel_listener, w);

	if (vkfwWlSupportCSD) {
		w->frame_viewport = wp_viewporter_get_viewport (vkfwWpViewporter,
			w->frame_surface);
		if (!w->frame_viewport) {
			xdg_toplevel_destroy (w->xdg_toplevel);
			w->xdg_toplevel = nullptr;
			return VK_ERROR_INITIALIZATION_FAILED;
		}
	}

	if (vkfwZxdgDecorationManagerV1) {
		w->decoration_v1 = zxdg_decoration_manager_v1_get_toplevel_decoration (
			vkfwZxdgDecorationManagerV1, w->xdg_toplevel);
		if (!w->decoration_v1) {
			if (vkfwWlSupportCSD) {
				wp_viewport_destroy (w->frame_viewport);
				w->frame_viewport = nullptr;
			}
			xdg_toplevel_destroy (w->xdg_toplevel);
			w->xdg_toplevel = nullptr;
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		zxdg_toplevel_decoration_v1_add_listener (w->decoration_v1,
			&toplevel_decoration_v1_listener, w);

		zxdg_toplevel_decoration_v1_set_mode (w->decoration_v1,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	if (w->title)
		xdg_toplevel_set_title (w->xdg_toplevel, w->title);

	w->visible = true;
	wl_surface_commit (w->content_surface);
	if (vkfwWlSupportCSD)
			wl_surface_commit (w->frame_surface);
	wl_display_roundtrip (vkfwWlDisplay);
	return VK_SUCCESS;
}

VkResult
vkfwWlHideWindow (VKFWwindow *window)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	if (!w->visible)
		return VK_SUCCESS;

	if (w->decoration_v1) {
		zxdg_toplevel_decoration_v1_destroy (w->decoration_v1);
		w->decoration_v1 = nullptr;
	}

	if (vkfwWlSupportCSD) {
		wp_viewport_destroy (w->frame_viewport);
		w->frame_viewport = nullptr;
	}

	xdg_toplevel_destroy (w->xdg_toplevel);
	w->xdg_toplevel = nullptr;

	if (vkfwWlSupportCSD) {
		if (w->has_csd) {
			wl_surface_attach (w->frame_surface, nullptr, 0, 0);
			wl_subsurface_set_position (w->content_subsurface, 0, 0);
			w->has_csd = false;
			w->has_csd_buffer_attached = false;
		}
		wl_surface_commit (w->content_surface);
		wl_surface_commit (w->frame_surface);
	} else
		wl_surface_commit (w->content_surface);

	w->visible = false;
	wl_display_flush (vkfwWlDisplay);
	return VK_SUCCESS;
}

VkResult
vkfwWlSetWindowTitle (VKFWwindow *window, const char *title)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	char *s = strdup (title);
	if (!s)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	if (w->title)
		free (w->title);
	w->title = s;

	if (w->xdg_toplevel) {
		xdg_toplevel_set_title (w->xdg_toplevel, w->title);
		if (vkfwWlSupportCSD && 0)
			wl_surface_commit (w->frame_surface);
	}

	return VK_SUCCESS;
}
