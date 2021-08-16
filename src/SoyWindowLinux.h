#pragma once

#if defined(ENABLE_OPENGL)
#include "LinuxDRM/esUtil.h"
#endif

class Platform::TWindow : public SoyWindow
{
public:
	TWindow( const std::string& Name, Soy::Rectx<int32_t>& Rect );
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual void									SetFullscreen(bool Fullscreen) override;
	virtual bool									IsFullscreen() override;
	virtual bool									IsMinimised() override;
	virtual bool									IsForeground() override;
	virtual void									EnableScrollBars(bool Horz,bool Vert) override;

#if defined(ENABLE_OPENGL)
	ESContext											mESContext;
#endif
};