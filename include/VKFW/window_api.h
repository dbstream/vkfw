/**
 * VKFW window creation API functions.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */
#include <VKFW/warn_internal.h>

#ifndef VKFW_WINDOW_API_H
#define VKFW_WINDOW_API_H 1

#include <VKFW/vkfw.h>

#define VKFW_EVENT_MODE_POLL 0
#define VKFW_EVENT_MODE_TIMEOUT 1
#define VKFW_EVENT_MODE_DEADLINE 2

struct VKFWwindowbackend_T {
	VkResult (*open_connection) (void);
	void (*close_connection) (void);

	VkResult (*request_instance_extensions) (void);

	/**
	 * malloc and free a platform-specific VKFWwindow. Backend may
	 * initialize private variables as well.
	 * note: if free_window is nullptr, stdlib free will be used.
	 */
	VKFWwindow *(*alloc_window) (void);
	void (*free_window) (VKFWwindow *);

	/**
	 * Create a platform-specific VKFWwindow. Generic fields are already
	 * filled-in.
	 */
	VkResult (*create_window) (VKFWwindow *);
	void (*destroy_window) (VKFWwindow *);

	VkResult (*create_surface) (VKFWwindow *, VkSurfaceKHR *);

	VkResult (*query_present_support) (VkPhysicalDevice, uint32_t, VkBool32 *);

	VkResult (*show_window) (VKFWwindow *);
	VkResult (*hide_window) (VKFWwindow *);

	VkResult (*set_title) (VKFWwindow *, const char *);

	/**
	 * Generic handler for retrieving events. The second argument is one of
	 * VKFW_EVENT_MODE_POLL, VKFW_EVENT_MODE_TIMEOUT and
	 * VKFW_EVENT_MODE_DEADLINE.
	 */
	VkResult (*get_event) (VKFWevent *, int, uint64_t);

	int (*translate_keycode) (int);
	int (*translate_key) (int);
};

extern VKFWwindowbackend *vkfwCurrentWindowBackend;

#endif /* VKFW_WINDOW_API_H */
