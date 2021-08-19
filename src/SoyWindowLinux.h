#pragma once

#if defined(ENABLE_OPENGL)
#include "LinuxDRM/esUtil.h"
#include <EGL/egl.h>
#endif

//#if defined(ENABLE_OPENGL)
#include <EGL/egl.h>
class EglWindow;
class EglRenderView;
//#endif
class EglContext;


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

class EglWindow : public SoyWindow
{
public:
	EglWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect );
	~EglWindow();

	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual void					SetFullscreen(bool Fullscreen) override	{};
	virtual bool					IsFullscreen() override	{	return true;	};
	virtual bool					IsMinimised() override	{	return false;	};
	virtual bool					IsForeground() override	{	return true;	};
	virtual void					EnableScrollBars(bool Horz,bool Vert) override	{};

public:
	std::shared_ptr<EglContext>		mContext;
};

class EglRenderView : public Gui::TRenderView
{
public:
	EglRenderView(SoyWindow& Parent);

public:
	//	bit unsafe!
	EglWindow&		mWindow;
};

