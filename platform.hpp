#ifndef PLATFORM_H
#define PLATFORM_H

// WINDOWS
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR    1
#include <windows.h>

// LINUX ( Via XCB library )
#elif defined(__linux)
#define VK_USE_PLATFORM_XCB_KHR    1
#include <xcb/xcb.h>

#else
#error Platform not yet supported
#endif

#include <vulkan.h>

#endif // PLATFORM_H
