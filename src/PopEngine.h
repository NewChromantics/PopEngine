#pragma once

#if defined(TARGET_WINDOWS)
	#define EXPORT	__declspec(dllexport)
#else
	#define EXPORT
#endif

extern "C" EXPORT int PopEngine(const char* ProjectPath);
