#pragma once

#include "SoyWindow.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

namespace Platform
{
	class TRenderView;
}

class Platform::TRenderView : public Gui::TRenderView
{
public:
	virtual MTKView*	GetMetalView()=0;
	virtual GLKView*	GetOpenglView()=0;
};
