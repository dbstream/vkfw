/**
 * Properly include <Windows.h>.
 * Copyright (C) 2024  dbstream
 */

#ifndef VKFW_WIN32_H
#define VKFW_WIN32_H 1

/**
 * NOTE: include it as <windows.h>. On real Windows systems, the file name is
 * capitalized, but on mingw it is not. It doesn't actually matter on Windows,
 * as file names aren't case-sensitive by default.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern HINSTANCE vkfwHInstance;

#endif /* VKFW_WIN32_H */
