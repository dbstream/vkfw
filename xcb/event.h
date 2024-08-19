/**
 * XCB event handling
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <xcb/xcb.h>

VkResult
vkfwXcbGetEvent (VKFWevent *e, int mode, uint64_t timeout);
