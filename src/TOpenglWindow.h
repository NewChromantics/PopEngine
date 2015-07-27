#pragma once

#include <SoyTypes.h>
#include <SoyThread.h>
#include "SoyOpenglContext.h"

#if __has_feature(objc_arc)
#error expected ARC off, if we NEED arc, then the NSWindow & view need to go in a pure obj-c wrapper to auto retain the refcounted object
#endif


class MacWindow;
class TOpenglView;

class TOpenglWindow : public SoyWorkerThread
{
public:
	TOpenglWindow(const std::string& Name,vec2f Pos,vec2f Size);
	~TOpenglWindow();
	
	bool			IsValid();
	
	virtual bool	Iteration();
	
	Opengl::TContext*	GetContext();
	
private:
	void			OnViewRender(Opengl::TRenderTarget& RenderTarget)
	{
		mOnRender.OnTriggered(RenderTarget);
	}
	
public:
	SoyEvent<Opengl::TRenderTarget>	mOnRender;			//	for now we have a 1:1 view window connection
	SoyListenerId	mOnRenderListener;
	
	std::string		mName;
	std::shared_ptr<TOpenglView>	mView;

private:
	std::shared_ptr<MacWindow>		mMacWindow;
};

