#pragma once

#if defined(TARGET_OSX) && defined(__OBJC__)
#import <Cocoa/Cocoa.h>

//! Project version number for PopCoreml.
//FOUNDATION_EXPORT double PopCoremlVersionNumber;

//! Project version string for PopCoreml.
//FOUNDATION_EXPORT const unsigned char PopCoremlVersionString[];

#endif

#include <stdint.h>


#if defined(_MSC_VER) && !defined(TARGET_PS4)
#define __export			extern "C" __declspec(dllexport)
#define __exportclass		 __declspec(dllexport)
#else
#define __export			extern "C"
#define __exportclass
#endif

__export int32_t			PopCoreml_GetVersion();

#define POPML_KINECT_GPUID_CPU				(-1)
#define POPML_KINECT_GPUID_DEFAULT			(0)
#define POPML_KINECT_TRACKMODE_WIDEDEPTH	(0)
#define POPML_KINECT_TRACKMODE_WIDECAMERA	(1)
#define POPML_KINECT_TRACKMODE_NARROWDEPTH	(2)
#define POPML_KINECT_TRACKMODE_NARROWCAMERA	(3)
#define POPML_KINECT_TRACKMODE_NARROWSMALLDEPTH	(4)
#define POPML_KINECT_TRACKMODE_NARROWSMALLCAMERA	(5)

//	1.1.3
//		Added SetKinectTrackMode(), as before.
//	1.1.2
//		Added SetKinectGpu() to base class (until we a have better options API)
//	1.1.1
//		added uint64_t timestamp (ms) CoreMl::TWorldObject (possibly non-breaking as signatures haven't changed)


