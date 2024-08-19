/**
 * Keyboard input handling.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include "keyboard.h"

static int keycode_lookup[256];
static int key_lookup[VKFW_MAX_KEYS];

void
vkfwXcbInitKeyboard (void)
{
	for (int i = 0; i < 256; i++)
		keycode_lookup[i] = VKFW_KEY_UNKNOWN;
	for (int i = 0; i < VKFW_MAX_KEYS; i++)
		key_lookup[i] = VKFW_KEY_UNKNOWN;
}

int
vkfwXcbTranslateKeycode (int keycode)
{
	return keycode_lookup[keycode];
}

int
vkfwXcbTranslateKey (int key)
{
	return key_lookup[key];
}
