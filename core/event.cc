/**
 * VKFW events
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <VKFW/window.h>

extern "C"
VKFWAPI void
vkfwUnhandledEvent (VKFWevent *e)
{
	(void) e;
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
