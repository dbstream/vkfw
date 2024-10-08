/**
 * Wayland backend initialization.
 * Copyright (C) 2024  dbstream
 */
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#include <VKFW/logging.h>
#include <VKFW/options.h>
#include <VKFW/platform.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include "event.h"
#include "input.h"
#include "wayland.h"
#include "window.h"

#include "csd_close_button.cc"
#include "default_cursor.cc"

#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

wl_display *vkfwWlDisplay;
wl_registry *vkfwWlRegistry;
wl_compositor *vkfwWlCompositor;
wl_subcompositor *vkfwWlSubcompositor;
wl_shm *vkfwWlShm;
wp_viewporter *vkfwWpViewporter;
xdg_wm_base *vkfwXdgWmBase;
zxdg_decoration_manager_v1 *vkfwZxdgDecorationManagerV1;

bool vkfwWlSupportCSD;

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
	surface_ci.surface = w->content_surface;

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

static uint32_t vkfwWlCompositorId;
static uint32_t vkfwWlSeatId;
static uint32_t vkfwWlSubcompositorId;
static uint32_t vkfwWlShmId;
static uint32_t vkfwWpViewporterId;
static uint32_t vkfwXdgWmBaseId;
static uint32_t vkfwZxdgDecorationManagerV1Id;

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

	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: interface %s v%u <%u>\n", interface, version, name);
	if (!strcmp (interface, "wl_compositor"))
		vkfwWlCompositorId = name;
	else if (!strcmp (interface, "wl_seat"))
		vkfwWlSeatId = name;
	else if (!strcmp (interface, "wl_subcompositor"))
		vkfwWlSubcompositorId = name;
	else if (!strcmp (interface, "wl_shm"))
		vkfwWlShmId = name;
	else if (!strcmp (interface, "wp_viewporter"))
		vkfwWpViewporterId = name;
	else if (!strcmp (interface, "xdg_wm_base"))
		vkfwXdgWmBaseId = name;
	else if (!strcmp (interface, "zxdg_decoration_manager_v1"))
		vkfwZxdgDecorationManagerV1Id = name;
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

wl_buffer *vkfwWlFrameBuffer;
wl_buffer *vkfwWlCursorBuffer;
wl_buffer *vkfwWlCloseButtonBuffer;

static constexpr size_t CSD_BUFFER_SIZE = 0x2000;

static uint8_t csd_buffer_data[CSD_BUFFER_SIZE] = {
	// offset 0x0000: single pixel ARGB8888 buffer for window frame
	0xcf, 0xcf, 0xc0, 0xff

	// offset 0x0040: 24x24 ARGB8888 buffer for cursor
	// offset 0x0940: 15x15 ARGB8888 buffer for CSD close button
};

/**
 * Copy pixels from an 8-bit RGBA image into a wayland ARGB8888 image.
 */
static void
copy_rgba_to_argb (uint8_t *dst, const uint8_t *src, size_t num_pixels)
{
	for (; num_pixels; num_pixels--) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];

		dst += 4;
		src += 4;
	}
}

