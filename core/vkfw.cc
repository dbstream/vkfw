/**
 * VKFW initialization.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/event.h>
#include <VKFW/logging.h>
#include <VKFW/options.h>
#include <VKFW/platform.h>
#include <VKFW/vector.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
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
 * Options are parsed into a vector. The vector begins empty, and is populated
 * in two stages:
 * 1. parse the [optstring]s into a vector of [option]s. There are three
 * optstrings: builtin_optstring, prog_optstring and VKFW_OPTIONS.
 * builtin_optstring is always parsed first. The order in which prog_optstring
 * and VKFW_OPTIONS are parsed depends on VKFW_OPT_PREFER_ENV.
 * 2. parse the vector of [option]s into a vector of [optname]s and [optarg]s.
 */

struct vkfw_option {
	char *optname;
	const char *optarg;
};

static VKFWvector<vkfw_option> vkfw_options;

static const char builtin_optstring[] = "enable_xcb;enable_wayland;enable_win32";

static const char *prog_optstring;
static uint32_t prog_optflags;

static VkResult
parse_options (void)
{
	const char *env_opts = nullptr;
	if (!(prog_optflags & VKFW_OPT_DONT_USE_ENV))
		env_opts = std::getenv ("VKFW_OPTIONS");

	const char *optstrings[4];
	size_t i = 0;

	/**
	 * Build a list of optstrings to parse. Optstrings that occur later in
	 * the array have priority in case of an option conflict.
	 */

	optstrings[i++] = builtin_optstring;
	if (prog_optflags & VKFW_OPT_PREFER_ENV) {
		if (prog_optstring)	optstrings[i++] = prog_optstring;
		if (env_opts)		optstrings[i++] = env_opts;
	} else {
		if (env_opts)		optstrings[i++] = env_opts;
		if (prog_optstring)	optstrings[i++] = prog_optstring;
	}

	optstrings[i++] = nullptr;

	i = 0;
	for (const char **p = optstrings; *p; p++)
		i += strlen (*p) + 1;

	char *opts = (char *) malloc (i + 1);
	if (!opts)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	opts[0] = 0;

	for (const char **p = optstrings; *p; p++)
		strcat (strcat (opts, ";"), *p);

	char *opts_orig = opts;

	// stage 1: tokenize the optstring into options.

	char *s = opts;
	for (; *opts; opts = s) {
		s = strchr (opts, ';');
		if (!s)
			for (s = opts; *s; s++);
		else if (opts == s) {
			s++;
			continue;
		}

		if (*s)
			*(s++) = 0;

		bool insert = true;
		if (*opts == '-') {
			opts++;
			insert = false;
		}

		if (!*opts)
			continue;

		for (i = 0; i < vkfw_options.size (); i++) {
			char *a = opts;
			char *b = vkfw_options[i].optname;

			bool equiv = true;
			for (;;) {
				if (!*a || *a == '=') {
					if (*b && *b != '=')
						equiv = false;
					break;
				} else if (!*b || *b == '=' || *a != *b) {
					equiv = false;
					break;
				}

				a++;
				b++;
			}

			if (equiv) {
				if (insert) {
					vkfw_options[i].optname = opts;
					insert = false;
				} else {
					vkfw_options[i] = vkfw_options[vkfw_options.size () - 1];
					vkfw_options.pop_back ();
				}
				break;
			}
		}

		if (insert) {
			vkfw_option opt {};
			opt.optname = opts;
			if (!vkfw_options.push_back (opt)) {
				free (opts_orig);
				vkfw_options.resize (0);
				return VK_ERROR_OUT_OF_HOST_MEMORY;
			}
		}
	}

	// stage 2: parse option values from options

	for (vkfw_option &o : vkfw_options) {
		char *s = o.optname;
		for (; *s && *s != '='; s++);
		if (*s == '=') {
			*(s++) = 0;
			o.optarg = s;
		} else
			o.optarg = "true";
	}

	return VK_SUCCESS;
}

extern "C"
VKFWAPI void
vkfwSetOptions (const char *optstring, uint32_t flags)
{
	prog_optstring = optstring;
	prog_optflags = flags;
}

extern "C"
VKFWAPI const char *
vkfwGetLibraryOption (const char *optname)
{
	for (const vkfw_option &o : vkfw_options)
		if (!strcmp (optname, o.optname))
			return o.optarg;

	return nullptr;
}

bool
vkfwGetBool (const char *name)
{
	const char *value = vkfwGetLibraryOption (name);
	if (!value)
		return false;

	return !strcmp (value, "true") || !strcmp (value, "1");
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

	result = parse_options ();
	if (result != VK_SUCCESS) {
		vkfwCurrentPlatform = nullptr;
		return result;
	}

	/**
	 * VKFW consists of three parts: the core, the platform and the backend.
	 * Platform is responsible for loading Vulkan and backend is responsible
	 * for window creation and events. initPlatform must happen before
	 * initBackend.
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

	vkfwCleanupEvents ();

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
