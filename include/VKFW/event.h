/**
 * VKFW events.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */
#include <VKFW/warn_internal.h>

#ifndef VKFW_EVENT_H
#define VKFW_EVENT_H 1

#include <VKFW/vkfw.h>

void
vkfwCleanupEvents (void);

void
vkfwQueueTextInputEvent (VKFWwindow *window, uint32_t codepoint,
	int x, int y, unsigned int mods);

#endif /* VKFW_EVENT_H */