static bool
setup_shm (void)
{
	static_assert (default_cursor.width == 24);
	static_assert (default_cursor.height == 24);
	static_assert (default_cursor.bytes_per_pixel == 4);
	copy_rgba_to_argb (&csd_buffer_data[0x40], default_cursor.pixel_data, 24 * 24);

	static_assert (csd_close_button.width == 15);
	static_assert (csd_close_button.height == 15);
	static_assert (csd_close_button.bytes_per_pixel == 4);
	copy_rgba_to_argb (&csd_buffer_data[0x940], csd_close_button.pixel_data, 15 * 15);

	int fd = memfd_create ("vkfw_wayland_shm", MFD_ALLOW_SEALING);
	if (fd == -1)
		return false;

	if (ftruncate (fd, CSD_BUFFER_SIZE) == -1) {
		close (fd);
		return false;
	}

	void *addr = mmap (nullptr, CSD_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		close (fd);
		return false;
	}

	memcpy (addr, csd_buffer_data, CSD_BUFFER_SIZE);
	munmap (addr, CSD_BUFFER_SIZE);

	wl_shm_pool *pool = wl_shm_create_pool (vkfwWlShm, fd, CSD_BUFFER_SIZE);
	close (fd);
	if (!pool)
		return false;

	vkfwWlFrameBuffer = wl_shm_pool_create_buffer (pool, 0,
		VKFW_WL_FRAME_SOURCE_WIDTH, VKFW_WL_FRAME_SOURCE_HEIGHT, 4, WL_SHM_FORMAT_ARGB8888);
	vkfwWlCursorBuffer = wl_shm_pool_create_buffer (pool, 0x40,
		24, 24, 4 * 24, WL_SHM_FORMAT_ARGB8888);
	vkfwWlCloseButtonBuffer = wl_shm_pool_create_buffer (pool, 0x940,
		15, 15, 4 * 15, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy (pool);
	if (vkfwWlFrameBuffer && vkfwWlCursorBuffer && vkfwWlCloseButtonBuffer)
		return true;
	if (vkfwWlFrameBuffer)
		wl_buffer_destroy (vkfwWlFrameBuffer);
	if (vkfwWlCursorBuffer)
		wl_buffer_destroy (vkfwWlCursorBuffer);
	if (vkfwWlCloseButtonBuffer)
		wl_buffer_destroy (vkfwWlCloseButtonBuffer);
	return false;
}

static void
vkfwWlClose (void)
{
	vkfwWlTerminateInput ();
	wl_buffer_destroy (vkfwWlCloseButtonBuffer);
	wl_buffer_destroy (vkfwWlCursorBuffer);
	wl_buffer_destroy (vkfwWlFrameBuffer);
	if (vkfwWpViewporter)
		wp_viewporter_destroy (vkfwWpViewporter);
	if (vkfwWlSubcompositor)
		wl_subcompositor_destroy (vkfwWlSubcompositor);
	if (vkfwZxdgDecorationManagerV1)
		zxdg_decoration_manager_v1_destroy (vkfwZxdgDecorationManagerV1);
	wl_shm_destroy (vkfwWlShm);
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
	if (!vkfwGetBool ("enable_wayland"))
		return VK_ERROR_INITIALIZATION_FAILED;

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
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_seat=%u\n", vkfwWlSeatId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_subcompositor=%u\n", vkfwWlSubcompositorId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_shm=%u\n", vkfwWlShmId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wp_viewporter=%u\n", vkfwWpViewporterId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKWF: Wayland: xdg_wm_base=%u\n", vkfwXdgWmBaseId);
	vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: zxdg_decoration_manager_v1=%u\n", vkfwZxdgDecorationManagerV1Id);

	if (!vkfwWlCompositorId || !vkfwXdgWmBaseId || !vkfwWlShmId) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: required protocols are not supported\n");
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	vkfwWlCompositor = (wl_compositor *) wl_registry_bind (vkfwWlRegistry,
		vkfwWlCompositorId, &wl_compositor_interface, 5);
	vkfwXdgWmBase = (xdg_wm_base *) wl_registry_bind (vkfwWlRegistry,
		vkfwXdgWmBaseId, &xdg_wm_base_interface, 5);

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

	if (vkfwGetBool ("wl_disable_ssd"))
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: zxdg_decoration_manager_v1 disabled by library options\n");
	else if (vkfwZxdgDecorationManagerV1Id) {
		vkfwZxdgDecorationManagerV1 = (zxdg_decoration_manager_v1 *) wl_registry_bind (
			vkfwWlRegistry, vkfwZxdgDecorationManagerV1Id, &zxdg_decoration_manager_v1_interface, 1);
		if (!vkfwZxdgDecorationManagerV1)
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create decoration manager; only CSD can be used\n");
	} else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: zxdg_decoration_manager_v1 is unavailable; only CSD can be used\n");

	if (vkfwWlSubcompositorId) {
		vkfwWlSubcompositor = (wl_subcompositor *) wl_registry_bind (
			vkfwWlRegistry, vkfwWlSubcompositorId, &wl_subcompositor_interface, 1);
		if (!vkfwWlSubcompositor)
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create subcompositor; CSD will be disabled\n");
	} else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_subcompositor not available; CSD will be disabled\n");

	if (vkfwWlShmId) {
		vkfwWlShm = (wl_shm *) wl_registry_bind (vkfwWlRegistry,
			vkfwWlShmId, &wl_shm_interface, 1);
		if (!vkfwWlShm)
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create shm; CSD will be disabled\n");
	} else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_shm is not available; CSD will be disabled\n");

	if (vkfwWpViewporterId) {
		vkfwWpViewporter = (wp_viewporter *) wl_registry_bind (
			vkfwWlRegistry, vkfwWpViewporterId, &wp_viewporter_interface, 1);
		if (!vkfwWpViewporter)
			vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to create viewporter; CSD will be disabled\n");
	} else
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wp_viewporter is not available; CSD will be disabled\n");

	xdg_wm_base_add_listener (vkfwXdgWmBase, &wm_base_listener, nullptr);
	if (wl_display_roundtrip (vkfwWlDisplay) == -1) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: wl_display_roundtrip failed\n");
		if (vkfwWpViewporter)
			wp_viewporter_destroy (vkfwWpViewporter);
		if (vkfwWlShm)
			wl_shm_destroy (vkfwWlShm);
		if (vkfwWlSubcompositor)
			wl_subcompositor_destroy (vkfwWlSubcompositor);
		if (vkfwZxdgDecorationManagerV1)
			zxdg_decoration_manager_v1_destroy (vkfwZxdgDecorationManagerV1);
		xdg_wm_base_destroy (vkfwXdgWmBase);
		wl_compositor_destroy (vkfwWlCompositor);
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (!setup_shm ()) {
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: failed to send buffers to the compositor\n");
		if (vkfwWlSupportCSD)
			wl_buffer_destroy (vkfwWlFrameBuffer);
		if (vkfwWpViewporter)
			wp_viewporter_destroy (vkfwWpViewporter);
		if (vkfwWlSubcompositor)
			wl_subcompositor_destroy (vkfwWlSubcompositor);
		if (vkfwZxdgDecorationManagerV1)
			zxdg_decoration_manager_v1_destroy (vkfwZxdgDecorationManagerV1);
		wl_shm_destroy (vkfwWlShm);
		xdg_wm_base_destroy (vkfwXdgWmBase);
		wl_compositor_destroy (vkfwWlCompositor);
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
	}

	if (vkfwWlSubcompositor && vkfwWpViewporter) {
		vkfwWlSupportCSD = true;
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: client-side decorations are supported\n");
	}

	if (vkfwZxdgDecorationManagerV1)
		vkfwPrintf (VKFW_LOG_BACKEND, "VKFW: Wayland: zxdg_decoration_manager_v1 is supported\n");

	VkResult result = vkfwWlInitializeInput (vkfwWlSeatId);
	if (result != VK_SUCCESS) {
		if (vkfwWlSupportCSD)
			wl_buffer_destroy (vkfwWlFrameBuffer);
		if (vkfwWpViewporter)
			wp_viewporter_destroy (vkfwWpViewporter);
		if (vkfwWlSubcompositor)
			wl_subcompositor_destroy (vkfwWlSubcompositor);
		if (vkfwZxdgDecorationManagerV1)
			zxdg_decoration_manager_v1_destroy (vkfwZxdgDecorationManagerV1);
		wl_shm_destroy (vkfwWlShm);
		xdg_wm_base_destroy (vkfwXdgWmBase);
		wl_compositor_destroy (vkfwWlCompositor);
		wl_registry_destroy (vkfwWlRegistry);
		wl_display_disconnect (vkfwWlDisplay);
		unload_wayland_funcs ();
		return result;
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
	.set_title = vkfwWlSetWindowTitle,
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
