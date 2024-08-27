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

static VKFWwindow *text_input_window;
static uint32_t text_input_codepoint;
static int text_input_x, text_input_y;
static unsigned int text_input_mods;

void
vkfwCleanupEvents (void)
{
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

static void
after_event_received (VKFWevent *e, VkResult result)
{
	if (result != VK_SUCCESS)
		return;

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
}

extern "C"
VKFWAPI VkResult
vkfwGetNextEvent (VKFWevent *e)
{
	if (get_queued_event (e))
		return VK_SUCCESS;

	VkResult result = VK_SUCCESS;
	e->type = VKFW_EVENT_NONE;
	e->window = nullptr;
	if (vkfwCurrentWindowBackend->get_event) {
		result = vkfwCurrentWindowBackend->get_event (e, VKFW_EVENT_MODE_POLL, 0);
		after_event_received (e, result);
	}
	return result;
}

extern "C"
VKFWAPI VkResult
vkfwWaitNextEvent (VKFWevent *e, uint64_t timeout)
{
	if (!timeout)
		return vkfwGetNextEvent (e);

	if (get_queued_event (e))
		return VK_SUCCESS;

	VkResult result = VK_SUCCESS;
	e->type = VKFW_EVENT_NONE;
	e->window = nullptr;
	if (vkfwCurrentWindowBackend->get_event) {
		result = vkfwCurrentWindowBackend->get_event (e, VKFW_EVENT_MODE_TIMEOUT, timeout);
		after_event_received (e, result);
	} else
		vkfwDelay (timeout);
	return result;
}

extern "C"
VKFWAPI VkResult
vkfwWaitNextEventUntil (VKFWevent *e, uint64_t deadline)
{
	if (!deadline)
		return vkfwGetNextEvent (e);

	if (get_queued_event (e))
		return VK_SUCCESS;

	VkResult result = VK_SUCCESS;
	e->type = VKFW_EVENT_NONE;
	e->window = nullptr;
	if (vkfwCurrentWindowBackend->get_event) {
		result = vkfwCurrentWindowBackend->get_event (e, VKFW_EVENT_MODE_DEADLINE, deadline);
		after_event_received (e, result);
	} else
		vkfwDelayUntil (deadline);
	return result;
}
