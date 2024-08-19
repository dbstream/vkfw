/**
 * XCB event handling
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <sys/poll.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include "event.h"
#include "window.h"
#include "xcb.h"

static void
handle_key_press (VKFWevent *e, xcb_key_press_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_KEY_PRESSED;
	e->x = xe->event_x;
	e->y = xe->event_y;
	e->keycode = xe->detail;
}

static void
handle_key_release (VKFWevent *e, xcb_key_release_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_KEY_RELEASED;
	e->x = xe->event_x;
	e->y = xe->event_y;
	e->keycode = xe->detail;
}

/**
 * X11 generates scrolling as a button press followed by a button release.
 */

static void
handle_button_press (VKFWevent *e, xcb_button_press_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_BUTTON_PRESSED;
	e->window = (VKFWwindow *) window;
	e->x = xe->event_x;
	e->y = xe->event_y;

	if (xe->detail >= 4 && xe->detail <= 7) {
		e->type = VKFW_EVENT_SCROLL;
		e->scroll_direction = (xe->detail >= 6)
			? VKFW_SCROLL_HORIZONTAL : VKFW_SCROLL_VERTICAL;
		e->scroll_value = (xe->detail & 1) ? 1 : -1;
	} else switch (xe->detail) {
	case 1:
		e->button = VKFW_LEFT_MOUSE_BUTTON;
		break;
	case 2:
		e->button = VKFW_SCROLL_WHEEL_BUTTON;
		break;
	case 3:
		e->button = VKFW_RIGHT_MOUSE_BUTTON;
		break;
	default:
		e->button = xe->detail - 4;
	}
}

static void
handle_button_release (VKFWevent *e, xcb_button_release_event_t *xe)
{
	if (xe->detail >= 4 && xe->detail <= 7)
		return;

	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_BUTTON_RELEASED;
	e->window = (VKFWwindow *) window;
	e->x = xe->event_x;
	e->y = xe->event_y;

	switch (xe->detail) {
	case 1:
		e->button = VKFW_LEFT_MOUSE_BUTTON;
		break;
	case 2:
		e->button = VKFW_SCROLL_WHEEL_BUTTON;
		break;
	case 3:
		e->button = VKFW_RIGHT_MOUSE_BUTTON;
		break;
	default:
		e->button = xe->detail - 4;
	}
}

static void
handle_motion_notify (VKFWevent *e, xcb_motion_notify_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_POINTER_MOTION;
	e->window = (VKFWwindow *) window;
	e->x = xe->event_x;
	e->y = xe->event_y;
}

static void
handle_focus_in (VKFWevent *e, xcb_focus_in_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_WINDOW_GAINED_FOCUS;
	e->window = (VKFWwindow *) window;
}

static void
handle_focus_out (VKFWevent *e, xcb_focus_out_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->event);
	if (!window)
		return;

	e->type = VKFW_EVENT_WINDOW_LOST_FOCUS;
	e->window = (VKFWwindow *) window;
}

static void
handle_map_notify (VKFWevent *e, xcb_map_notify_event_t *xe)
{
	(void) e;
	(void) xe;
}

static void
handle_reparent_notify (VKFWevent *e, xcb_reparent_notify_event_t *xe)
{
	(void) e;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: wid=%" PRIu32 " was reparented to wid=%" PRIu32 "\n",
		xe->window, xe->parent);

	/**
	 * Keep track of a window's true parent, as it may be useful.
	 */

	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->window);
	if (!window)
		return;

	window->parent = xe->parent;
}

static void
handle_configure_notify (VKFWevent *e, xcb_configure_notify_event_t *xe)
{
	VKFWxcbwindow *window = vkfwXcbXIDToWindow (xe->window);
	if (!window)
		return;

	e->type = VKFW_EVENT_WINDOW_RESIZE_NOTIFY;
	e->window = (VKFWwindow *) window;
	e->extent.width = xe->width;
	e->extent.height = xe->height;
}

static void
handle_wm_protocols_message (VKFWevent *e, xcb_client_message_event_t *xe)
{
	uint32_t msg = xe->data.data32[0];
	if (!msg)
		return;

	if (msg == vkfw__NET_WM_PING) {
		/**
		 * _NET_WM_PING: the window manager sends this event to clients
		 * to test if they are responsive. Clients must send a response
		 * to the root window with event->window set to the root window.
		 */
		xe->window = vkfw_xcb_default_screen->root;
		xcb_void_cookie_t cookie = xcb_send_event (vkfw_xcb_connection, 0,
			vkfw_xcb_default_screen->root,
			XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
			| XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *) xe);
		if (xcb_request_check (vkfw_xcb_connection, cookie))
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: got an error when responding to _NET_WM_PING\n");
	} else if (msg == vkfw_WM_DELETE_WINDOW) {
		/**
		 * WM_DELETE_WINDOW: the window manager sends this event to
		 * clients when window deletion has been requested, for example
		 * by means of the user clicking the close button on the window
		 * decorations.
		 *
		 * Clients are not required to respond in any particular way.
		 */
		e->type = VKFW_EVENT_WINDOW_CLOSE_REQUEST;
		e->window = (VKFWwindow *) vkfwXcbXIDToWindow (xe->window);
	} else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: unknown WM_PROTOCOLS message %" PRIu32 "\n", msg);
}

