/**
 * Core window functions.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <VKFW/window.h>
#include <VKFW/window_api.h>
#include <stdlib.h>

void
vkfwRefWindow (VKFWwindow *window)
{
	window->internal_refcnt++;
}

void
vkfwUnrefWindow (VKFWwindow *window)
{
	if (--window->internal_refcnt)
		return;

	if (vkfwCurrentWindowBackend->free_window)
		vkfwCurrentWindowBackend->free_window (window);
	else
		free (window);
}

extern "C"
VKFWAPI VkResult
vkfwCreateWindow (VKFWwindow **handle, VkExtent2D size)
{
	VKFWwindow *w;
	if (vkfwCurrentWindowBackend->alloc_window)
		w = vkfwCurrentWindowBackend->alloc_window ();
	else
		w = (VKFWwindow *) malloc (sizeof (VKFWwindow));

	if (!w)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	w->internal_refcnt = 1;
	w->flags = 0;
	w->pointer_flags = 0;
	w->extent = size;
	VkResult result = vkfwCurrentWindowBackend->create_window (w);
	if (result != VK_SUCCESS) {
		vkfwUnrefWindow (w);
		return result;
	}

	*handle = w;
	return VK_SUCCESS;
}

extern "C"
VKFWAPI void
vkfwDestroyWindow (VKFWwindow *handle)
{
	handle->flags |= VKFW_WINDOW_DELETED;
	vkfwCurrentWindowBackend->destroy_window (handle);
	vkfwUnrefWindow (handle);
}

extern "C"
VKFWAPI VkResult
vkfwCreateSurface (VKFWwindow *handle, VkSurfaceKHR *out)
{
	if (vkfwCurrentWindowBackend->create_surface)
		return vkfwCurrentWindowBackend->create_surface (handle, out);
	return VK_ERROR_UNKNOWN;
}

extern "C"
VKFWAPI void *
vkfwSetWindowUserPointer (VKFWwindow *handle, void *user)
{
	void *ret = handle->user;
	handle->user = user;
	return ret;
}

extern "C"
VKFWAPI void *
vkfwGetWindowUserPointer (VKFWwindow *handle)
{
	return handle->user;
}

extern "C"
VKFWAPI VkExtent2D
vkfwGetFramebufferExtent (VKFWwindow *handle)
{
	return handle->extent;
}

extern "C"
VKFWAPI VkResult
vkfwSetWindowTitle (VKFWwindow *handle, const char *title)
{
	if (vkfwCurrentWindowBackend->set_title)
		return vkfwCurrentWindowBackend->set_title (handle, title);
	return VK_SUCCESS;
}

extern "C"
VKFWAPI VkResult
vkfwShowWindow (VKFWwindow *handle)
{
	if (vkfwCurrentWindowBackend->show_window)
		return vkfwCurrentWindowBackend->show_window (handle);
	return VK_SUCCESS;
}

extern "C"
VKFWAPI VkResult
vkfwHideWindow (VKFWwindow *handle)
{
	if (vkfwCurrentWindowBackend->hide_window)
		return vkfwCurrentWindowBackend->hide_window (handle);
	return VK_SUCCESS;
}

extern "C"
VKFWAPI void
vkfwSetPointerMode (VKFWwindow *handle, unsigned int mode)
{
	handle->pointer_flags = mode;
	if (vkfwCurrentWindowBackend->update_pointer_mode)
		vkfwCurrentWindowBackend->update_pointer_mode (handle);
}
