/**
 * VKFW platform functions.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */
#include <VKFW/warn_internal.h>

#ifndef VKFW_PLATFORM_H
#define VKFW_PLATFORM_H 1

#include <VKFW/vkfw.h>

struct VKFWplatform_T {
	VkResult (*initPlatform) (void);
	void (*terminatePlatform) (void);

	VkResult (*loadVulkan) (void **vulkan_loader);

	VKFWwindowbackend *(*initBackend) (void);

	void *(*loadModule) (const char *name);
	void (*unloadModule) (void *handle);
	void *(*lookupSymbol) (void *handle, const char *name);

	uint64_t (*getTime) (void);
	void (*delay) (uint64_t);
	void (*delayUntil) (uint64_t);
};

extern VKFWplatform *vkfwCurrentPlatform;

#endif /* VKFW_PLATFORM_H */
