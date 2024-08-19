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

	xcb_void_cookie_t cookie = xcb_create_window (vkfw_xcb_connection,
		XCB_COPY_FROM_PARENT, w->wid, w->parent,
		0, 0, handle->extent.width, handle->extent.height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		vkfw_xcb_default_screen->root_visual, cw_mask, cw_values);

	if (xcb_request_check (vkfw_xcb_connection, cookie)) {
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
		xcb_void_cookie_t protocols_cookie = xcb_change_property (
			vkfw_xcb_connection, XCB_PROP_MODE_REPLACE, w->wid,
			vkfw_WM_PROTOCOLS, XCB_ATOM_ATOM, 32,
			num_protocols, protocols);
		if (xcb_request_check (vkfw_xcb_connection, protocols_cookie)) {
			unregister_window_wid (w->wid);
			xcb_request_check (vkfw_xcb_connection,
				xcb_destroy_window (vkfw_xcb_connection, w->wid));
			return VK_ERROR_INITIALIZATION_FAILED;
		}
	}

	return VK_SUCCESS;
}

void
vkfwXcbDestroyWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	xcb_void_cookie_t cookie = xcb_destroy_window (vkfw_xcb_connection, w->wid);
	if (xcb_request_check (vkfw_xcb_connection, cookie))
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: error in xcb_destroy_window\n");

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
	xcb_void_cookie_t cookie = xcb_map_window (vkfw_xcb_connection, w->wid);
	if (xcb_request_check (vkfw_xcb_connection, cookie))
		return VK_ERROR_UNKNOWN;
	return VK_SUCCESS;
}

VkResult
vkfwXcbHideWindow (VKFWwindow *handle)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	xcb_void_cookie_t cookie = xcb_unmap_window (vkfw_xcb_connection, w->wid);
	if (xcb_request_check (vkfw_xcb_connection, cookie))
		return VK_ERROR_UNKNOWN;
	return VK_SUCCESS;
}

VkResult
vkfwXcbSetWindowTitle (VKFWwindow *handle, const char *title)
{
	VKFWxcbwindow *w = (VKFWxcbwindow *) handle;
	uint32_t n = strlen (title);

	xcb_void_cookie_t cookie1 = xcb_change_property (vkfw_xcb_connection,
		XCB_PROP_MODE_REPLACE, w->wid, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, n, title);

	xcb_void_cookie_t cookie2 = xcb_change_property (vkfw_xcb_connection,
		XCB_PROP_MODE_REPLACE, w->wid, XCB_ATOM_WM_ICON_NAME,
		XCB_ATOM_STRING, 8, n, title);

	bool failed = false;
	failed |= xcb_request_check (vkfw_xcb_connection, cookie1) ? true : false;
	failed |= xcb_request_check (vkfw_xcb_connection, cookie2) ? true : false;
	if (failed)
		return VK_ERROR_UNKNOWN;

	return VK_SUCCESS;
}
