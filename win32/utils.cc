/** 
 * Helper functions for Win32.
 * Copyright (C) 2024  dbstream
 */
#include <stdint.h>
#include <stdlib.h>
#include "utils.h"

/**
 * Parse one codepoint from a UTF-8 string.
 * Return value: a negative number on failure, otherwise Unicode codepoint.
 */
static int32_t
convert_from_utf8 (const char *buf, int *advance)
{
	if ((uint8_t) buf[0] >= 0xf8)
		/** Starting byte of 11111xxx, which is disallowed. */
		return -1;

	int32_t codepoint = 0;
	int n;
	if ((uint8_t) buf[0] >= 0xf0) {
		/** 4-byte encoding */
		codepoint += (uint8_t) buf[0] & 0x8;
		n = 4;
	} else if ((uint8_t) buf[0] >= 0xe0) {
		/** 3-byte encoding */
		codepoint += (uint8_t) buf[0] & 0xf;
		n = 3;
	} else if ((uint8_t) buf[0] >= 0xc0) {
		/** 2-byte encoding */
		codepoint += (uint8_t) buf[0] & 0x1f;
		n = 2;
	} else if ((uint8_t) buf[0] >= 0x80)
		/** Starting byte of 10xxxxxx, which is disallowed. */
		return -1;
	else {
		/** 1-byte encoding */
		*advance = 1;
		return buf[0];
	}

	if (!codepoint)
		/** Multi-byte encodings: Disallow overlong encodings */
		return -1;

	for (int i = 1; i < n; i++) {
		if (((uint8_t) buf[i] & 0xc0) != 0x80)
			/**
			 * Continuation bytes must be of the form 10xxxxxx.
			 * This check also detects the terminating NULL byte.
			 */
			return -1;

		codepoint = (uint32_t) codepoint << 6;
		codepoint += (uint8_t) buf[i] & 0x3f;
	}

	if (codepoint >= 0xd800 && codepoint < 0xe000)
		/** These codepoints are UTF-16 surrogate pairs. */
		return -1;

	if (codepoint >= 0x110000)
		/** These codepoints are outside the code space. */
		return -1;

	*advance = n;
	return codepoint;
}

/**
 * Convert one codepoint to UTF-8.
 * Return value: a negative number on failure, otherwise number of bytes
 * written.
 */
static int
convert_to_utf8 (char *buf, int32_t codepoint)
{
	if (codepoint < 0)
		/** Negative codepoints are invalid. */
		return -1;

	if (codepoint >= 0xd800 && codepoint < 0xe000)
		/** These codepoints are UTF-16 surrogate pairs. */
		return -1;

	if (codepoint < 0x80) {
		/** 1-byte encoding */
		buf[0] = codepoint;
		return 1;
	} else if (codepoint < 0x800) {
		/** 2-byte encoding */
		buf[0] = ((uint32_t) codepoint >> 6) | 0xc0;
		buf[1] = ((uint32_t) codepoint & 0x3f) | 0x80;
		return 2;
	} else if (codepoint < 0x10000) {
		/** 3-byte encoding */
		buf[0] = ((uint32_t) codepoint >> 12) | 0xe0;
		buf[1] = (((uint32_t) codepoint >> 6) & 0x3f) | 0x80;
		buf[2] = ((uint32_t) codepoint & 0x3f) | 0x80;
		return 3;
	} else if (codepoint < 0x110000) {
		/** 4-byte encoding */
		buf[0] = ((uint32_t) codepoint >> 18) | 0xf0;
		buf[1] = (((uint32_t) codepoint >> 12) & 0x3f) | 0x80;
		buf[2] = (((uint32_t) codepoint >> 6) & 0x3f) | 0x80;
		buf[3] = ((uint32_t) codepoint & 0x3f) | 0x80;
		return 4;
	}

	/** 5-byte encodings aren't allowed. */
	return -1;
}

/**
 * Parse one codepoint from a UTF-16 string.
 * Return value: a negative number on failure, otherwise Unicode codepoint.
 */
static int32_t
convert_from_utf16 (const wchar_t *buf, int *advance)
{
	if ((uint16_t) buf[0] < 0xd800 || (uint16_t) buf[0] >= 0xe000) {
		/** Code points in the Basic Multilingual Plane. */
		*advance = 1;
		return (uint16_t) buf[0];
	}

	uint16_t high_surrogate = (uint16_t) buf[0];
	uint16_t low_surrogate = (uint16_t) buf[1];

	if ((high_surrogate & 0xfc00) != 0xd800)
		return -1;
	if ((low_surrogate & 0xfc00) != 0xdc00)
		return -1;

	*advance = 2;
	return 0x10000
		+ (((uint32_t) high_surrogate & 0x3ff) << 10)
		+ ((uint32_t) low_surrogate & 0x3ff);
}

/**
 * Convert one codepoint to UTF-16.
 * Return value: a negative number on failure, otherwise number of wchars
 * written.
 */
static int
convert_to_utf16 (wchar_t *buf, int32_t codepoint)
{
	if (codepoint < 0)
		/** Negative codepoints are invalid. */
		return -1;

	if (codepoint >= 0xd800 && codepoint < 0xe000)
		/** These codepoints are UTF-16 surrogate pairs. */
		return -1;

	if (codepoint < 0x10000) {
		/** 1-wchar encoding. */
		buf[0] = codepoint;
		return 1;
	}

	if (codepoint >= 0x110000)
		/** Codepoints above 0x110000 are too large for UTF-16. */
		return -1;

	codepoint -= 0x10000;

	/** 2-wchar encoding. */
	buf[0] = 0xd800 + (codepoint >> 10);
	buf[1] = 0xdc00 + (codepoint & 0x3ff);
	return 2;
}

wchar_t *
vkfwUTF8ToUTF16 (const char *s)
{
	size_t n = 0;

	for (const char *p = s;;) {
		int a;
		int32_t c = convert_from_utf8 (p, &a);
		if (c < 0)
			return nullptr;

		p += a;

		wchar_t tmpbuf[2];
		a = convert_to_utf16 (tmpbuf, c);
		if (a < 0)
			return nullptr;

		n += a;
		if (!c)
			break;
	}

	wchar_t *buf = (wchar_t *) malloc (n * sizeof (wchar_t));
	if (!buf)
		return nullptr;

	for (wchar_t *p = buf;;) {
		int a;
		int32_t c = convert_from_utf8 (s, &a);
		s += a;

		a = convert_to_utf16 (p, c);
		p += a;

		if (!c)
			break;
	}

	return buf;
}

char *
vkfwUTF16ToUTF8 (const wchar_t *s)
{
	size_t n = 0;

	for (const wchar_t *p = s;;) {
		int a;
		int32_t c = convert_from_utf16 (p, &a);
		if (c < 0)
			return nullptr;

		p += a;

		char tmpbuf[4];
		a = convert_to_utf8 (tmpbuf, c);
		if (a < 0)
			return nullptr;

		n += a;
		if (!c)
			break;
	}

	char *buf = (char *) malloc (n * sizeof (char));
	if (!buf)
		return nullptr;

	for (char *p = buf;;) {
		int a;
		int32_t c = convert_from_utf16 (s, &a);
		s += a;

		a = convert_to_utf8 (p, c);
		p += a;

		if (!c)
			break;
	}

	return buf;
}
