/**
 * Wayland input handling.
 * Copyright (C) 2024  dbstream
 *
 * Currently we support one pointer device and one keyboard device. These
 * usually correspond to a virtual pointer and virtual keyboard anyways, so
 * it does not matter. In the future, we may decide to support more.
 */
#include <VKFW/event.h>
#include <VKFW/logging.h>
#include <VKFW/window.h>
#include <unistd.h>
#include "input.h"
#include "wayland.h"
#include "window.h"

#include <linux/input-event-codes.h>

static wl_seat *seat;
static wl_pointer *ptr_dev;
static wl_keyboard *kbd_dev;

static wl_surface *cursor_surface;

static VKFWwlwindow *ptr_focus_window;
static wl_surface *ptr_focus;
static int ptr_x, ptr_y;

static VKFWwlwindow *kbd_focus_window;
static wl_surface *kbd_focus;

static void
ptr_defocus (void)
{
	if (ptr_focus_window) {
		vkfwUnrefWindow ((VKFWwindow *) ptr_focus_window);
		ptr_focus_window = nullptr;
	}
}

static void
kbd_defocus (void)
{
	if (kbd_focus_window) {
		VKFWevent e {};
		e.type = VKFW_EVENT_WINDOW_LOST_FOCUS;
		e.window = (VKFWwindow *) kbd_focus_window;
		vkfwSendEventToApplication (&e);

		vkfwUnrefWindow ((VKFWwindow *) kbd_focus_window);
		kbd_focus_window = nullptr;
	}
}

static void
handle_ptr_enter (void *data, wl_pointer *dev, uint32_t serial,
	wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
	ptr_focus_window = (VKFWwlwindow *) wl_surface_get_user_data (surface);
	if (ptr_focus_window) {
		vkfwRefWindow ((VKFWwindow *) ptr_focus_window);
		ptr_focus = surface;
		ptr_x = wl_fixed_to_int (x);
		ptr_y = wl_fixed_to_int (y);

		wl_pointer_set_cursor (dev, serial, cursor_surface, 3, 2);
	}

	if (ptr_focus_window && ptr_focus == ptr_focus_window->content_surface) {
		VKFWevent e {};
		e.type = VKFW_EVENT_POINTER_MOTION;
		e.window = (VKFWwindow *) ptr_focus_window;
		e.x = ptr_x;
		e.y = ptr_y;
		vkfwSendEventToApplication (&e);
	}
}

static void
handle_ptr_leave (void *data, wl_pointer *dev, uint32_t serial,
	wl_surface *surface)
{
	ptr_defocus ();
}

static void
handle_ptr_motion (void *data, wl_pointer *dev, uint32_t time,
	wl_fixed_t x, wl_fixed_t y)
{
	ptr_x = wl_fixed_to_int (x);
	ptr_y = wl_fixed_to_int (y);

	if (ptr_focus_window && ptr_focus == ptr_focus_window->content_surface) {
		VKFWevent e {};
		e.type = VKFW_EVENT_POINTER_MOTION;
		e.window = (VKFWwindow *) ptr_focus_window;
		e.x = ptr_x;
		e.y = ptr_y;
		vkfwSendEventToApplication (&e);
	}
}

