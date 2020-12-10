#pragma once

#include "SoyWindow.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

@class GLView;

namespace Platform
{
	class TRenderView;
}

class Platform::TRenderView : public Gui::TRenderView
{
public:
	virtual MTKView*	GetMetalView()=0;
	virtual GLView*		GetOpenglView()=0;
};
