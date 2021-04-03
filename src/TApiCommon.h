#pragma once
#include "TBind.h"
#include "SoyPixels.h"
#include "SoyVector.h"

//	windows macros!
#if defined(TARGET_WINDOWS)
	#undef Yield
	#undef GetComputerName
#endif

class SoyImageProxy;
class SoyPixels;
class SoyPixelsImpl;

namespace Platform
{
	class TFileMonitor;
}

namespace Soy
{
	class TShellExecute;
}

namespace Opengl
{
	class TTexture;
	class TContext;
	class TAsset;
}


//	engine stuff under the Pop namespace
namespace ApiPop
{
	extern const char	Namespace[];
	void	Bind(Bind::TContext& Context);
	
	class TFileMonitorWrapper;
	class TShellExecuteWrapper;

	class TAsyncLoop;
	DECLARE_BIND_TYPENAME(AsyncLoop);
	DECLARE_BIND_TYPENAME(Image);
	DECLARE_BIND_TYPENAME(FileMonitor);
	DECLARE_BIND_TYPENAME(ShellExecute);
}



class TAsyncLoopWrapper : public Bind::TObjectWrapper<ApiPop::BindType::AsyncLoop,ApiPop::TAsyncLoop>
{
public:
	TAsyncLoopWrapper(Bind::TContext& Context) :
		TObjectWrapper			( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);

	virtual void 		Construct(Bind::TCallback& Params) override;
	static void			Iteration(Bind::TCallback& Params);

protected:
	std::shared_ptr<ApiPop::TAsyncLoop>&	mAsyncLoop = mObject;
	Bind::TPersistent		mFunction;
	Bind::TPersistent		mIterationBindThisFunction;
};




//	an image is a generic accessor for pixels, opengl textures, etc etc
class TImageWrapper : public Bind::TObjectWrapper<ApiPop::BindType::Image, SoyImageProxy>
{
public:
	static int Debug_ImageCounter;
public:
	TImageWrapper(Bind::TContext& Context);
	~TImageWrapper();
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 	Construct(Bind::TCallback& Arguments) override;
	
	void			Alloc(Bind::TCallback& Arguments);
	void			Flip(Bind::TCallback& Arguments);
	void			LoadFile(Bind::TCallback& Arguments);
	void			GetWidth(Bind::TCallback& Arguments);
	void			GetHeight(Bind::TCallback& Arguments);
	void			GetRgba8(Bind::TCallback& Arguments);
	void			GetPixelBuffer(Bind::TCallback& Arguments);
	void			SetLinearFilter(Bind::TCallback& Arguments);
	void			Copy(Bind::TCallback& Arguments);
	void			WritePixels(Bind::TCallback& Arguments);
	void			Resize(Bind::TCallback& Arguments);
	void			Clip(Bind::TCallback& Arguments);
	void			Clear(Bind::TCallback& Arguments);
	void			SetFormat(Bind::TCallback& Arguments);
	void			GetFormat(Bind::TCallback& Arguments);
	void			GetPngData(Bind::TCallback& Params);
	

	//	proxy funcs to soyimage
	SoyPixelsImpl&		GetPixels();
	void				GetPixels(SoyPixelsImpl& CopyTarget);
	void				SetPixels(std::shared_ptr<SoyPixelsImpl> Pixels);
	SoyImageProxy&		GetImage();	//	throw if missing (deallocated)
	Opengl::TTexture&	GetTexture();
	std::shared_ptr<Opengl::TTexture>	GetTexturePtr();

public:
	std::shared_ptr<SoyImageProxy>&	mImage = mObject;
};




class ApiPop::TFileMonitorWrapper : public Bind::TObjectWrapper<BindType::FileMonitor,int>
{
public:
	TFileMonitorWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			Add(Bind::TCallback& Params);
	void			Add(const std::string& Filename);
	void			WaitForChange(Bind::TCallback& Params);
	void			OnChanged(const std::string& Filename);
	
public:
	Bind::TPromiseQueueObjects<std::string>		mChangedFileQueue;
	//std::shared_ptr<Platform::TFileMonitor>&	mFileMonitor = mObject;
	Array<std::shared_ptr<Platform::TFileMonitor>>	mMonitors;
};



class ApiPop::TShellExecuteWrapper : public Bind::TObjectWrapper<BindType::ShellExecute, Soy::TShellExecute>
{
public:
	TShellExecuteWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			WaitForExit(Bind::TCallback& Params);
	void			WaitForOutput(Bind::TCallback& Params);

private:
	void			OnExit(int32_t ExitCode);
	void			OnStdOut(const std::string& Out);
	void			OnStdErr(const std::string& Out);
	void			FlushPending();
	void			FlushPendingOutput();

public:
	std::shared_ptr<Soy::TShellExecute>&	mShellExecute = mObject;

	Bind::TPromiseQueue		mWaitForExitPromises;
	Bind::TPromiseQueue		mWaitForOutputPromises;

	std::mutex				mMetaLock;
	bool					mHasExited = false;
	int32_t					mExitedCode = 0;
	Array<std::string>		mPendingOutput;	//	note: no filter between stderr and stdout atm
};

