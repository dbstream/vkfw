/**
 * Keyboard input handling.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/event.h>
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include "keyboard.h"
#include "xcb.h"
#include "xkb.h"

#include <inttypes.h>
#include <locale.h>
#include <string.h>

static int keycode_lookup[256];
static int key_lookup[VKFW_MAX_KEYS];

int
vkfwXcbTranslateKeycode (int keycode)
{
	if (keycode >= 0 && keycode <= 255)
		return keycode_lookup[keycode];
	return VKFW_KEY_UNKNOWN;
}

int
vkfwXcbTranslateKey (int key)
{
	if (key >= 0 && key < VKFW_MAX_KEYS)
		return key_lookup[key];
	return VKFW_KEY_UNKNOWN;
}

bool vkfw_has_xkb;
uint8_t vkfw_xkb_event_base;

static xkb_context *vkfw_xkb_ctx;
static int32_t core_kbd_devid;

static xkb_compose_table *compose_table;

struct keyboard_data {
	xkb_keymap *keymap;
	xkb_state *state;
	xkb_compose_state *compose;
	int32_t devid;
};

static struct keyboard_data keyboard;

static void
unref_keyboard (keyboard_data *kbd)
{
	xkb_compose_state_unref (kbd->compose);
	xkb_state_unref (kbd->state);
	xkb_keymap_unref (kbd->keymap);
}

void
vkfwXcbXkbKeyPress (const VKFWevent *e, xcb_key_press_event_t *xe)
{
	xkb_keycode_t keycode = xe->detail;
	xkb_keysym_t keysym = xkb_state_key_get_one_sym (keyboard.state, keycode);
	if (!keysym)
		return;

	xkb_compose_state_feed (keyboard.compose, keysym);
	xkb_compose_status status = xkb_compose_state_get_status (keyboard.compose);

	if (status == XKB_COMPOSE_CANCELLED) {
		xkb_compose_state_reset (keyboard.compose);
		keysym = 0;
	} else if (status == XKB_COMPOSE_COMPOSED) {
		keysym = xkb_compose_state_get_one_sym (keyboard.compose);
		xkb_compose_state_reset (keyboard.compose);
	} else if (status != XKB_COMPOSE_NOTHING)
		keysym = 0;

	if (keysym) {
		uint32_t codepoint = xkb_keysym_to_utf32 (keysym);
		if (codepoint)
			vkfwQueueTextInputEvent (e->window, codepoint,
				e->x, e->y, e->modifiers);
	}
}

void
vkfwXcbXkbKeyRelease (const VKFWevent *e, xcb_key_release_event_t *xe)
{
	(void) e;
	(void) xe;
}

static bool
select_events (int32_t devid)
{
	uint16_t kbd = XCB_XKB_NKN_DETAIL_KEYCODES;

	uint16_t state = XCB_XKB_STATE_PART_MODIFIER_BASE
		| XCB_XKB_STATE_PART_MODIFIER_LATCH
		| XCB_XKB_STATE_PART_MODIFIER_LOCK
		| XCB_XKB_STATE_PART_GROUP_BASE
		| XCB_XKB_STATE_PART_GROUP_LATCH
		| XCB_XKB_STATE_PART_GROUP_LOCK;

	uint16_t events = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY
		| XCB_XKB_EVENT_TYPE_MAP_NOTIFY
		| XCB_XKB_EVENT_TYPE_STATE_NOTIFY;

	uint16_t map = XCB_XKB_MAP_PART_KEY_TYPES
		| XCB_XKB_MAP_PART_KEY_SYMS
		| XCB_XKB_MAP_PART_MODIFIER_MAP
		| XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
		| XCB_XKB_MAP_PART_KEY_ACTIONS
		| XCB_XKB_MAP_PART_VIRTUAL_MODS
		| XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;

	xcb_xkb_select_events_details_t details = {
		.affectNewKeyboard = kbd,
		.newKeyboardDetails = kbd,
		.affectState = state,
		.stateDetails = state
	};

	xcb_xkb_select_events_aux (vkfw_xcb_connection,
		devid, events, 0, 0, map, map, &details);

	return true;
}

static bool
setup_keyboard (keyboard_data *kbd, bool recreate)
{
	xkb_keymap *keymap = xkb_x11_keymap_new_from_device (vkfw_xkb_ctx,
		vkfw_xcb_connection, kbd->devid, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap)
		return false;

	xkb_state *state = xkb_x11_state_new_from_device (keymap,
		vkfw_xcb_connection, kbd->devid);
	if (!state) {
		xkb_keymap_unref (keymap);
		return false;
	}

	xkb_compose_state *compose = xkb_compose_state_new (compose_table,
		XKB_COMPOSE_STATE_NO_FLAGS);
	if (!compose) {
		xkb_state_unref (state);
		xkb_keymap_unref (keymap);
		return false;
	}

	if (recreate)
		unref_keyboard (kbd);
	kbd->keymap = keymap;
	kbd->state = state;
	kbd->compose = compose;
	return true;
}

static int
name_to_key (const char *name)
{
	static const struct vkfw_xcb_key_table {
		const char *name;
		int key;
	} table[] = {
		{ "SPCE",	VKFW_KEY_SPACE },
		{ "AE01",	VKFW_KEY_1 },
		{ "AE02",	VKFW_KEY_2 },
		{ "AE03",	VKFW_KEY_3 },
		{ "AE04",	VKFW_KEY_4 },
		{ "AE05",	VKFW_KEY_5 },
		{ "AE06",	VKFW_KEY_6 },
		{ "AE07",	VKFW_KEY_7 },
		{ "AE08",	VKFW_KEY_8 },
		{ "AE09",	VKFW_KEY_9 },
		{ "AE10",	VKFW_KEY_0 },
		{ "AD01",	VKFW_KEY_Q },
		{ "AD02",	VKFW_KEY_W },
		{ "AD03",	VKFW_KEY_E },
		{ "AD04",	VKFW_KEY_R },
		{ "AD05",	VKFW_KEY_T },
		{ "AD06",	VKFW_KEY_Y },
		{ "AD07",	VKFW_KEY_U },
		{ "AD08",	VKFW_KEY_I },
		{ "AD09",	VKFW_KEY_O },
		{ "AD10",	VKFW_KEY_P },
		{ "AC01",	VKFW_KEY_A },
		{ "AC02",	VKFW_KEY_S },
		{ "AC03",	VKFW_KEY_D },
		{ "AC04",	VKFW_KEY_F },
		{ "AC05",	VKFW_KEY_G },
		{ "AC06",	VKFW_KEY_H },
		{ "AC07",	VKFW_KEY_J },
		{ "AC08",	VKFW_KEY_K },
		{ "AC09",	VKFW_KEY_L },
		{ "AB01",	VKFW_KEY_Z },
		{ "AB02",	VKFW_KEY_X },
		{ "AB03",	VKFW_KEY_C },
		{ "AB04",	VKFW_KEY_V },
		{ "AB05",	VKFW_KEY_B },
		{ "AB06",	VKFW_KEY_N },
		{ "AB07",	VKFW_KEY_M },
		{ "LCTL",	VKFW_KEY_LEFT_CTRL },
		{ "LFSH",	VKFW_KEY_LEFT_SHIFT },
		{ "LALT",	VKFW_KEY_LEFT_ALT },
		{ "RCTL",	VKFW_KEY_RIGHT_CTRL },
		{ "RTSH",	VKFW_KEY_RIGHT_SHIFT },
		{ "RALT",	VKFW_KEY_RIGHT_ALT },
		{ "BKSP",	VKFW_KEY_BACKSPACE },
		{ "INS",	VKFW_KEY_INSERT },
		{ "DELE",	VKFW_KEY_DEL },
		{ "HOME",	VKFW_KEY_HOME },
		{ "END",	VKFW_KEY_END },
		{ "PGUP",	VKFW_KEY_PG_UP },
		{ "PGDN",	VKFW_KEY_PG_DOWN },
		{ "LEFT",	VKFW_KEY_ARROW_LEFT },
		{ "RGHT",	VKFW_KEY_ARROW_RIGHT },
		{ "UP",		VKFW_KEY_ARROW_UP },
		{ "DOWN",	VKFW_KEY_ARROW_DOWN },
		{ "ESC",	VKFW_KEY_ESC },
		{ "KP0",	VKFW_KEY_NUMPAD_0 },
		{ "KP1",	VKFW_KEY_NUMPAD_1 },
		{ "KP2",	VKFW_KEY_NUMPAD_2 },
		{ "KP3",	VKFW_KEY_NUMPAD_3 },
		{ "KP4",	VKFW_KEY_NUMPAD_4 },
		{ "KP5",	VKFW_KEY_NUMPAD_5 },
		{ "KP6",	VKFW_KEY_NUMPAD_6 },
		{ "KP7",	VKFW_KEY_NUMPAD_7 },
		{ "KP8",	VKFW_KEY_NUMPAD_8 },
		{ "KP9",	VKFW_KEY_NUMPAD_9 },
		{ "KPAD",	VKFW_KEY_NUMPAD_ADD },
		{ "KPSU",	VKFW_KEY_NUMPAD_SUBTRACT },
		{ "KPDL",	VKFW_KEY_NUMPAD_COMMA },
		{ "KPMU",	VKFW_KEY_NUMPAD_MULTIPLY },
		{ "KPDV",	VKFW_KEY_NUMPAD_DIVIDE },
		{ "KPEN",	VKFW_KEY_NUMPAD_ENTER },
		{ "FK01",	VKFW_KEY_F1 },
		{ "FK02",	VKFW_KEY_F2 },
		{ "FK03",	VKFW_KEY_F3 },
		{ "FK04",	VKFW_KEY_F4 },
		{ "FK05",	VKFW_KEY_F5 },
		{ "FK06",	VKFW_KEY_F6 },
		{ "FK07",	VKFW_KEY_F7 },
		{ "FK08",	VKFW_KEY_F8 },
		{ "FK09",	VKFW_KEY_F9 },
		{ "FK10",	VKFW_KEY_F10 },
		{ "FK11",	VKFW_KEY_F11 },
		{ "FK12",	VKFW_KEY_F12 },
		{ "FK13",	VKFW_KEY_F13 },
		{ "FK14",	VKFW_KEY_F14 },
		{ "FK15",	VKFW_KEY_F15 },
		{ "FK16",	VKFW_KEY_F16 },
		{ "FK17",	VKFW_KEY_F17 },
		{ "FK18",	VKFW_KEY_F18 },
		{ "FK19",	VKFW_KEY_F19 },
		{ "FK20",	VKFW_KEY_F20 },
		{ "FK21",	VKFW_KEY_F21 },
		{ "FK22",	VKFW_KEY_F22 },
		{ "FK23",	VKFW_KEY_F23 },
		{ "FK24",	VKFW_KEY_F24 },
		{ "FK25",	VKFW_KEY_F25 }
	};

	for (const vkfw_xcb_key_table &e : table)
		if (!strcmp (e.name, name))
			return e.key;
	return VKFW_KEY_UNKNOWN;
}

static void
generate_tables (keyboard_data *kbd)
{
	for (int i = 0; i < 256; i++)
		keycode_lookup[i] = VKFW_KEY_UNKNOWN;
	for (int i = 0; i < VKFW_MAX_KEYS; i++)
		key_lookup[i] = VKFW_KEY_UNKNOWN;

	/**
	 * Look at the physical map of keys.
	 * Translate them to VKFW_KEY_* and store them in keycode_lookup.
	 */
	for (int i = 0; i < 256; i++) {
		if (!xkb_keycode_is_legal_x11 (i))
			continue;
		const char *name = xkb_keymap_key_get_name (kbd->keymap, i);
		if (!name)
			continue;

		keycode_lookup[i] = name_to_key (name);
	}

	/**
	 * Build the inverse table.
	 */
	for (int i = 0; i < 256; i++) {
		int j = keycode_lookup[i];
		if (j == VKFW_KEY_UNKNOWN)
			continue;
		if (key_lookup[j] != VKFW_KEY_UNKNOWN)
			key_lookup[j] = i;
	}
}

