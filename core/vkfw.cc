/**
 * VKFW initialization.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <mutex>

#if defined (_WIN32)
#define VKFW_PLATFORM_NAME vkfwPlatformWin32
#elif defined(__APPLE__)
#define VKFW_PLATFORM_NAME vkfwPlatformApple
#else
#define VKFW_PLATFORM_NAME vkfwPlatformUnix
#endif

VKFWplatform *vkfwCurrentPlatform;
VKFWwindowbackend *vkfwCurrentWindowBackend;

extern VKFWplatform VKFW_PLATFORM_NAME;

static std::mutex init_mu;
static int init_count;

/**
 * The nature of vkfwInit and vkfwTerminate suggests that and application ought
 * to be able to do the following:
 *	vkfwInit ()
 *	... use Vulkan
 *	vkfwTerminate ()
 *	vkfwInit ()
 * At the moment, this is not supported, as there is a lot of state which isn't
 * properly cleaned up by vkfwTerminate. In the future it may be supported.
 *
 * Implementing support for re-initializing VKFW will require a thorough audit
 * of the codebase to ensure that old values are properly cleaned up.
 */
static bool force_init_fail;

static uint32_t supported_api_version;

extern "C"
VKFWAPI uint32_t
vkfwGetVkInstanceVersion (void)
{
	return supported_api_version;
}

/**
 * VKFW version checking:
 *
 * - vkfwInitVersion is expected to never change signature. Thus, application
 *   can call vkfwInitVersion with the version it was compiled against, and we
 *   can check if this version of the library will work with the application.
 *
 * - expected_version uses the VK_MAKE_API_VERSION format. variant is always 0.
 *
 * - Full VKFW version does not need to be changed, because Vulkan SDK version
 *   can only impact the exported symbols for Vulkan functions, which will cause
 *   linker errors on a mismatch.
 *
 * - If the application uses VKFW via dlopen(), LoadModuleA(), or similar, it
 *   needs to check that symbols are not NULL before using VKFW.
 */

