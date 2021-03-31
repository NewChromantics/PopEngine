#pragma once

#if !defined(__OBJC__)
#error This should only be included in mm files
#endif

//	GLKViewController needs GLKit framework linked
#import <GLKit/GLKit.h>

//	gr: this should probably be a seperate SoySokol.h
#include "TApiSokol.h"
#import "SoyGuiSwift.h"	//	GLView



class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(GLView* View,Sokol::TContextParams Params);
	~SokolOpenglContext();
	
	virtual void					RequestPaint() override;
	virtual void					Queue(std::function<void(sg_context)> Exec) override;
	
private:
	void							RequestViewPaint();
	void							OnPaint(Soy::Rectx<size_t> Rect);
	void							RunGpuJobs();

public:
	bool							mRunning = true;
	std::shared_ptr<SoyThread>		mPaintThread;
	bool							mPaintRequested = false;
	
	sg_context						mSokolContext = {0};
	GLView*							mView = nullptr;
	EAGLContext*					mOpenglContext = nullptr;
	std::mutex						mOpenglContextLock;
	
	Sokol::TContextParams			mParams;
	//	either run these on a thread, or just resolve all at start of next paint
	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;

};
