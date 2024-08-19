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
};

#endif /* VKFW_WINDOW_H */
