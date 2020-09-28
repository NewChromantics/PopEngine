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
	bool							mRunning = true;

	sg_context						mSokolContext = {0};
	std::mutex						mOpenglContextLock;
	Win32::TOpenglContext*			mOpenglContext = nullptr;
	std::shared_ptr<Platform::TWindow>	mWindow;
	std::shared_ptr<Platform::TOpenglView>		mView;
	std::shared_ptr<Platform::TOpenglContext>	mWindowContext;
	std::shared_ptr<Platform::TWin32Thread>		mWindowThread;

	Sokol::TContextParams			mParams;

	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;
};