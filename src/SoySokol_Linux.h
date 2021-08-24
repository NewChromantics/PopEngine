#include "TApiSokol.h"
//#include "LinuxDRM/esUtil.h"

class EglWindow;

namespace Egl
{
	void	IsOkay(const char* Context);
}

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
	bool							mRunning = true;
	std::shared_ptr<SoyThread>		mPaintThread;
	bool							mPaintRequested = false;

	sg_context						mSokolContext = {0};
	std::shared_ptr<EglRenderView>	mRenderView;
	std::mutex						mOpenglContextLock;
	
	Sokol::TContextParams			mParams;

	Array<std::function<void(sg_context)>>	mGpuJobs;
	std::mutex							mGpuJobsLock;
};