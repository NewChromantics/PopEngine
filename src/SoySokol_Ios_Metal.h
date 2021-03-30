#pragma once

#if !defined(__OBJC__)
#error This should only be included in mm files
#endif

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

//	gr: this should probably be a seperate SoySokol.h
#include "TApiSokol.h"


class SokolMetalContext : public Sokol::TContext
{
public:
	SokolMetalContext(MTKView* View,Sokol::TContextParams Params);
	
	virtual void					Queue(std::function<void(sg_context)> Exec) override;

public:
	MTKView*						mView = nullptr;
	id<MTLDevice>					mMetalDevice;
	Sokol::TContextParams			mParams;
};

