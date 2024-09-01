/**
 * Win32 platform.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <stdlib.h>
#include "utils.h"
#include "win32.h"

static void *
loadModuleWin32 (const char *name)
{
	wchar_t *s = vkfwUTF8ToUTF16 (name);
	if (!s)
		return nullptr;

	HMODULE handle = LoadLibraryW (s);
	free (s);

	return (void *) handle;
}

static void
unloadModuleWin32 (void *handle)
{
	FreeLibrary ((HMODULE) handle);
}

static void *
lookupSymbolWin32 (void *handle, const char *name)
{
	return (void *) GetProcAddress ((HMODULE) handle, name);
}

static void *libvulkan_handle;

static void
terminatePlatformWin32 (void)
{
	if (libvulkan_handle)
		unloadModuleWin32 (libvulkan_handle);
}

static VkResult
loadVulkanWin32 (void **vulkan_loader)
{
	libvulkan_handle = loadModuleWin32 ("vulkan-1.dll");
	if (!libvulkan_handle) {
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: Win32: failed to load \"vulkan-1.dll\"\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	*vulkan_loader = lookupSymbolWin32 (libvulkan_handle, "vkGetInstanceProcAddr");
	if (!*vulkan_loader) {
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: Win32: couldn't find vkGetInstanceProcAddr\n");
		unloadModuleWin32 (libvulkan_handle);
		libvulkan_handle = nullptr;
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	return VK_SUCCESS;
}

extern VKFWwindowbackend vkfwBackendWin32;

static VKFWwindowbackend *
initBackendWin32 (void)
{
	if (vkfwBackendWin32.open_connection () != VK_SUCCESS)
		return nullptr;

	return &vkfwBackendWin32;
}

static uint64_t tsc_frequency;

static uint64_t
getTimeWin32 (void)
{
	LARGE_INTEGER i;
	QueryPerformanceCounter (&i);

	uint64_t tsc_value = i.QuadPart;

	/** 
	 * tsc_value/tsc_frequency is seconds.
	 * Therefore VKFW_SECONDS * tsc_value / tsc_frequency is what we want.
	 *
	 * However a direct calculation of that sort will lose too many high
	 * bits. Therefore, perform the above calculation using double.
	 *
	 * TODO: replace use of double with fixed-point math.
	 */
	return (uint64_t) (((double) tsc_value * (double) VKFW_SECONDS) / (double) tsc_frequency);
}

static void
delayWin32 (uint64_t t)
{
	/**
	 * TODO: replace this with a more high-precision sleep.
	 */
	Sleep (t / 1000);
}

/**
 * GCC allows struct declarations like
 *   VKFWplatform platform = {
 *     .foo = bar,
 *     .baz = xyz
 *   };
 * , but MSVC doesn't like this. To workaround this problem, explicitly set
 * members of VKFWplatform in initPlatformWin32.
 */

extern VKFWplatform vkfwPlatformWin32;

HINSTANCE vkfwHInstance;

static VkResult
initPlatformWin32 (void)
{
	vkfwPlatformWin32.terminatePlatform = terminatePlatformWin32;
	vkfwPlatformWin32.loadVulkan = loadVulkanWin32;
	vkfwPlatformWin32.initBackend = initBackendWin32;
	vkfwPlatformWin32.loadModule = loadModuleWin32;
	vkfwPlatformWin32.unloadModule = unloadModuleWin32;
	vkfwPlatformWin32.lookupSymbol = lookupSymbolWin32;
	vkfwPlatformWin32.getTime = getTimeWin32;
	vkfwPlatformWin32.delay = delayWin32;

	/** 
	 * This will actually initialize vkfwHInstance to the application
	 * HINSTANCE, but that shouldn't matter anyways.
	 */
	vkfwHInstance = GetModuleHandle (nullptr);

	unsigned int codepage = GetACP ();
	if (codepage != CP_UTF8) {
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: Win32: active codepage is %u\n", codepage);
		vkfwPrintf (VKFW_LOG_PLATFORM, "VKFW: Win32: it is strongly recommended to use CP_UTF8 (65001)\n");
	}

	LARGE_INTEGER i;
	QueryPerformanceFrequency (&i);

	tsc_frequency = i.QuadPart;
	return VK_SUCCESS;
}

VKFWplatform vkfwPlatformWin32 = {
	initPlatformWin32
};
