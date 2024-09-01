# Cross-compilation for Win32 using mingw.
set (CMAKE_SYSTEM_NAME Windows)
set (CMAKE_SYSTEM_PROCESSOR x86_64)
set (CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set (CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set (CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /usr/share/cmake/mingw-w64)
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set (CMAKE_C_FLAGS -static)
set (CMAKE_CXX_FLAGS -static)
