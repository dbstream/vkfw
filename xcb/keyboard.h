/**
 * Keycode --> VKFW_KEY_* mapping
 * Copyright (C) 2024  dbstream
 */

#include "xcb_xkb.h"
#include <VKFW/vkfw.h>
#include <stdint.h>

void
vkfwXcbInitKeyboard (void);

void
vkfwXcbTerminateKeyboard (void);

int
vkfwXcbTranslateKeycode (int keycode);

int
vkfwXcbTranslateKey (int key);

extern bool vkfw_has_xkb;
extern uint8_t vkfw_xkb_event_base;

void
vkfwXcbXkbNewKeyboardNotify (xcb_xkb_new_keyboard_notify_event_t *e);

void
vkfwXcbXkbMapNotify (xcb_xkb_map_notify_event_t *e);

void
vkfwXcbXkbStateNotify (xcb_xkb_state_notify_event_t *e);

/**
 * Notifier for key press events. This is not intended to fill in VKFWevent, but
 * rather to allow keyboard.cc to enqueue TEXT_INPUT events.
 */
void
vkfwXcbXkbKeyPress (const VKFWevent *e, xcb_key_press_event_t *xe);

/**
 * Notifier for key release events. See above comment.
 */
void
vkfwXcbXkbKeyRelease (const VKFWevent *e, xcb_key_release_event_t *xe);