void
vkfwXcbXkbNewKeyboardNotify (xcb_xkb_new_keyboard_notify_event_t *e)
{
	if (setup_keyboard (&keyboard, true))
		generate_tables (&keyboard);
	else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to handle XkbNewKeyboardNotify\n");
}

void
vkfwXcbXkbMapNotify (xcb_xkb_map_notify_event_t *e)
{
	if (setup_keyboard (&keyboard, true))
		generate_tables (&keyboard);
	else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to handle XkbMapNotify\n");
}

void
vkfwXcbXkbStateNotify (xcb_xkb_state_notify_event_t *e)
{
	xkb_state_update_mask (keyboard.state, e->baseMods, e->latchedMods,
		e->lockedMods, e->baseGroup, e->latchedGroup, e->lockedGroup);
}

static void unload_xkb (void);

static void
setup_xkb (void)
{
	int result = xkb_x11_setup_xkb_extension (vkfw_xcb_connection,
		XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
		XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, nullptr, nullptr,
		&vkfw_xkb_event_base, nullptr);
	if (!result) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to setup XKB extension\n");
		unload_xkb ();
		return;
	}

	vkfw_xkb_ctx = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
	if (!vkfw_xkb_ctx) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to create an XKB context\n");
		unload_xkb ();
		return;
	}

	xkb_context_set_log_level (vkfw_xkb_ctx, XKB_LOG_LEVEL_DEBUG);

	const char *compose_locale = setlocale (LC_CTYPE, nullptr);
	if (!compose_locale)
		compose_locale = "C";

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: compose_locale=%s\n", compose_locale);

	compose_table = xkb_compose_table_new_from_locale (vkfw_xkb_ctx,
		compose_locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (!compose_table) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to create XKB compose table from locale \"%s\"\n",
			compose_locale);
		xkb_context_unref (vkfw_xkb_ctx);
		unload_xkb ();
		return;
	}

	core_kbd_devid = xkb_x11_get_core_keyboard_device_id (vkfw_xcb_connection);
	if (core_kbd_devid < 0) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to get core keyboard device ID\n");
		xkb_compose_table_unref (compose_table);
		xkb_context_unref (vkfw_xkb_ctx);
		unload_xkb ();
		return;
	}

	select_events (core_kbd_devid);

	keyboard.devid = core_kbd_devid;
	if (!setup_keyboard (&keyboard, false)) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to setup XKB keyboard state\n");
		xkb_compose_table_unref (compose_table);
		xkb_context_unref (vkfw_xkb_ctx);
		unload_xkb ();
		return;
	}

	generate_tables (&keyboard);
}

