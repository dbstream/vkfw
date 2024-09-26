#!/bin/sh
# Use wayland-scanner to generate definitions for Wayland protocols.

set -e

wayland-scanner client-header /usr/share/wayland/wayland.xml wayland-protocol.h
wayland-scanner private-code /usr/share/wayland/wayland.xml wayland-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml zxdg-decoration-v1-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml zxdg-decoration-v1-protocol.c
