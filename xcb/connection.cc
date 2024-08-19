/**
 * X11/XCB initialization.
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_XCB_KHR 1
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "event.h"
#include "keyboard.h"
#include "window.h"
#include "xcb.h"

static void *libxcb_handle;

static void
unload_xcb_funcs (void)
{
	vkfwCurrentPlatform->unloadModule (libxcb_handle);
}

/**
 *   Whatever you do, keep this list in sync with #include "xcb.h", you
 *   fool, you moron.
 *
 * ~david
 */

PFNxcb_connect xcb_connect;
PFNxcb_disconnect xcb_disconnect;
PFNxcb_get_setup xcb_get_setup;
PFNxcb_setup_roots_iterator xcb_setup_roots_iterator;
PFNxcb_screen_next xcb_screen_next;
PFNxcb_generate_id xcb_generate_id;
PFNxcb_request_check xcb_request_check;
PFNxcb_create_window xcb_create_window;
PFNxcb_destroy_window xcb_destroy_window;
PFNxcb_map_window xcb_map_window;
PFNxcb_unmap_window xcb_unmap_window;
PFNxcb_wait_for_event xcb_wait_for_event;
PFNxcb_poll_for_event xcb_poll_for_event;
PFNxcb_poll_for_queued_event xcb_poll_for_queued_event;
PFNxcb_get_file_descriptor xcb_get_file_descriptor;
PFNxcb_flush xcb_flush;
PFNxcb_connection_has_error xcb_connection_has_error;
PFNxcb_change_property xcb_change_property;
PFNxcb_intern_atom xcb_intern_atom;
PFNxcb_intern_atom_reply xcb_intern_atom_reply;
PFNxcb_send_event xcb_send_event;

static bool
load_xcb_funcs (void)
{
	libxcb_handle = vkfwCurrentPlatform->loadModule ("libxcb.so.1");
	if (!libxcb_handle)
		libxcb_handle = vkfwCurrentPlatform->loadModule ("libxcb.so");
	if (!libxcb_handle)
		return false;

	bool failed = false;

#define L(sym)										\
	sym = (PFN##sym) vkfwCurrentPlatform->lookupSymbol(libxcb_handle, #sym);	\
	if (!sym)									\
		failed = true;
	L(xcb_connect)
	L(xcb_disconnect)
	L(xcb_get_setup)
	L(xcb_setup_roots_iterator)
	L(xcb_screen_next)
	L(xcb_generate_id)
	L(xcb_request_check)
	L(xcb_create_window)
	L(xcb_destroy_window)
	L(xcb_map_window)
	L(xcb_unmap_window)
	L(xcb_wait_for_event)
	L(xcb_poll_for_event)
	L(xcb_poll_for_queued_event)
	L(xcb_get_file_descriptor)
	L(xcb_flush)
	L(xcb_connection_has_error)
	L(xcb_change_property)
	L(xcb_intern_atom)
	L(xcb_intern_atom_reply)
	L(xcb_send_event)
#undef L

	if (failed) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to load some symbols\n");
		unload_xcb_funcs ();
		return false;
	}

	return true;
}

xcb_connection_t *vkfw_xcb_connection;
xcb_screen_t *vkfw_xcb_default_screen;

#define VKFW_DECLARE_ATOM(name) xcb_atom_t vkfw_##name;
VKFW_XCB_ALL_ATOMS(VKFW_DECLARE_ATOM)
#undef VKFW_DECLARE_ATOM

static bool
load_atoms (void)
{
#define VKFW_INTERN_ATOM(name)					\
	xcb_intern_atom_cookie_t c_##name = xcb_intern_atom (	\
		vkfw_xcb_connection, 1, strlen(#name), #name);
VKFW_XCB_ALL_ATOMS(VKFW_INTERN_ATOM)
#undef VKFW_INTERN_ATOM

	bool failed = false;
	xcb_intern_atom_reply_t *r;
	xcb_generic_error_t *e = nullptr;

#define VKFW_INTERN_ATOM(name)						\
	r = xcb_intern_atom_reply (vkfw_xcb_connection, c_##name, &e);	\
	if (e)								\
		failed = true;						\
	if (r) {							\
		vkfw_##name = r->atom;					\
		free (r);						\
	}
VKFW_XCB_ALL_ATOMS(VKFW_INTERN_ATOM)
#undef VKFW_INTERN_ATOM
#define VKFW_INTERN_ATOM(name)						\
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: %s=%" PRIu32 "\n", #name, vkfw_##name);
VKFW_XCB_ALL_ATOMS(VKFW_INTERN_ATOM)
#undef VKFW_INTERN_ATOM
	return !failed;
}

static VkResult
vkfwXcbOpen (void)
{
	if (!load_xcb_funcs ())
		return VK_ERROR_INITIALIZATION_FAILED;

	int i = 0;
	vkfw_xcb_connection = xcb_connect (nullptr, &i);
	if (!vkfw_xcb_connection) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to open connection\n");
		unload_xcb_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (xcb_connection_has_error (vkfw_xcb_connection)) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to open connection\n");
		unload_xcb_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: using Xcb backend\n");

	if (!load_atoms ()) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to load atoms\n");
		xcb_disconnect (vkfw_xcb_connection);
		unload_xcb_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	const xcb_setup_t *setup = xcb_get_setup (vkfw_xcb_connection);
	xcb_screen_iterator_t it = xcb_setup_roots_iterator (setup);

	for (int j = 0; j < i; j++)
		xcb_screen_next (&it);

	vkfw_xcb_default_screen = it.data;
	xcb_screen_t *s = it.data;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: default screen %" PRIu16 "x% " PRIu16 " white=%08" PRIx32 " black=%08" PRIx32 "\n",
		s->width_in_pixels,
		s->height_in_pixels,
		s->white_pixel,
		s->black_pixel);

	return VK_SUCCESS;
}

static void
vkfwXcbClose (void)
{
	xcb_disconnect (vkfw_xcb_connection);
	unload_xcb_funcs ();
}

static VkResult
vkfwXcbRequestExtensions (void)
{
	VkResult result = VK_SUCCESS;
	VkResult result2;
#define R(name)							\
	result2 = vkfwRequestInstanceExtension (name, true);	\
	if (result2 < result || (result2 && !result))		\
		result = result2;
	R (VK_KHR_SURFACE_EXTENSION_NAME);
	R (VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#undef R
	return result;
}

static VkResult
vkfwXcbQueryPresentSupport (VkPhysicalDevice device, uint32_t queue,
	VkBool32 *out)
{
	*out = vkGetPhysicalDeviceXcbPresentationSupportKHR (device, queue,
		vkfw_xcb_connection, vkfw_xcb_default_screen->root_visual);
	return VK_SUCCESS;
}

VKFWwindowbackend vkfwBackendXcb = {
	.open_connection = vkfwXcbOpen,
	.close_connection = vkfwXcbClose,
	.request_instance_extensions = vkfwXcbRequestExtensions,
	.alloc_window = vkfwXcbAllocWindow,
	.create_window = vkfwXcbCreateWindow,
	.destroy_window = vkfwXcbDestroyWindow,
	.create_surface = vkfwXcbCreateWindowSurface,
	.query_present_support = vkfwXcbQueryPresentSupport,
	.show_window = vkfwXcbShowWindow,
	.hide_window = vkfwXcbHideWindow,
	.set_title = vkfwXcbSetWindowTitle,
	.get_event = vkfwXcbGetEvent,
	.translate_keycode = vkfwXcbTranslateKeycode,
	.translate_key = vkfwXcbTranslateKey
};
