#pragma once

#if !defined(__OBJC__)
#error This should only be included in mm files
#endif

//	GLKViewController needs GLKit framework linked
//#import <GLKit/GLKit.h>
#import <AppKit/AppKit.h>

//	gr: this should probably be a seperate SoySokol.h
#include "TApiSokol.h"

/*

//	this could do metal & gl
@interface SokolViewDelegate_Gles : UIResponder<GLKViewDelegate>
	
@property std::function<void(CGRect)>	mOnPaint;
	
- (instancetype)init:(std::function<void(CGRect)> )OnPaint;
	
@end
*/
#include "SoyGuiSwift.h"


class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(GLView* View,Sokol::TContextParams Params);
	~SokolOpenglContext();
	
	virtual void					RequestPaint() override;
	virtual void					Queue(std::function<void(sg_context)> Exec) override;
	
private:
	void							RequestViewPaint();
	void							OnPaint(CGRect Rect);
	void							RunGpuJobs();
	bool							IsDoubleBuffered();

public:
	bool							mRunning = true;
	std::shared_ptr<SoyThread>		mPaintThread;
	bool							mPaintRequested = false;
	
	sg_context						mSokolContext = {0};
	GLView*							mView = nullptr;
	NSOpenGLContext*				mOpenglContext = nullptr;
	std::mutex						mOpenglContextLock;
	//SokolViewDelegate_Gles*			mDelegate = nullptr;
	
	Sokol::TContextParams			mParams;
	//	either run these on a thread, or just resolve all at start of next paint
	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;

};
