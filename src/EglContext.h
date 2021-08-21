#pragma once

#include <string>
#include <stdint.h>


namespace Egl
{
	void		IsOkay(const char* Context);
	std::string	GetString(int);//EGLint);

	class TDisplaySurfaceContext;
	class TParams;
}

class Egl::TParams
{
public:
	//	screen settings
	//	width/height of 0 = dont change resolution
	//	todo: window rect, so we can offset for TVs
	uint32_t	WindowWidth = 0;
	uint32_t	WindowHeight = 0;
	uint32_t	DisplayIndex = 0;
	uint32_t	WindowOffsetX = 0;
	uint32_t	WindowOffsetY = 0;
	uint32_t	DisplayWidth = 0;
	uint32_t	DisplayHeight = 0;

	//	opengl context settings
	uint32_t	ScreenBufferCount = 0;	//	0=dont set, 1=single buffer,2=double buffering, 3=triple buffer
	uint32_t	DepthBits = 8;
	uint32_t	StencilBits = 0;
	uint32_t	OpenglEsVersion = 2;
	uint32_t	MsaaSamples = 0;
};

//	currently all wrapped into one object, we don't currently need to split it
class Egl::TDisplaySurfaceContext
{
public:
    TDisplaySurfaceContext(TParams Params=TParams());
    ~TDisplaySurfaceContext();

    void    PrePaint();
    void    PostPaint();

    void    TestRender();
    void    GetDisplaySize(uint32_t& Width,uint32_t& Height);
};

