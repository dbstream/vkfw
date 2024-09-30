/**
 * Wayland globals.
 * Copyright (C) 2024  dbstream
 */
#ifndef VKFW_WAYLAND_H
#define VKFW_WAYLAND_H 1

#include "wayland_functions.h"
#include "wayland-protocol.h"
#include "viewporter-protocol.h"
#include "xdg-shell-protocol.h"
#include "zxdg-decoration-v1-protocol.h"

extern wl_display *vkfwWlDisplay;
extern wl_registry *vkfwWlRegistry;
extern wl_compositor *vkfwWlCompositor;
extern wl_subcompositor *vkfwWlSubcompositor;
extern wl_shm *vkfwWlShm;
extern wp_viewporter *vkfwWpViewporter;
extern xdg_wm_base *vkfwXdgWmBase;
extern zxdg_decoration_manager_v1 *vkfwZxdgDecorationManagerV1;

extern bool vkfwWlSupportCSD;

static constexpr int VKFW_WL_FRAME_SOURCE_WIDTH = 1;
static constexpr int VKFW_WL_FRAME_SOURCE_HEIGHT = 1;
extern wl_buffer *vkfwWlFrameBuffer;
extern wl_buffer *vkfwWlCursorBuffer;

#endif /* VKFW_WAYLAND_H */
