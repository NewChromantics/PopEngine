#pragma once


#if defined(ENABLE_OPENGL)
#define ENABLE_EGL
#endif

#if defined(ENABLE_EGL)
#include "EglContext.h"
//	GUI types
class EglWindow;
class EglRenderView;
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
};

#if defined(ENABLE_EGL)
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
	std::shared_ptr<Egl::TDisplaySurfaceContext>		mContext;
};
#endif

#if defined(ENABLE_EGL)
class EglRenderView : public Gui::TRenderView
{
public:
	EglRenderView(SoyWindow& Parent);

public:
	//	bit unsafe!
	EglWindow&		mWindow;
};
#endif
