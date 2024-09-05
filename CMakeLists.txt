# Copyright (C) 2024  dbstream

# As of writing this line of code, I have CMake 3.30 installed on my machine. To
# avoid any potential problems, simply require CMake 3.30.
cmake_minimum_required (VERSION 3.30)

# VKFW intentionally has a C-like API, but is written in C++.
project (vkfw LANGUAGES CXX)

# Use C++17, as in my experience, C++20 and newer can cause linter issues.
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_library (vkfw)

find_package (Vulkan)
target_include_directories (vkfw PUBLIC ${Vulkan_INCLUDE_DIRS})

target_compile_definitions (vkfw PRIVATE -DVKFW_BUILDING)

if (BUILD_SHARED_LIBS)
	target_compile_definitions (vkfw PUBLIC -DVKFW_DLL)
endif ()

target_include_directories (vkfw PUBLIC include)

# Generic sources.
target_sources (vkfw PRIVATE
	"core/event.cc"
	"core/keyboard.cc"
	"core/logging.cc"
	"core/vk_context.cc"
	"core/vk_functions.cc"
	"core/vkfw.cc"
	"core/window.cc"
)

set (VKFW_WANT_XCB OFF)

# Platform-specific sources.
if (UNIX)
	set (VKFW_WANT_XCB ON)
	target_sources (vkfw PRIVATE
		"unix/platform.cc"
	)
endif ()
if (WIN32)
	target_compile_definitions (vkfw PRIVATE -DVKFW_USE_WIN32)
	target_sources (vkfw PRIVATE
		"win32/backend.cc"
		"win32/event.cc"
		"win32/platform.cc"
		"win32/utils.cc"
		"win32/window.cc"
	)
endif ()

if (VKFW_WANT_XCB)
	target_compile_definitions (vkfw PRIVATE -DVKFW_USE_XCB)
	target_sources (vkfw PRIVATE
		"xcb/connection.cc"
		"xcb/event.cc"
		"xcb/keyboard.cc"
		"xcb/window.cc"
	)
endif ()

if (PROJECT_IS_TOP_LEVEL)
	add_executable (example)

	# Make sure users of VKFW can compile without any warnings.
	if (MSVC)
		# FIXME: VKFWevent contains unnamed anonymous unions and
		# structs. This is perfectly fine on GCC, but breaks MSVC.
		#
		# Mitigate this by compiling with /W3. I'm not sure if this
		# fixes the problem.
		target_compile_options (example PRIVATE /W3 /WX)
	else ()
		target_compile_options (example PRIVATE -Wall -Wextra -Werror)
	endif ()

	target_link_libraries (example vkfw)

	target_sources (example PRIVATE "example.cc")
endif ()

