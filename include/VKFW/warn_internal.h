/**
 * Warn on inclusion when VKFW_BUILDING is not defined.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */

#ifndef VKFW_BUILDING
#warn VKFW: an internal header file was included but VKFW_BUILDING is not defined
#endif
