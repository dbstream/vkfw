/** 
 * Win32 event handling.
 * Copyright (C) 2024  dbstream
 */

#include <VKFW/vkfw.h>
#include "win32.h"

LRESULT CALLBACK
vkfwWin32WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

VkResult
vkfwWin32GetEvent (VKFWevent *e, int mode, uint64_t timeout);

VkResult
vkfwWin32InitEvents (void);

void
vkfwWin32TerminateEvents (void);
