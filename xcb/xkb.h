/**
 * XKB functions.
 * Copyright (C) 2024  dbstream
 */
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#define VKFW_XKB_ALL_FUNCS(macro)		\
macro(xkb_x11_setup_xkb_extension)		\
macro(xkb_x11_get_core_keyboard_device_id)	\
macro(xkb_x11_keymap_new_from_device)		\
macro(xkb_x11_state_new_from_device)		\
macro(xkb_context_new)				\
macro(xkb_context_unref)			\
macro(xkb_context_set_log_level)		\
macro(xkb_keymap_unref)				\
macro(xkb_state_unref)				\
macro(xkb_state_key_get_one_sym)		\
macro(xkb_keysym_to_utf8)			\
macro(xkb_keymap_key_get_syms_by_level)		\
macro(xkb_keymap_key_get_name)			\
macro(xkb_compose_state_feed)			\
macro(xkb_compose_state_reset)			\
macro(xkb_compose_state_unref)			\
macro(xkb_compose_state_get_one_sym)		\
macro(xkb_compose_table_unref)			\
macro(xkb_keysym_to_utf32)			\
macro(xkb_compose_table_new_from_locale)	\
macro(xkb_compose_state_new)			\
macro(xkb_state_update_mask)			\
macro(xkb_compose_state_get_status)

#define VKFW_XKB_DEFINE_FUNC(name)	\
typedef decltype(&name) PFN##name;	\
extern PFN##name vkfw_##name;
VKFW_XKB_ALL_FUNCS(VKFW_XKB_DEFINE_FUNC)
#undef VKFW_XKB_DEFINE_FUNC

#define xkb_x11_setup_xkb_extension vkfw_xkb_x11_setup_xkb_extension
#define xkb_x11_get_core_keyboard_device_id vkfw_xkb_x11_get_core_keyboard_device_id
#define xkb_x11_keymap_new_from_device vkfw_xkb_x11_keymap_new_from_device
#define xkb_x11_state_new_from_device vkfw_xkb_x11_state_new_from_device
#define xkb_context_new vkfw_xkb_context_new
#define xkb_context_unref vkfw_xkb_context_unref
#define xkb_context_set_log_level vkfw_xkb_context_set_log_level
#define xkb_keymap_unref vkfw_xkb_keymap_unref
#define xkb_state_unref vkfw_xkb_state_unref
#define xkb_state_key_get_one_sym vkfw_xkb_state_key_get_one_sym
#define xkb_keysym_to_utf8 vkfw_xkb_keysym_to_utf8
#define xkb_keymap_key_get_syms_by_level vkfw_xkb_keymap_key_get_syms_by_level
#define xkb_keymap_key_get_name vkfw_xkb_keymap_key_get_name
#define xkb_compose_state_feed vkfw_xkb_compose_state_feed
#define xkb_compose_state_reset vkfw_xkb_compose_state_reset
#define xkb_compose_state_unref vkfw_xkb_compose_state_unref
#define xkb_compose_state_get_one_sym vkfw_xkb_compose_state_get_one_sym
#define xkb_compose_table_unref vkfw_xkb_compose_table_unref
#define xkb_keysym_to_utf32 vkfw_xkb_keysym_to_utf32
#define xkb_compose_table_new_from_locale vkfw_xkb_compose_table_new_from_locale
#define xkb_compose_state_new vkfw_xkb_compose_state_new
#define xkb_state_update_mask vkfw_xkb_state_update_mask
#define xkb_compose_state_get_status vkfw_xkb_compose_state_get_status
