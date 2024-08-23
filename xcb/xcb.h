/**
 * XCB functions.
 * Copyright (C) 2024  dbstream
 */
#include <xcb/xcb.h>

extern xcb_connection_t *vkfw_xcb_connection;
extern xcb_screen_t *vkfw_xcb_default_screen;

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
macro(xcb_create_window)		\
macro(xcb_destroy_window)		\
macro(xcb_map_window)			\
macro(xcb_unmap_window)			\
macro(xcb_change_property)		\
macro(xcb_intern_atom)			\
macro(xcb_intern_atom_reply)		\
macro(xcb_send_event)

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
#define xcb_create_window vkfw_xcb_create_window
#define xcb_destroy_window vkfw_xcb_destroy_window
#define xcb_map_window vkfw_xcb_map_window
#define xcb_unmap_window vkfw_xcb_unmap_window
#define xcb_change_property vkfw_xcb_change_property
#define xcb_intern_atom vkfw_xcb_intern_atom
#define xcb_intern_atom_reply vkfw_xcb_intern_atom_reply
#define xcb_send_event vkfw_xcb_send_event
