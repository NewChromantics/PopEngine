#pragma once

#if !defined(__OBJC__)
#error This should only be included in mm files
#endif

//	GLKViewController needs GLKit framework linked
#import <GLKit/GLKit.h>

//	gr: this should probably be a seperate SoySokol.h
#include "TApiSokol.h"



//	this could do metal & gl
@interface SokolViewDelegate_Gles : UIResponder<GLKViewDelegate>
	
@property std::function<void(CGRect)>	mOnPaint;
	
- (instancetype)init:(std::function<void(CGRect)> )OnPaint;
	
@end


class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,Sokol::TContextParams Params);
	~SokolOpenglContext();
	
	void							TriggerPaint();
	
public:
	bool							mRunning = true;
	std::shared_ptr<SoyThread>		mPaintThread;
	
	sg_context						mSokolContext = {0};
	GLKView*             			mView = nullptr;
	EAGLContext*					mOpenglContext = nullptr;
	SokolViewDelegate_Gles*			mDelegate = nullptr;
	
	Sokol::TContextParams			mParams;
};