static void
handle_client_message (VKFWevent *e, xcb_client_message_event_t *xe)
{
	if (!xe->type)
		return;

	/**
	 * Client messages are not sent by the X11 server, rather other clients
	 * (such as the window manager).
	 */

	if (xe->type == vkfw_WM_PROTOCOLS)
		handle_wm_protocols_message (e, xe);
}

static void
handle_generic_event (VKFWevent *e, xcb_ge_generic_event_t *xe)
{
	/**
	 * TODO: what is this? Do we need to handle it in any way?
	 */
	(void) e;
	(void) xe;
}

static void
handle_event (VKFWevent *e, xcb_generic_event_t *xe)
{
	e->type = VKFW_EVENT_NULL;

	switch (xe->response_type & 0x7f) {
	case XCB_KEY_PRESS:
		handle_key_press (e, (xcb_key_press_event_t *) xe);
		break;
	case XCB_KEY_RELEASE:
		handle_key_release (e, (xcb_key_release_event_t *) xe);
		break;
	case XCB_BUTTON_PRESS:
		handle_button_press (e, (xcb_button_press_event_t *) xe);
		break;
	case XCB_BUTTON_RELEASE:
		handle_button_release (e, (xcb_button_release_event_t *) xe);
		break;
	case XCB_MOTION_NOTIFY:
		handle_motion_notify (e, (xcb_motion_notify_event_t *) xe);
		break;
	case XCB_FOCUS_IN:
		handle_focus_in (e, (xcb_focus_in_event_t *) xe);
		break;
	case XCB_FOCUS_OUT:
		handle_focus_out (e, (xcb_focus_out_event_t *) xe);
		break;
	case XCB_MAP_NOTIFY:
		handle_map_notify (e, (xcb_map_notify_event_t *) xe);
		break;
	case XCB_REPARENT_NOTIFY:
		handle_reparent_notify (e, (xcb_reparent_notify_event_t *) xe);
		break;
	case XCB_CONFIGURE_NOTIFY:
		handle_configure_notify (e, (xcb_configure_notify_event_t *) xe);
		break;
	case XCB_CLIENT_MESSAGE:
		handle_client_message (e, (xcb_client_message_event_t *) xe);
		break;
	case XCB_GE_GENERIC:
		handle_generic_event (e, (xcb_ge_generic_event_t *) xe);
		break;
	default:
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: unhandled event type %u\n", xe->response_type);
	}

	free (xe);
}

VkResult
vkfwXcbGetEvent (VKFWevent *e, int mode, uint64_t timeout)
{
	xcb_generic_event_t *xe;

	/**
	 * Indefinite timeout: fall back to xcb_wait_for_event.
	 */
	if (timeout == UINT64_MAX) {
		xe = xcb_wait_for_event (vkfw_xcb_connection);

		if (xe)
			handle_event (e, xe);
		else if (xcb_connection_has_error (vkfw_xcb_connection))
			return VK_ERROR_SURFACE_LOST_KHR;

		return VK_SUCCESS;
	}

	/**
	 * Non-zero timeout: poll the XCB file descriptor.
	 */
	if (timeout) {
		/**
		 * XCB keeps an internal event queue that poll cannot know
		 * about. Look at this queue before waiting on the fd, as
		 * otherwise it is possible to sleep while there are events
		 * available to process.
		 */
		xe = xcb_poll_for_queued_event (vkfw_xcb_connection);
		if (xe) {
			handle_event (e, xe);
			return VK_SUCCESS;
		}

		struct pollfd fds[] = {
			{ xcb_get_file_descriptor (vkfw_xcb_connection), POLLIN, 0 }
		};

		/**
		 * If we are using deadline mode, we would really want a poll(2)
		 * with an absolute expiry time instead of a timeout. To the
		 * best of my knowledge, there exists no convenient way to do
		 * this currently (epoll?).
		 */
		if (mode == VKFW_EVENT_MODE_DEADLINE) {
			uint64_t now = vkfwGetTime ();
			if (timeout > now)
				timeout -= now;
			else
				timeout = 0;
		}

		timeout = (timeout + 999) / 1000;
		poll (fds, 1, timeout);
	}

	xe = xcb_poll_for_event (vkfw_xcb_connection);
	if (xe)
		handle_event (e, xe);
	else if (xcb_connection_has_error (vkfw_xcb_connection))
		return VK_ERROR_SURFACE_LOST_KHR;

	return VK_SUCCESS;
}