static void
handle_ptr_button (void *data, wl_pointer *dev, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state)
{
	if (!ptr_focus_window)
		return;

	// Implement CSD menu and move/resize operations.
	if (ptr_focus_window->has_csd && ptr_focus == ptr_focus_window->frame_surface) {
		if (state != WL_POINTER_BUTTON_STATE_PRESSED)
			return;

		if (button == BTN_RIGHT) {
			xdg_toplevel_show_window_menu (ptr_focus_window->xdg_toplevel,
				seat, serial, ptr_x, ptr_y);
			return;
		} else if (button != BTN_LEFT)
			return;

		bool top = false, bottom = false, left = false, right = false;
		if (ptr_y < 5)
			top = true;
		else if (ptr_y >= ptr_focus_window->configured_height - 5)
			bottom = true;
		if (ptr_x < 5)
			left = true;
		else if (ptr_x >= ptr_focus_window->configured_width - 5)
			right = true;

		uint32_t resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
		if (top) {
			if (left)
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
			else if (right)
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
			else
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
		} else if (bottom) {
			if (left)
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
			else if (right)
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
			else
				resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
		} else if (left)
			resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
		else if (right)
			resize_edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;

		if (resize_edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
			xdg_toplevel_move (ptr_focus_window->xdg_toplevel,
				seat, serial);
		else
			xdg_toplevel_resize (ptr_focus_window->xdg_toplevel,
				seat, serial, resize_edge);

		return;
	}

	// Implement CSD close button.
	if (ptr_focus_window->has_csd_decorations && ptr_focus == ptr_focus_window->close_button_surface) {
		if (button != BTN_LEFT)
			return;

		if (state != WL_POINTER_BUTTON_STATE_RELEASED)
			return;

		VKFWevent e {};
		e.type = VKFW_EVENT_WINDOW_CLOSE_REQUEST;
		e.window = (VKFWwindow *) ptr_focus_window;
		vkfwSendEventToApplication (&e);
		return;
	}
}

static void
handle_ptr_axis (void *data, wl_pointer *dev, uint32_t time,
	uint32_t axis, wl_fixed_t value)
{

}

static void
handle_ptr_frame (void *data, wl_pointer *dev)
{

}

static void
handle_ptr_axis_source (void *data, wl_pointer *dev, uint32_t source)
{

}

static void
handle_ptr_axis_stop (void *data, wl_pointer *dev, uint32_t time,
	uint32_t axis)
{

}

static void
handle_ptr_axis_discrete (void *data, wl_pointer *dev, uint32_t axis,
	int32_t discrete)
{

}

static void
handle_ptr_axis_value120 (void *data, wl_pointer *dev, uint32_t axis,
	int32_t value120)
{

}

static void
handle_ptr_axis_relative_direction (void *data, wl_pointer *dev, uint32_t axis,
	uint32_t direction)
{

}

static const struct wl_pointer_listener pointer_listener = {
	.enter = handle_ptr_enter,
	.leave = handle_ptr_leave,
	.motion = handle_ptr_motion,
	.button = handle_ptr_button,
	.axis = handle_ptr_axis,
	.frame = handle_ptr_frame,
	.axis_source = handle_ptr_axis_source,
	.axis_stop = handle_ptr_axis_stop,
	.axis_discrete = handle_ptr_axis_discrete,
	.axis_value120 = handle_ptr_axis_value120,
	.axis_relative_direction = handle_ptr_axis_relative_direction
};

static void
handle_kbd_keymap (void *data, wl_keyboard *dev, uint32_t format,
	int32_t fd_, uint32_t size)
{
	int fd = fd_;

	close (fd);
}

static void
handle_kbd_enter (void *data, wl_keyboard *dev, uint32_t serial,
	wl_surface *surface, wl_array *keys)
{

}

static void
handle_kbd_leave (void *data, wl_keyboard *dev, uint32_t serial,
	wl_surface *surface)
{
	kbd_defocus ();
}

static void
handle_kbd_key (void *data, wl_keyboard *dev, uint32_t serial,
	uint32_t key, uint32_t time, uint32_t state)
{

}

static void
handle_kbd_modifiers (void *data, wl_keyboard *dev, uint32_t serial,
	uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
	uint32_t group)
{

}

static void
handle_kbd_repeat_info (void *data, wl_keyboard *dev, int32_t rate,
	int32_t delay)
{

}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = handle_kbd_keymap,
	.enter = handle_kbd_enter,
	.leave = handle_kbd_leave,
	.key = handle_kbd_key,
	.modifiers = handle_kbd_modifiers,
	.repeat_info = handle_kbd_repeat_info
};

static void
handle_wl_seat_capabilites (void *data, wl_seat *seat, uint32_t cap)
{
	(void) data;
	(void) seat;

	if (cap & WL_SEAT_CAPABILITY_POINTER) {
		if (!ptr_dev) {
			ptr_dev = wl_seat_get_pointer (seat);
			if (ptr_dev)
				wl_pointer_add_listener (ptr_dev, &pointer_listener, nullptr);
			else
				vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create pointer device\n");
		}
	} else {
		if (ptr_dev) {
			wl_pointer_destroy (ptr_dev);
			ptr_dev = nullptr;

			ptr_defocus ();
		}
	}

	if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
		if (!kbd_dev) {
			kbd_dev = wl_seat_get_keyboard (seat);
			if (kbd_dev)
				wl_keyboard_add_listener (kbd_dev, &keyboard_listener, nullptr);
			else
				vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create keyboard device\n");
		}
	} else {
		if (kbd_dev) {
			wl_keyboard_destroy (kbd_dev);
			kbd_dev = nullptr;

			kbd_defocus ();
		}
	}
}

static void
handle_wl_seat_name (void *data, wl_seat *seat, const char *name)
{
	(void) data;
	(void) seat;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: input method is \"%s\"\n", name);
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = handle_wl_seat_capabilites,
	.name = handle_wl_seat_name
};

VkResult
vkfwWlInitializeInput (uint32_t seat_id)
{
	if (!seat_id) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: no wl_seat available!\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	cursor_surface = wl_compositor_create_surface (vkfwWlCompositor);
	if (!cursor_surface)
		return VK_ERROR_INITIALIZATION_FAILED;

	wl_surface_attach (cursor_surface, vkfwWlCursorBuffer, 0, 0);
	wl_surface_commit (cursor_surface);

	seat = (wl_seat *) wl_registry_bind (vkfwWlRegistry, seat_id, &wl_seat_interface, 7);
	if (!seat) {
		wl_surface_destroy (cursor_surface);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	wl_seat_add_listener (seat, &seat_listener, nullptr);
	wl_display_roundtrip (vkfwWlDisplay);

	return VK_SUCCESS;
}

void
vkfwWlTerminateInput (void)
{
	if (ptr_dev) {
		wl_pointer_destroy (ptr_dev);
		ptr_defocus ();
	}
	if (kbd_dev) {
		wl_keyboard_destroy (kbd_dev);
		kbd_defocus ();
	}
	wl_seat_destroy (seat);
}
