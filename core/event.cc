/**
 * VKFW events
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/event.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <VKFW/window.h>

extern "C"
VKFWAPI void
vkfwUnhandledEvent (VKFWevent *e)
{
	(void) e;
}

static VKFWeventhandler user_event_handler;
static void *user_event_pointer;

extern "C"
VKFWAPI VKFWeventhandler
vkfwSetEventHandler (VKFWeventhandler handler, void *user)
{
	VKFWeventhandler old = user_event_handler;
	user_event_handler = handler;
	user_event_pointer = user;
	return old;
}

void
vkfwSendEventToApplication (VKFWevent *e)
{
	switch (e->type) {
	case VKFW_EVENT_WINDOW_RESIZE_NOTIFY:
		/**
		 * Keep window->extent up to date.
		 */
		e->window->extent = e->extent;
		break;
	case VKFW_EVENT_KEY_PRESSED:
	case VKFW_EVENT_KEY_RELEASED:
		e->key = vkfwTranslateKeycode (e->keycode);
		break;
	}

	if (user_event_handler)
		user_event_handler (e, user_event_pointer);
}

static VkResult
get_compat_event (VKFWevent *e, uint64_t deadline);

extern "C"
VKFWAPI VkResult
vkfwDispatchEvents (int mode, uint64_t timeout)
{
	if (mode == VKFW_EVENT_MODE_POLL)
		timeout = 0;

	if (vkfwCurrentWindowBackend->dispatch_events)
		return vkfwCurrentWindowBackend->dispatch_events (mode, timeout);

	if (timeout && mode == VKFW_EVENT_MODE_TIMEOUT)
		timeout += vkfwGetTime ();

	VkResult result;
	VKFWevent e;
	for (;;) {
		for (;;) {
			e.type = VKFW_EVENT_NONE;
			e.window = nullptr;

			result = get_compat_event (&e, timeout);
			if (result != VK_SUCCESS)
				return result;

			if (e.type == VKFW_EVENT_NONE)
				break;
			if (e.type == VKFW_EVENT_NULL)
				continue;
			vkfwSendEventToApplication (&e);
		}

		if (!timeout || vkfwGetTime () >= timeout)
			return VK_SUCCESS;
	}
}

static VKFWwindow *text_input_window;
static uint32_t text_input_codepoint;
static int text_input_x, text_input_y;
static unsigned int text_input_mods;

void
vkfwCleanupEvents (void)
{
	user_event_handler = nullptr;

	if (text_input_window) {
		vkfwUnrefWindow (text_input_window);
		text_input_window = nullptr;
	}
}

void
vkfwQueueTextInputEvent (VKFWwindow *window, uint32_t codepoint,
	int x, int y, unsigned int mods)
{
	if (!(window->flags & VKFW_WINDOW_TEXT_INPUT_ENABLED))
		return;

	if (text_input_window)
		vkfwUnrefWindow (text_input_window);

	vkfwRefWindow (window);
	text_input_codepoint = codepoint;
	text_input_x = x;
	text_input_y = y;
	text_input_mods = mods;
	text_input_window = window;
}

extern "C"
VKFWAPI void
vkfwDisableTextInput (VKFWwindow *handle)
{
	handle->flags &= ~VKFW_WINDOW_TEXT_INPUT_ENABLED;
	if (text_input_window == handle) {
		vkfwUnrefWindow (text_input_window);
		text_input_window = nullptr;
	}
}

extern "C"
VKFWAPI void
vkfwEnableTextInput (VKFWwindow *handle)
{
	handle->flags |= VKFW_WINDOW_TEXT_INPUT_ENABLED;
}

static bool
get_queued_event (VKFWevent *e)
{
	if (text_input_window) {
		if (text_input_window->flags & VKFW_WINDOW_DELETED) {
			vkfwUnrefWindow (text_input_window);
			text_input_window = nullptr;
		} else {
			e->window = text_input_window;
			e->type = VKFW_EVENT_TEXT_INPUT;
			e->x = text_input_x;
			e->y = text_input_y;
			e->codepoint = text_input_codepoint;
			e->modifiers = text_input_mods;
			vkfwUnrefWindow (text_input_window);
			text_input_window = nullptr;
			return true;
		}
	}

	return false;
}

static VkResult
get_compat_event (VKFWevent *e, uint64_t deadline)
{
	if (get_queued_event (e))
		return VK_SUCCESS;

	if (vkfwCurrentWindowBackend->get_event)
		return vkfwCurrentWindowBackend->get_event (e, VKFW_EVENT_MODE_DEADLINE, deadline);

	vkfwDelayUntil (deadline);
	return VK_SUCCESS;
}
