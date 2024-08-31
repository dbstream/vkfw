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

bool
vkfwXcbCheck (xcb_void_cookie_t cookie)
{
	xcb_generic_error_t *e = xcb_request_check (vkfw_xcb_connection, cookie);

	if (!e)
		return false;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: error %u (conn_error=%d)\n",
		e->error_code, xcb_connection_has_error (vkfw_xcb_connection));

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: ... major=%u minor=%u\n",
		e->major_code, e->minor_code);

	if (e->error_code == XCB_VALUE) {
		xcb_value_error_t *ve = (xcb_value_error_t *) e;
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: ... bad value=%u\n", ve->bad_value);
	}

	free (e);
	return true;
}

static void *libxcb_handle;

static void
unload_xcb_funcs (void)
{
	vkfwCurrentPlatform->unloadModule (libxcb_handle);
}

#define VKFW_XCB_DEFINE_FUNC(name) PFN##name name;
VKFW_XCB_ALL_FUNCS(VKFW_XCB_DEFINE_FUNC)
#undef VKFW_XCB_DEFINE_FUNC

static bool
load_xcb_funcs (void)
{
	libxcb_handle = vkfwCurrentPlatform->loadModule ("libxcb.so.1");
	if (!libxcb_handle)
		libxcb_handle = vkfwCurrentPlatform->loadModule ("libxcb.so");
	if (!libxcb_handle)
		return false;

	bool failed = false;

#define VKFW_XCB_LOAD_FUNC(sym)								\
	sym = (PFN##sym) vkfwCurrentPlatform->lookupSymbol(libxcb_handle, #sym);	\
	if (!sym)									\
		failed = true;
	VKFW_XCB_ALL_FUNCS(VKFW_XCB_LOAD_FUNC)
#undef VKFW_XCB_LOAD_FUNC

	if (failed) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to load some symbols\n");
		unload_xcb_funcs ();
		return false;
	}

	return true;
}

xcb_connection_t *vkfw_xcb_connection;
xcb_screen_t *vkfw_xcb_default_screen;

xcb_cursor_t vkfw_xcb_cursors[2];

static void
destroy_cursors (void)
{
	xcb_free_cursor (vkfw_xcb_connection, vkfw_xcb_cursors[1]);
}

static bool
create_cursors (void)
{
	vkfw_xcb_cursors[0] = XCB_CURSOR_NONE;

	xcb_pixmap_t pixmap = xcb_generate_id (vkfw_xcb_connection);
	if (pixmap == -1)
		return false;

	xcb_void_cookie_t cookie = xcb_create_pixmap_checked (vkfw_xcb_connection,
		1, pixmap, vkfw_xcb_default_screen->root, 16, 16);
	if (vkfwXcbCheck (cookie))
		return false;

	vkfw_xcb_cursors[1] = xcb_generate_id (vkfw_xcb_connection);
	if (vkfw_xcb_cursors[1] == -1) {
		xcb_free_pixmap (vkfw_xcb_connection, pixmap);
		return false;
	}

	cookie = xcb_create_cursor_checked (vkfw_xcb_connection, vkfw_xcb_cursors[1],
		pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
	if (vkfwXcbCheck (cookie)) {
		xcb_free_pixmap (vkfw_xcb_connection, pixmap);
		return false;
	}

	xcb_free_pixmap (vkfw_xcb_connection, pixmap);
	return true;
}

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
	if (e) {							\
		failed = true;						\
		free (e);						\
	}								\
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

	if (!create_cursors ()) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb backend failed to create cursors\n");
		xcb_disconnect (vkfw_xcb_connection);
		unload_xcb_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwXcbInitKeyboard ();
	return VK_SUCCESS;
}

static void
vkfwXcbClose (void)
{
	vkfwXcbTerminateKeyboard ();
	destroy_cursors ();
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
	.translate_key = vkfwXcbTranslateKey,
	.update_pointer_mode = vkfwXcbUpdatePointerMode
};
