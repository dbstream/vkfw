/**
 * XCB functions.
 * Copyright (C) 2024  dbstream
 */
#include <xcb/xcb.h>

extern xcb_connection_t *vkfw_xcb_connection;
extern xcb_screen_t *vkfw_xcb_default_screen;

extern xcb_cursor_t vkfw_xcb_cursors[];

bool
vkfwXcbCheck (xcb_void_cookie_t cookie);

#define VKFW_XCB_CURSOR_NORMAL 0
#define VKFW_XCB_CURSOR_HIDDEN 1

#define VKFW_XCB_ALL_ATOMS(macro)		\
	macro(WM_PROTOCOLS)			\
	macro(WM_DELETE_WINDOW)			\
	macro(_NET_WM_PING)

#define VKFW_DECLARE_ATOM(name) extern xcb_atom_t vkfw_##name;
VKFW_XCB_ALL_ATOMS(VKFW_DECLARE_ATOM)
#undef VKFW_DECLARE_ATOM

#define VKFW_XCB_ALL_FUNCS(macro)	\
macro(xcb_connect)			\
macro(xcb_disconnect)			\
macro(xcb_get_setup)			\
macro(xcb_generate_id)			\
macro(xcb_request_check)		\
macro(xcb_wait_for_event)		\
macro(xcb_poll_for_event)		\
macro(xcb_poll_for_queued_event)	\
macro(xcb_get_file_descriptor)		\
macro(xcb_flush)			\
macro(xcb_connection_has_error)		\
macro(xcb_setup_roots_iterator)		\
macro(xcb_screen_next)			\
macro(xcb_create_window_checked)	\
macro(xcb_destroy_window)		\
macro(xcb_map_window_checked)		\
macro(xcb_unmap_window_checked)		\
macro(xcb_change_property_checked)	\
macro(xcb_intern_atom)			\
macro(xcb_intern_atom_reply)		\
macro(xcb_send_event_checked)		\
macro(xcb_create_cursor_checked)	\
macro(xcb_free_cursor)			\
macro(xcb_free_pixmap)			\
macro(xcb_create_pixmap_checked)	\
macro(xcb_change_window_attributes)	\
macro(xcb_grab_pointer_unchecked)	\
macro(xcb_ungrab_pointer)		\
macro(xcb_warp_pointer)

#define VKFW_XCB_DEFINE_FUNC(name)	\
typedef decltype(&name) PFN##name;	\
extern PFN##name vkfw_##name;
VKFW_XCB_ALL_FUNCS(VKFW_XCB_DEFINE_FUNC)
#undef VKFW_XCB_DEFINE_FUNC

	/**
	 * define macros
	 */

#define xcb_connect vkfw_xcb_connect
#define xcb_disconnect vkfw_xcb_disconnect
#define xcb_get_setup vkfw_xcb_get_setup
#define xcb_generate_id vkfw_xcb_generate_id
#define xcb_request_check vkfw_xcb_request_check
#define xcb_wait_for_event vkfw_xcb_wait_for_event
#define xcb_poll_for_event vkfw_xcb_poll_for_event
#define xcb_poll_for_queued_event vkfw_xcb_poll_for_queued_event
#define xcb_get_file_descriptor vkfw_xcb_get_file_descriptor
#define xcb_flush vkfw_xcb_flush
#define xcb_connection_has_error vkfw_xcb_connection_has_error
#define xcb_setup_roots_iterator vkfw_xcb_setup_roots_iterator
#define xcb_screen_next vkfw_xcb_screen_next
#define xcb_create_window_checked vkfw_xcb_create_window_checked
#define xcb_destroy_window vkfw_xcb_destroy_window
#define xcb_map_window_checked vkfw_xcb_map_window_checked
#define xcb_unmap_window_checked vkfw_xcb_unmap_window_checked
#define xcb_change_property_checked vkfw_xcb_change_property_checked
#define xcb_intern_atom vkfw_xcb_intern_atom
#define xcb_intern_atom_reply vkfw_xcb_intern_atom_reply
#define xcb_send_event_checked vkfw_xcb_send_event_checked
#define xcb_create_cursor_checked vkfw_xcb_create_cursor_checked
#define xcb_free_cursor vkfw_xcb_free_cursor
#define xcb_free_pixmap vkfw_xcb_free_pixmap
#define xcb_create_pixmap_checked vkfw_xcb_create_pixmap_checked
#define xcb_change_window_attributes vkfw_xcb_change_window_attributes
#define xcb_grab_pointer_unchecked vkfw_xcb_grab_pointer_unchecked
#define xcb_ungrab_pointer vkfw_xcb_ungrab_pointer
#define xcb_warp_pointer vkfw_xcb_warp_pointer
