#pragma once

#include "SoyWindow.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

#import "SoyGuiSwift.h"
//@class GLView;

namespace Platform
{
	class TRenderView;
	
	void	RunJobOnMainThread(std::function<void()> Lambda,bool Block);
}

class Platform::TRenderView : public Gui::TRenderView
{
public:
	virtual MTKView*	GetMetalView()=0;
	virtual GLView*		GetOpenglView()=0;
};