extern "C"
VKFWAPI VkResult
vkfwInitVersion (uint32_t expected_version)
{
	/**
	 * Print the complete version of the VKFW library. This includes:
	 * - Full VKFW version.
	 * - Vulkan version we compiled against.
	 * - Supported platforms and contexts.
	 *
	 * Note that the Vulkan version is the version of the headers we
	 * compile against. The Vulkan version found on the system may differ.
	 */
	vkfwPrintf (VKFW_LOG_CORE, "VKFW %d.%d.%d-%d.%d.%d"
#if defined (_WIN32)
		" Win32"
#elif defined (__APPLE__)
		" Apple"
#else
		" Unix"
#endif
		"\n",
		VKFW_VERSION_MAJOR,
		VKFW_VERSION_MINOR,
		VKFW_VERSION_PATCH,
		VK_API_VERSION_MAJOR (VK_HEADER_VERSION_COMPLETE),
		VK_API_VERSION_MINOR (VK_HEADER_VERSION_COMPLETE),
		VK_API_VERSION_PATCH (VK_HEADER_VERSION_COMPLETE));

	int expected_variant = VK_API_VERSION_VARIANT (expected_version);
	int expected_major = VK_API_VERSION_MAJOR (expected_version);
	int expected_minor = VK_API_VERSION_MINOR (expected_version);
	int expected_patch = VK_API_VERSION_PATCH (expected_version);

	if (expected_variant != 0) {
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: version mismatch: the application is compiled against VKFW %d.%d.%d.%d (variant != 0)\n",
			expected_variant,
			expected_major,
			expected_minor,
			expected_patch);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (expected_major != VKFW_VERSION_MAJOR || expected_minor > VKFW_VERSION_MINOR) {
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: version mismatch: the application is compiled against VKFW %d.%d.%d\n",
			expected_major,
			expected_minor,
			expected_patch);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	std::scoped_lock g (init_mu);
	if (force_init_fail) {
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: the application tried to re-initialize VKFW after terminating it, which is currently not supported.\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (init_count) {
		init_count++;
		return VK_SUCCESS;
	}

	vkfwCurrentPlatform = &VKFW_PLATFORM_NAME;
	VkResult result = VK_SUCCESS;

	/**
	 * VKFW consists of three parts: the core, the platform and the backend.
	 * Platform is responsible for loading Vulkan and backend is responsible
	 * for window creation and events. initPlatform must happen before
	 * initBackend.
	 *
	 * Load Vulkan after initializing the backend. On unix/xcb, strange
	 * behaviors such as double frees can be caused if libvulkan.so.1 is
	 * loaded before libxcb.so.1. I have no idea why this happens, but
	 * initializing Xcb first seems to fix it.
	 */

	if (vkfwCurrentPlatform->initPlatform)
		result = vkfwCurrentPlatform->initPlatform ();
	if (result != VK_SUCCESS) {
		vkfwCurrentPlatform = nullptr;
		return result;
	}

	vkfwCurrentWindowBackend = vkfwCurrentPlatform->initBackend ();
	if (!vkfwCurrentWindowBackend) {
		if (vkfwCurrentPlatform->terminatePlatform)
			vkfwCurrentPlatform->terminatePlatform ();
		vkfwCurrentPlatform = nullptr;
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	void *gipa_;
	result = vkfwCurrentPlatform->loadVulkan (&gipa_);
	if (result != VK_SUCCESS) {
		if (vkfwCurrentPlatform->terminatePlatform)
			vkfwCurrentPlatform->terminatePlatform ();
		vkfwCurrentPlatform = nullptr;
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr) gipa_;
	vkfwLoadLoader (gipa);

	if (gipa (VK_NULL_HANDLE, "vkEnumerateInstanceVersion")) {
		result = vkEnumerateInstanceVersion (&supported_api_version);
		if (result != VK_SUCCESS) {
			if (vkfwCurrentWindowBackend->close_connection)
				vkfwCurrentWindowBackend->close_connection ();
			if (vkfwCurrentPlatform->terminatePlatform)
				vkfwCurrentPlatform->terminatePlatform ();
			vkfwCurrentPlatform = nullptr;
			return result;
		}
	} else {
		supported_api_version = VK_API_VERSION_1_0;
	}

	if (VK_API_VERSION_VARIANT (supported_api_version) != 0) {
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: Vulkan variant %d (we require variant 0)\n",
			VK_API_VERSION_VARIANT (supported_api_version));
		if (vkfwCurrentWindowBackend->close_connection)
			vkfwCurrentWindowBackend->close_connection ();	
		if (vkfwCurrentPlatform->terminatePlatform)
			vkfwCurrentPlatform->terminatePlatform ();
		vkfwCurrentPlatform = nullptr;
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwPrintf (VKFW_LOG_CORE, "VKFW: Vulkan version %d.%d.%d\n",
		VK_API_VERSION_MAJOR (supported_api_version),
		VK_API_VERSION_MINOR (supported_api_version),
		VK_API_VERSION_PATCH (supported_api_version));

	if (VK_API_VERSION_MAJOR (supported_api_version) != 1) {
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: requires Vulkan version 1.x, found %d.x\n",
			VK_API_VERSION_MAJOR (supported_api_version));
		if (vkfwCurrentWindowBackend->close_connection)
			vkfwCurrentWindowBackend->close_connection ();
		if (vkfwCurrentPlatform->terminatePlatform)
			vkfwCurrentPlatform->terminatePlatform ();
		vkfwCurrentPlatform = nullptr;
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	init_count++;
	return VK_SUCCESS;
}

extern "C"
VKFWAPI void
vkfwTerminate (void)
{
	std::scoped_lock g (init_mu);
	if (--init_count)
		return;

	/* Disallow re-initialization */
	force_init_fail = true;

	if (vkfwLoadedDevice)
		vkfwShutdownDevice ();
	if (vkfwLoadedInstance)
		vkfwShutdownInstance ();

	if (vkfwCurrentWindowBackend->close_connection)
		vkfwCurrentWindowBackend->close_connection ();
	vkfwCurrentWindowBackend = nullptr;

	if (vkfwCurrentPlatform->terminatePlatform)
		vkfwCurrentPlatform->terminatePlatform ();

	vkfwCurrentPlatform = nullptr;
}

extern "C"
VKFWAPI uint64_t
vkfwGetTime (void)
{
	return vkfwCurrentPlatform->getTime ();
}

extern "C"
VKFWAPI void
vkfwDelay (uint64_t t)
{
	if (vkfwCurrentPlatform->delay)
		vkfwCurrentPlatform->delay (t);
	else
		vkfwCurrentPlatform->delayUntil (t + vkfwGetTime ());
}

extern "C"
VKFWAPI void
vkfwDelayUntil (uint64_t t)
{
	if (vkfwCurrentPlatform->delayUntil)
		vkfwCurrentPlatform->delayUntil (t);
	else {
		uint64_t now = vkfwGetTime ();
		if (now < t)
			vkfwCurrentPlatform->delay (t - now);
	}
}
