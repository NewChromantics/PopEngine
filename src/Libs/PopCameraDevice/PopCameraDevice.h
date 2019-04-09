#pragma once

//	if you're using this header to link to the DLL, you'll probbaly need the lib :)
//#pragma comment(lib, "PopCameraDevice.lib")

#include <stdint.h>


#if !defined(__export)

#if defined(_MSC_VER) && !defined(TARGET_PS4)
#define __export			extern "C" __declspec(dllexport)
#else
#define __export			extern "C"
#endif

#endif


__export void				EnumCameraDevices(char* StringBuffer,int32_t StringBufferLength);
__export int32_t			CreateCameraDevice(const char* Name);
__export void				FreeCameraDevice(int32_t Instance);
__export void				GetMeta(int32_t Instance,int32_t* MetaValues,int32_t MetaValuesCount);
__export int32_t			PopFrame(int32_t Instance,uint8_t* Plane0,int32_t Plane0Size,uint8_t* Plane1,int32_t Plane1Size,uint8_t* Plane2,int32_t Plane2Size);


__export const char*		PopDebugString();
__export void				ReleaseDebugString(const char* String);
