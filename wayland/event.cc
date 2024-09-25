/**
 * Wayland event dispatching.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include "event.h"
#include "wayland.h"

#include <sys/poll.h>
#include <errno.h>

VkResult
vkfwWlDispatchEvents (int mode, uint64_t timeout)
{
	if (mode == VKFW_EVENT_MODE_TIMEOUT && timeout)
		timeout += vkfwGetTime ();

	/**
	 * FIXME: can we remove wl_display_roundtrip from this loop somehow?
	 */

	for (;;) {
		if (wl_display_roundtrip (vkfwWlDisplay) == -1)
			return VK_ERROR_UNKNOWN;

		if (!timeout || vkfwGetTime () >= timeout)
			return VK_SUCCESS;

		while (wl_display_prepare_read(vkfwWlDisplay) != 0)
			if (wl_display_dispatch_pending (vkfwWlDisplay) == -1)
				return VK_ERROR_UNKNOWN;

		if (wl_display_flush (vkfwWlDisplay) == -1) {
			wl_display_cancel_read (vkfwWlDisplay);
			if (errno == EAGAIN)
				continue;
			return VK_ERROR_UNKNOWN;
		}

		uint64_t now = vkfwGetTime ();
		if (now + 999 >= timeout)  {
			wl_display_cancel_read (vkfwWlDisplay);
			return VK_SUCCESS;
		}

		struct pollfd fds[] = {
			{ wl_display_get_fd (vkfwWlDisplay), POLLIN, 0 }
		};

		poll (fds, 1, (timeout - now + 999) / 1000);
		wl_display_cancel_read (vkfwWlDisplay);
		if (!(fds[0].revents & POLLIN))
			return VK_SUCCESS;
	}
}
