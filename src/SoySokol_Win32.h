#include "TApiSokol.h"


#if defined(TARGET_WINDOWS)&&defined(ENABLE_OPENGL)
#include "Win32OpenglContext.h"
#endif

class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(std::shared_ptr<SoyWindow> Window, Sokol::TContextParams Params);
	~SokolOpenglContext();

	virtual void					Queue(std::function<void(sg_context)> Exec) override;
	
private:
	void							DoPaint();
	void							OnPaint();
	void							RunGpuJobs();

public:
	std::shared_ptr<SoyThread>		mPaintThread;
	bool							mRunning = true;
	
	sg_context						mSokolContext = {0};
	std::mutex						mOpenglContextLock;
	std::shared_ptr<Platform::TWindow>	mWindow;
	std::shared_ptr<Platform::TOpenglView>		mView;
	std::shared_ptr<Win32::TOpenglContext>	mOpenglContext;
	std::shared_ptr<Platform::TWin32Thread>		mWindowThread;

	Sokol::TContextParams			mParams;
	bool							mEnableRenderWhenMinimised = false;
	bool							mEnableRenderWhenBackground = false;

	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;
};