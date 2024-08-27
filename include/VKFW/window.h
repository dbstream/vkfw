/**
 * Shared fields in VKFWwindow_T between different platforms.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */
#include <VKFW/warn_internal.h>

#ifndef VKFW_WINDOW_H
#define VKFW_WINDOW_H 1

#include <VKFW/vkfw.h>

struct VKFWwindow_T {
	void *user;
	VkExtent2D extent;
	unsigned int internal_refcnt;
	unsigned int flags;
};

#define VKFW_WINDOW_DELETED 1U
#define VKFW_WINDOW_TEXT_INPUT_ENABLED 2U

void
vkfwRefWindow (VKFWwindow *window);

void
vkfwUnrefWindow (VKFWwindow *window);

#endif /* VKFW_WINDOW_H */
