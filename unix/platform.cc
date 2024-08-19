/**
 * Unix-like platform.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/window_api.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>

static void *libvulkan_handle;

static const char *libvulkan_path;

const char *libvulkan_paths[] = {
	"libvulkan.so.1",
	"libvulkan.so"
};

static void *
loadModuleUnix (const char *name);

static void
unloadModuleUnix (void *handle);

static void *
lookupSymbolUnix (void *handle, const char *name);

static VkResult
loadVulkanUnix (void **vulkan_loader)
{
	for (const char *path : libvulkan_paths) {
		libvulkan_handle = loadModuleUnix (path);
		if (libvulkan_handle) {
			libvulkan_path = path;
			break;
		}
	}

	if (!libvulkan_handle) {
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: initPlatformUnix failed: failed to dlopen() Vulkan\n");
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: initPlatformUnix failed: tried\n");
		for (const char *path : libvulkan_paths)
			vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: ... %s\n", path);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	*vulkan_loader = lookupSymbolUnix (libvulkan_handle, "vkGetInstanceProcAddr");
	if (!*vulkan_loader) {
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: initPlatformUnix failed: %s does not contain vkGetInstanceProcAddr\n",
			libvulkan_path);
		unloadModuleUnix (libvulkan_handle);
	}

	return VK_SUCCESS;
}

static void
terminatePlatformUnix (void)
{
	if (libvulkan_handle) {
		unloadModuleUnix (libvulkan_handle);
		libvulkan_handle = nullptr;
		libvulkan_path = nullptr;
	}
}

extern VKFWwindowbackend vkfwBackendXcb;

static VKFWwindowbackend *
initBackendUnix (void)
{
	if (vkfwBackendXcb.open_connection () == VK_SUCCESS)
		return &vkfwBackendXcb;
	return nullptr;
}

static void *
loadModuleUnix (const char *name)
{
	return dlopen (name, RTLD_NOW | RTLD_LOCAL);
}

static void
unloadModuleUnix (void *handle)
{
	dlclose (handle);
}

static void *
lookupSymbolUnix (void *handle, const char *name)
{
	return dlsym (handle, name);
}

static uint64_t
getTimeUnix (void)
{
	struct timespec t;
	clock_gettime (CLOCK_MONOTONIC, &t);
	uint64_t v = t.tv_sec;
	v *= 1000000;
	v += t.tv_nsec / 1000;
	return v;
}

static void
delayUntilUnix (uint64_t target)
{
	struct timespec t;
	t.tv_sec = target / 1000000;
	t.tv_nsec = (target % 1000000) * 1000;

	while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &t, nullptr) == EINTR);
}

VKFWplatform vkfwPlatformUnix = {
	.terminatePlatform = terminatePlatformUnix,
	.loadVulkan = loadVulkanUnix,
	.initBackend = initBackendUnix,
	.loadModule = loadModuleUnix,
	.unloadModule = unloadModuleUnix,
	.lookupSymbol = lookupSymbolUnix,
	.getTime = getTimeUnix,
	.delayUntil = delayUntilUnix
};
