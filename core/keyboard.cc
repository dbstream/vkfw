/**
 * Keyboard layout handling.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>

extern "C"
VKFWAPI int
vkfwTranslateKeycode (int keycode)
{
	if (vkfwCurrentWindowBackend->translate_keycode)
		return vkfwCurrentWindowBackend->translate_keycode (keycode);
	return VKFW_KEY_UNKNOWN;
}

extern "C"
VKFWAPI int
vkfwTranslateKey (int key)
{
	if (vkfwCurrentWindowBackend->translate_key)
		return vkfwCurrentWindowBackend->translate_key (key);
	return VKFW_KEY_UNKNOWN;
}
