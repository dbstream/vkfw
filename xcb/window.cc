/**
 * Xcb window functions
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_XCB_KHR
#include <VKFW/logging.h>
#include <VKFW/vector.h>
#include <VKFW/vkfw.h>
#include <VKFW/window.h>
#include <inttypes.h>
#include <stdlib.h>
#include <xcb/xcb_atom.h>
#include <mutex>
#include "window.h"
#include "xcb.h"

struct wid_window_pair {
	xcb_window_t id;
	VKFWxcbwindow *window;
};

static VKFWvector<wid_window_pair> wid_to_window_map;
static std::mutex wid_to_window_mu;

static bool
register_window_wid (xcb_window_t id, VKFWxcbwindow *window)
{
	std::scoped_lock g (wid_to_window_mu);
	return wid_to_window_map.push_back ({id, window});
}

static void
unregister_window_wid (xcb_window_t id)
{
	std::scoped_lock g (wid_to_window_mu);
	size_t n = wid_to_window_map.size () - 1;
	for (size_t i = 0; i <= n; i++) {
		if (wid_to_window_map[i].id == id) {
			if (i < n)
				wid_to_window_map[i] = wid_to_window_map[n];
			wid_to_window_map.pop_back ();
			return;
		}
	}
}

VKFWxcbwindow *
vkfwXcbXIDToWindow (xcb_window_t wid)
{
	std::scoped_lock g (wid_to_window_mu);
	for (const wid_window_pair &p : wid_to_window_map)
		if (p.id == wid)
			return p.window;
	return nullptr;
}

VKFWwindow *
vkfwXcbAllocWindow (void)
{
	return (VKFWwindow *) malloc (sizeof (VKFWxcbwindow));
}

VkResult
vkfwXcbCreateWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	w->parent = vkfw_xcb_default_screen->root;
	w->pointer_mode = 0;
	w->warp_x = -1;
	w->warp_y = -1;
	w->last_x = 0;
	w->last_y = 0;
	w->wid = xcb_generate_id (vkfw_xcb_connection);
	if (w->wid == -1)
		return VK_ERROR_INITIALIZATION_FAILED;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: allocated XID=%" PRIu32 " for an application window\n", w->wid);

	if (!register_window_wid (w->wid, w))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	uint32_t cw_mask = XCB_CW_EVENT_MASK;
	uint32_t cw_values[] = {
		XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
		| XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY
		| XCB_EVENT_MASK_FOCUS_CHANGE
	};

	xcb_void_cookie_t cookie = xcb_create_window_checked (vkfw_xcb_connection,
		XCB_COPY_FROM_PARENT, w->wid, w->parent,
		0, 0, handle->extent.width, handle->extent.height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		vkfw_xcb_default_screen->root_visual, cw_mask, cw_values);

	if (vkfwXcbCheck (cookie)) {
		unregister_window_wid (w->wid);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	xcb_atom_t protocols[2];
	uint32_t num_protocols = 0;

	if (vkfw__NET_WM_PING)
		protocols[num_protocols++] = vkfw__NET_WM_PING;
	if (vkfw_WM_DELETE_WINDOW)
		protocols[num_protocols++] = vkfw_WM_DELETE_WINDOW;

	if (vkfw_WM_PROTOCOLS) {
		xcb_void_cookie_t protocols_cookie = xcb_change_property_checked (
			vkfw_xcb_connection, XCB_PROP_MODE_REPLACE, w->wid,
			vkfw_WM_PROTOCOLS, XCB_ATOM_ATOM, 32,
			num_protocols, protocols);
		if (vkfwXcbCheck (protocols_cookie)) {
			unregister_window_wid (w->wid);
			xcb_destroy_window (vkfw_xcb_connection, w->wid);
			return VK_ERROR_INITIALIZATION_FAILED;
		}
	}

	return VK_SUCCESS;
}

VKFWxcbwindow *vkfw_xcb_focus_window;

void
vkfwXcbDestroyWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	if (w == vkfw_xcb_focus_window)
		vkfw_xcb_focus_window = nullptr;
	xcb_destroy_window (vkfw_xcb_connection, w->wid);
	xcb_flush (vkfw_xcb_connection);
	unregister_window_wid (w->wid);
}

VkResult
vkfwXcbCreateWindowSurface (VKFWwindow *handle, VkSurfaceKHR *out)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	VkXcbSurfaceCreateInfoKHR ci {};
	ci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	ci.connection = vkfw_xcb_connection;
	ci.window = w->wid;
	return vkCreateXcbSurfaceKHR (vkfwLoadedInstance, &ci, nullptr, out);
}

VkResult
vkfwXcbShowWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	xcb_void_cookie_t cookie = xcb_map_window_checked (vkfw_xcb_connection, w->wid);
	if (vkfwXcbCheck (cookie))
		return VK_ERROR_UNKNOWN;
	return VK_SUCCESS;
}

VkResult
vkfwXcbHideWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	xcb_void_cookie_t cookie = xcb_unmap_window_checked (vkfw_xcb_connection, w->wid);
	if (vkfwXcbCheck (cookie))
		return VK_ERROR_UNKNOWN;
	return VK_SUCCESS;
}

VkResult
vkfwXcbSetWindowTitle (VKFWwindow *handle, const char *title)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	uint32_t n = strlen (title);

	xcb_void_cookie_t cookie1 = xcb_change_property_checked (vkfw_xcb_connection,
		XCB_PROP_MODE_REPLACE, w->wid, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, n, title);

	xcb_void_cookie_t cookie2 = xcb_change_property_checked (vkfw_xcb_connection,
		XCB_PROP_MODE_REPLACE, w->wid, XCB_ATOM_WM_ICON_NAME,
		XCB_ATOM_STRING, 8, n, title);

	bool failed = false;
	failed |= vkfwXcbCheck (cookie1);
	failed |= vkfwXcbCheck (cookie2);
	if (failed)
		return VK_ERROR_UNKNOWN;

	return VK_SUCCESS;
}

void
vkfwXcbUpdatePointerMode (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;

	uint32_t f = handle->pointer_flags;
	uint32_t m = w->pointer_mode;

	/**
	 * CONFINED implies GRABBED.
	 */
	if (f & VKFW_POINTER_CONFINED)
		f |= VKFW_POINTER_GRABBED;

	if ((f & VKFW_POINTER_HIDDEN) && !(m & VKFW_POINTER_HIDDEN)) {
		uint32_t cw_values[] = {
			vkfw_xcb_cursors[VKFW_XCB_CURSOR_HIDDEN]
		};
		xcb_change_window_attributes (vkfw_xcb_connection,
			w->wid, XCB_CW_CURSOR, cw_values);
	} else if (!(f & VKFW_POINTER_HIDDEN) && (m & VKFW_POINTER_HIDDEN)) {
		uint32_t cw_values[] = {
			vkfw_xcb_cursors[VKFW_XCB_CURSOR_NORMAL]
		};
		xcb_change_window_attributes (vkfw_xcb_connection,
			w->wid, XCB_CW_CURSOR, cw_values);
	}

	uint32_t grab_mask = VKFW_POINTER_CONFINED | VKFW_POINTER_GRABBED;

	if ((f & grab_mask) && ((f & grab_mask) != (m & grab_mask))) {
		xcb_window_t confine_window = XCB_WINDOW_NONE;
		if (f & VKFW_POINTER_CONFINED)
			confine_window = w->wid;

		xcb_grab_pointer_unchecked (vkfw_xcb_connection, 1, w->wid,
			0, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
			confine_window, XCB_CURSOR_NONE, XCB_CURRENT_TIME);
	} else if (!(f & grab_mask) && (m & grab_mask)) {
		xcb_ungrab_pointer (vkfw_xcb_connection, XCB_CURRENT_TIME);
	}

	if ((f & VKFW_POINTER_RELATIVE) && w->warp_x == -1 && w->warp_y == -1) {
		w->warp_x = w->window.extent.width / 2;
		w->warp_y = w->window.extent.height / 2;
		xcb_warp_pointer (vkfw_xcb_connection, w->wid, w->wid, 0, 0,
			w->window.extent.width, w->window.extent.height,
			w->warp_x, w->warp_y);
	}

	xcb_flush (vkfw_xcb_connection);
	w->pointer_mode = f;
}
