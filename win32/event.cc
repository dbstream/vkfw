/**
 * Win32 event handling.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include "event.h"
#include "win32.h"

LRESULT CALLBACK
vkfwWin32WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	return DefWindowProcW (hwnd, msg, wparam, lparam);
}