static void load_xkb (void);

void
vkfwXcbInitKeyboard (void)
{
	for (int i = 0; i < 256; i++)
		keycode_lookup[i] = VKFW_KEY_UNKNOWN;
	for (int i = 0; i < VKFW_MAX_KEYS; i++)
		key_lookup[i] = VKFW_KEY_UNKNOWN;

	load_xkb ();
	if (vkfw_has_xkb)
		setup_xkb ();

	if (!vkfw_has_xkb)
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: Failed to initialize XKB. Keyboard input will probably be broken\n");
}

void
vkfwXcbTerminateKeyboard (void)
{
	if (vkfw_has_xkb) {
		unref_keyboard (&keyboard);
		xkb_context_unref (vkfw_xkb_ctx);
		unload_xkb ();
	}
}

static void *libxkbcommon_x11_handle;
static void *libxcb_xkb_handle;

static void
unload_xkb (void)
{
	vkfw_has_xkb = false;
	vkfwCurrentPlatform->unloadModule (libxkbcommon_x11_handle);
}

#define VKFW_XKB_DEFINE_FUNC(name) PFN##name name;
VKFW_XKB_ALL_FUNCS(VKFW_XKB_DEFINE_FUNC)
VKFW_XCB_XKB_ALL_FUNCS(VKFW_XKB_DEFINE_FUNC)
#undef VKFW_XKB_DEFINE_FUNC

