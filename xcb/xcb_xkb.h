/**
 * libxcb-xkb functions
 * Copyright (C) 2024  dbstream
 */
#ifndef VKFW_XCB_XKB_H
#define VKFW_XCB_XKB_H 1

/**
 * Workaround a bug in XCB that affects C++ compatibility wrt the xkb header.
 *
 * The xkb header uses 'explicit' as a member name for a struct. Despite the
 * 'extern "C"', struct members cannot be named 'explicit', as it is a keyword
 * in C++. Work around it by defining it to be something else which is not a
 * keyword.
 */
#ifdef explicit
/** undefining 'explicit' should not matter, as we do not use it ourselves. */
#warning "'explicit' is already defined. Undefining it"
#undef explicit
#endif
#define explicit explicit_
#include <xcb/xkb.h>
#undef explicit

#define VKFW_XCB_XKB_ALL_FUNCS(macro)		\
macro(xcb_xkb_select_events_aux)

#define VKFW_XCB_XKB_DEFINE_FUNC(name)	\
typedef decltype(&name) PFN##name;	\
extern PFN##name vkfw_##name;
VKFW_XCB_XKB_ALL_FUNCS(VKFW_XCB_XKB_DEFINE_FUNC)
#undef VKFW_XCB_XKB_DEFINE_FUNC

#define xcb_xkb_select_events_aux vkfw_xcb_xkb_select_events_aux

#endif /* VKFW_XCB_XKB_H */
