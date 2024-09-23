/**
 * Wayland globals.
 * Copyright (C) 2024  dbstream
 */
#ifndef VKFW_WAYLAND_H
#define VKFW_WAYLAND_H 1

#include "wayland_functions.h"
#include "wayland-protocol.h"
#include "xdg-shell-protocol.h"

extern wl_display *vkfwWlDisplay;
extern wl_registry *vkfwWlRegistry;
extern wl_compositor *vkfwWlCompositor;
extern xdg_wm_base *vkfwXdgWmBase;

#endif /* VKFW_WAYLAND_H */
