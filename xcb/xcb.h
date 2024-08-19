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

/**
 * Be extremely careful when adding XCB functions here. A mistake here can cause
 * bugs that appear to originate from libxcb and are hard to debug.
 *
 * When adding or removing function pointers, remember to update
 * vkfw_xcb_load_functions in connection.cc.
 *
 * TODO: use XML-XCB to generate this instead of writing it by hand.
 */

#ifndef VKFW_VERIFY_XCB_PROTOS
#define VKFW_VERIFY_XCB_PROTOS(name)
#endif

	/* things in <xcb/xcb.h> */

typedef xcb_connection_t *(*PFNxcb_connect) (
	const char *,
	int *);
VKFW_VERIFY_XCB_PROTOS(xcb_connect)

typedef void (*PFNxcb_disconnect) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_disconnect)

typedef const xcb_setup_t *(*PFNxcb_get_setup) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_get_setup)

typedef uint32_t (*PFNxcb_generate_id) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_generate_id)

typedef xcb_generic_error_t *(*PFNxcb_request_check) (
	xcb_connection_t *,
	xcb_void_cookie_t);
VKFW_VERIFY_XCB_PROTOS(xcb_request_check)

typedef xcb_generic_event_t *(*PFNxcb_wait_for_event) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_wait_for_event)

typedef xcb_generic_event_t *(*PFNxcb_poll_for_event) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_poll_for_event)

typedef xcb_generic_event_t *(*PFNxcb_poll_for_queued_event) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_poll_for_queued_event)

typedef int (*PFNxcb_get_file_descriptor) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_get_file_descriptor)

typedef int (*PFNxcb_flush) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_flush)

typedef int (*PFNxcb_connection_has_error) (
	xcb_connection_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_connection_has_error)

	/* things in <xcb/xproto.h> */

typedef xcb_intern_atom_cookie_t (*PFNxcb_intern_atom) (
	xcb_connection_t *,
	uint8_t,
	uint16_t,
	const char *);
VKFW_VERIFY_XCB_PROTOS(xcb_intern_atom)

typedef xcb_intern_atom_reply_t *(*PFNxcb_intern_atom_reply) (
	xcb_connection_t *,
	xcb_intern_atom_cookie_t,
	xcb_generic_error_t **);
VKFW_VERIFY_XCB_PROTOS(xcb_intern_atom_reply)

typedef xcb_screen_iterator_t (*PFNxcb_setup_roots_iterator) (
	const xcb_setup_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_setup_roots_iterator)

typedef void (*PFNxcb_screen_next) (
	xcb_screen_iterator_t *);
VKFW_VERIFY_XCB_PROTOS(xcb_screen_next)

typedef xcb_void_cookie_t (*PFNxcb_create_window) (
	xcb_connection_t *,
	uint8_t,
	xcb_window_t,
	xcb_window_t,
	int16_t,
	int16_t,
	uint16_t,
	uint16_t,
	uint16_t,
	uint16_t,
	xcb_visualid_t,
	uint32_t,
	const void *);
VKFW_VERIFY_XCB_PROTOS(xcb_create_window)

typedef xcb_void_cookie_t (*PFNxcb_destroy_window) (
	xcb_connection_t *,
	xcb_window_t);
VKFW_VERIFY_XCB_PROTOS(xcb_destroy_window)

typedef xcb_void_cookie_t (*PFNxcb_map_window) (
	xcb_connection_t *,
	xcb_window_t);
VKFW_VERIFY_XCB_PROTOS(xcb_map_window)

typedef xcb_void_cookie_t (*PFNxcb_unmap_window) (
	xcb_connection_t *,
	xcb_window_t);
VKFW_VERIFY_XCB_PROTOS(xcb_unmap_window)

typedef xcb_void_cookie_t (*PFNxcb_change_property) (
	xcb_connection_t *,
	uint8_t,
	xcb_window_t,
	xcb_atom_t,
	xcb_atom_t,
	uint8_t,
	uint32_t,
	const void *
);
VKFW_VERIFY_XCB_PROTOS(xcb_change_property)

typedef xcb_void_cookie_t (*PFNxcb_send_event) (
	xcb_connection_t *,
	uint8_t,
	xcb_window_t,
	uint32_t,
	const char *);
VKFW_VERIFY_XCB_PROTOS(xcb_send_event)

#undef VKFW_VERIFY_XCB_PROTOS

	/**
	 * declare variables
	 */

extern PFNxcb_connect vkfw_xcb_connect;
extern PFNxcb_disconnect vkfw_xcb_disconnect;
extern PFNxcb_get_setup vkfw_xcb_get_setup;
extern PFNxcb_generate_id vkfw_xcb_generate_id;
extern PFNxcb_request_check vkfw_xcb_request_check;
extern PFNxcb_wait_for_event vkfw_xcb_wait_for_event;
extern PFNxcb_poll_for_event vkfw_xcb_poll_for_event;
extern PFNxcb_poll_for_queued_event vkfw_xcb_poll_for_queued_event;
extern PFNxcb_get_file_descriptor vkfw_xcb_get_file_descriptor;
extern PFNxcb_flush vkfw_xcb_flush;
extern PFNxcb_connection_has_error vkfw_xcb_connection_has_error;
extern PFNxcb_setup_roots_iterator vkfw_xcb_setup_roots_iterator;
extern PFNxcb_screen_next vkfw_xcb_screen_next;
extern PFNxcb_create_window vkfw_xcb_create_window;
extern PFNxcb_destroy_window vkfw_xcb_destroy_window;
extern PFNxcb_map_window vkfw_xcb_map_window;
extern PFNxcb_unmap_window vkfw_xcb_unmap_window;
extern PFNxcb_change_property vkfw_xcb_change_property;
extern PFNxcb_intern_atom vkfw_xcb_intern_atom;
extern PFNxcb_intern_atom_reply vkfw_xcb_intern_atom_reply;
extern PFNxcb_send_event vkfw_xcb_send_event;

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
