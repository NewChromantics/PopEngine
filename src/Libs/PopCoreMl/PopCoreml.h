#pragma once

#if defined(TARGET_OSX)
#import <Cocoa/Cocoa.h>

//! Project version number for PopCoreml.
FOUNDATION_EXPORT double PopCoremlVersionNumber;

//! Project version string for PopCoreml.
FOUNDATION_EXPORT const unsigned char PopCoremlVersionString[];

#endif

#include <stdint.h>


#if defined(_MSC_VER) && !defined(TARGET_PS4)
#define __export			extern "C" __declspec(dllexport)
#else
#define __export			extern "C"
#endif

__export int32_t			PopCoreml_GetVersion();
