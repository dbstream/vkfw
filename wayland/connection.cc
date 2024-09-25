/**
 * Wayland backend initialization.
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#include <VKFW/logging.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include "event.h"
#include "wayland.h"
#include "window.h"

#include <stdlib.h>
#include <string.h>

wl_display *vkfwWlDisplay;
wl_registry *vkfwWlRegistry;
wl_compositor *vkfwWlCompositor;
xdg_wm_base *vkfwXdgWmBase;

static VKFWwindow *
vkfwWlAllocWindow (void)
{
	return (VKFWwindow *) malloc (sizeof (VKFWwlwindow));
}

static VkResult
vkfwWlRequestExtensions (void)
{
	VkResult result;

	result = vkfwRequestInstanceExtension (VK_KHR_SURFACE_EXTENSION_NAME, true);
	if (result != VK_SUCCESS)
		return result;

	result = vkfwRequestInstanceExtension (VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, true);
	if (result != VK_SUCCESS)
		return result;

	return VK_SUCCESS;
}

static VkResult
vkfwWlCreateSurface (VKFWwindow *window, VkSurfaceKHR *surface)
{
	VKFWwlwindow *w = (VKFWwlwindow *) window;

	VkWaylandSurfaceCreateInfoKHR surface_ci {};
	surface_ci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
	surface_ci.display = vkfwWlDisplay;
	surface_ci.surface = w->surface;

	return vkCreateWaylandSurfaceKHR (vkfwLoadedInstance, &surface_ci,
		nullptr, surface);
}

static VkResult
vkfwWlQueryPresentSupport (VkPhysicalDevice device, uint32_t queue, VkBool32 *result)
{
	*result = vkGetPhysicalDeviceWaylandPresentationSupportKHR (device, queue,
		vkfwWlDisplay);
	return VK_SUCCESS;
}

static uint32_t vkfwWlCompositorId, vkfwXdgWmBaseId;

static void
handle_wm_base_ping (void *data, xdg_wm_base *wm_base, uint32_t serial)
{
	(void) data;
	xdg_wm_base_pong (wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	.ping = handle_wm_base_ping
};

static void
handle_registry_global (void *data, wl_registry *registry,
	uint32_t name, const char *interface, uint32_t version)
{
	(void) data;
	(void) registry;
	(void) version;

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: interface %s v%u\n", interface, version);
	if (!strcmp (interface, "wl_compositor"))
		vkfwWlCompositorId = name;
	else if (!strcmp (interface, "xdg_wm_base"))
		vkfwXdgWmBaseId = name;
}

static void
handle_registry_global_remove (void *data, wl_registry *registry,
	uint32_t name)
{
	(void) data;
	(void) registry;
	(void) name;
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_registry_global,
	.global_remove = handle_registry_global_remove
};

static void
unload_wayland_funcs (void);

static void
vkfwWlClose (void)
{
	xdg_wm_base_destroy (vkfwXdgWmBase);
	wl_compositor_destroy (vkfwWlCompositor);
	wl_registry_destroy (vkfwWlRegistry);
	wl_display_disconnect (vkfwWlDisplay);
	unload_wayland_funcs ();
}

static bool
load_wayland_funcs (void);

static VkResult
vkfwWlOpen (void)
{
	if (!load_wayland_funcs ())
		return VK_ERROR_INITIALIZATION_FAILED;

	vkfwWlDisplay = wl_display_connect (nullptr);
	if (!vkfwWlDisplay) {
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Using Wayland backend\n");

	vkfwWlRegistry = wl_display_get_registry (vkfwWlDisplay);
	if (!vkfwWlRegistry) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_display_get_registry returned nullptr\n");
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	wl_registry_add_listener (vkfwWlRegistry, &registry_listener, nullptr);
	if (wl_display_roundtrip (vkfwWlDisplay) == -1) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_display_roundtrip failed\n");
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_compositor=%u\n", vkfwWlCompositorId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKWF: Wayland: xdg_wm_base=%u\n", vkfwXdgWmBaseId);

	if (!vkfwWlCompositorId || !vkfwXdgWmBaseId) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: required protocols are not supported\n");
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwWlCompositor = (wl_compositor *) wl_registry_bind (vkfwWlRegistry,
		vkfwWlCompositorId, &wl_compositor_interface, 6);
	vkfwXdgWmBase = (xdg_wm_base *) wl_registry_bind (vkfwWlRegistry,
		vkfwXdgWmBaseId, &xdg_wm_base_interface, 6);

	if (!vkfwWlCompositor || !vkfwXdgWmBase)  {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create required objects\n");
		if (vkfwWlCompositor)
			wl_compositor_destroy (vkfwWlCompositor);
		else if (vkfwXdgWmBase)
			xdg_wm_base_destroy (vkfwXdgWmBase);
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	xdg_wm_base_add_listener (vkfwXdgWmBase, &wm_base_listener, nullptr);
	if (wl_display_roundtrip (vkfwWlDisplay) == -1) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_display_roundtrip failed\n");
		xdg_wm_base_destroy (vkfwXdgWmBase);
		wl_compositor_destroy (vkfwWlCompositor);
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	return VK_SUCCESS;
}

VKFWwindowbackend vkfwBackendWayland = {
	.open_connection = vkfwWlOpen,
	.close_connection = vkfwWlClose,
	.request_instance_extensions = vkfwWlRequestExtensions,
	.alloc_window = vkfwWlAllocWindow,
	.create_window = vkfwWlCreateWindow,
	.destroy_window = vkfwWlDestroyWindow,
	.create_surface = vkfwWlCreateSurface,
	.query_present_support = vkfwWlQueryPresentSupport,
	.show_window = vkfwWlShowWindow,
	.hide_window = vkfwWlHideWindow,
	.dispatch_events = vkfwWlDispatchEvents
};

#define VKFW_WL_DEFINE_FUNC(name) PFN##name name;
VKFW_WL_ALL_FUNCS(VKFW_WL_DEFINE_FUNC)
#undef VKFW_WL_DEFINE_FUNC

static void *libwayland_handle;

static void
unload_wayland_funcs (void)
{
	vkfwCurrentPlatform->unloadModule (libwayland_handle);
}

static bool
load_wayland_funcs (void)
{
	libwayland_handle = vkfwCurrentPlatform->loadModule ("libwayland-client.so");
	if (!libwayland_handle)
		return false;

	bool failed = false;

#define VKFW_WL_LOAD_FUNC(sym)								\
	sym = (PFN##sym) vkfwCurrentPlatform->lookupSymbol (libwayland_handle, #sym);	\
	if (!sym)									\
		failed = true;
	VKFW_WL_ALL_FUNCS(VKFW_WL_LOAD_FUNC)
#undef VKFW_WL_LOAD_FUNC

	if (failed) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland backend failed to load some symbols\n");
		unload_wayland_funcs ();
		return false;
	}

	return true;
}