static void
load_xkb (void)
{
	libxcb_xkb_handle = vkfwCurrentPlatform->loadModule ("libxcb-xkb.so.1");
	if (!libxcb_xkb_handle)
		libxcb_xkb_handle = vkfwCurrentPlatform->loadModule ("libxcb-xkb.so");
	if (!libxcb_xkb_handle) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: libxcb-xkb is not present\n");
		return ;
	}

	libxkbcommon_x11_handle = vkfwCurrentPlatform->loadModule ("libxkbcommon-x11.so");
	if (!libxkbcommon_x11_handle) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: libxkbcommon-x11 is not present\n");
		vkfwCurrentPlatform->unloadModule (libxcb_xkb_handle);
		return;
	}

	bool failed = false;
#define VKFW_XKB_LOAD_FUNC(name)								\
	name = (PFN##name) vkfwCurrentPlatform->lookupSymbol (libxkbcommon_x11_handle, #name);	\
	if (!name)										\
		failed = true;
	VKFW_XKB_ALL_FUNCS(VKFW_XKB_LOAD_FUNC)
#undef VKFW_XKB_LOAD_FUNC

#define VKFW_XCB_XKB_LOAD_FUNC(name)								\
	name = (PFN##name) vkfwCurrentPlatform->lookupSymbol (libxcb_xkb_handle, #name);	\
	if (!name)										\
		failed = true;
	VKFW_XCB_XKB_ALL_FUNCS(VKFW_XCB_XKB_LOAD_FUNC)
#undef VKFW_XCB_XKB_LOAD_FUNC

	if (failed) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: failed to load some xkbcommon symbols\n");
		unload_xkb ();
		return;
	}

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Xcb: using libxkbcommon for keyboard input\n");
	vkfw_has_xkb = true;
}
