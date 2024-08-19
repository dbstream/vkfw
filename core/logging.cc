/**
 * Message logging.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/vkfw.h>
#include <stdarg.h>
#include <stdio.h>

static bool logging_enabled[VKFW_LOG_NUM];

extern "C"
VKFWAPI void
vkfwEnableDebugLogging (int source)
{
	if (source >= 0 && source < VKFW_LOG_NUM)
		logging_enabled[source] = true;
	else if(source == VKFW_LOG_ALL || source == VKFW_LOG_NONE)
		for (int i = 0; i < VKFW_LOG_NUM; i++)
			logging_enabled[i] = source == VKFW_LOG_ALL;
}

void
vkfwPrintf (int source, const char *msg, ...)
{
	if (!logging_enabled[source])
		return;

	va_list args;
	va_start (args, msg);
	vprintf (msg, args);
	va_end (args);
}
