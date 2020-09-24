#pragma once
#include "LinuxDRM/esUtil.h"

class Platform::TWindow : public SoyWindow
{
public:
	TWindow( const std::string& Name );
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual void									SetFullscreen(bool Fullscreen) override;
	virtual bool									IsFullscreen() override;
	virtual bool									IsMinimised() override;
	virtual bool									IsForeground() override;
	virtual void									EnableScrollBars(bool Horz,bool Vert) override;

	ESContext											mESContext;
};