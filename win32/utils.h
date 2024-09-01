/**
 * Helper functions for Win32.
 * Copyright (C) 2024  dbstream
 */

/**
 * We need wchar_t to be UTF-16.
 */
static_assert (sizeof (wchar_t) == 2);

/**
 * Convert an UTF-8 string to UTF-16 for use with Win32 APIs.
 * Return value: a malloc'd string on success, or nullptr on failure.
 */
wchar_t *
vkfwUTF8ToUTF16 (const char *s);

/**
 * Convert an UTF-16 string to UTF-8.
 * Return value: a malloc'd string on success, or nullptr on failure.
 */
char *
vkfwUTF16ToUTF8 (const wchar_t *s);
