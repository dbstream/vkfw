/**
 * Make sure XCB prototypes are correct.
 * Copyright (C) 2024  dbstream
 */
#include <type_traits>
#define VKFW_VERIFY_XCB_PROTOS(name) \
static_assert (std::is_same_v<decltype (&name), PFN##name>);
#include "xcb.h"
