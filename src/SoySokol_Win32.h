#include "TApiSokol.h"


#if defined(TARGET_WINDOWS)&&defined(ENABLE_OPENGL)
#include "SoyOpenglContext_Win32.h"
#endif

class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(Sokol::TContextParams Params);
	~SokolOpenglContext();

	virtual void					Queue(std::function<void(sg_context)> Exec) override;
	
private:
	void							DoPaint();
	void							OnPaint(Soy::Rectx<size_t> Rect);
	void							RunGpuJobs();

public:
	std::shared_ptr<SoyThread>		mPaintThread;
	bool							mRunning = true;
	
	sg_context						mSokolContext = {0};
	std::mutex						mOpenglContextLock;

	//	in other platforms, a lot of this context stuff has moved to renderview
	std::shared_ptr<Platform::TRenderView>	mWindow;	//	should be able to reduce down to win32 control
	std::shared_ptr<Win32::TOpenglContext>	mOpenglContext;
	std::shared_ptr<Platform::TWin32Thread>		mWindowThread;

	Sokol::TContextParams			mParams;
	bool							mEnableRenderWhenMinimised = false;
	bool							mEnableRenderWhenBackground = false;

	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;
};