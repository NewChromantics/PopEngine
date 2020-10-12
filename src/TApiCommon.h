#pragma once
#include "TBind.h"
#include "SoyPixels.h"
#include "SoyVector.h"

//	windows macros!
#if defined(TARGET_WINDOWS)
	#undef Yield
	#undef GetComputerName
#endif

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

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
	class TExternalDrive;
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
class TImageWrapper : public Bind::TObjectWrapper<ApiPop::BindType::Image,SoyPixelsImpl>
{
public:
	TImageWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	~TImageWrapper();
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 		Construct(Bind::TCallback& Arguments) override;
	
	static void			Alloc(Bind::TCallback& Arguments);
	static void			Flip(Bind::TCallback& Arguments);
	void				LoadFile(Bind::TCallback& Arguments);
	static void			GetWidth(Bind::TCallback& Arguments);
	static void			GetHeight(Bind::TCallback& Arguments);
	static void			GetRgba8(Bind::TCallback& Arguments);
	static void			GetPixelBuffer(Bind::TCallback& Arguments);
	static void			SetLinearFilter(Bind::TCallback& Arguments);
	static void			Copy(Bind::TCallback& Arguments);
	static void			WritePixels(Bind::TCallback& Arguments);
	static void			Resize(Bind::TCallback& Arguments);
	static void			Clip(Bind::TCallback& Arguments);
	static void			Clear(Bind::TCallback& Arguments);
	static void			SetFormat(Bind::TCallback& Arguments);
	static void			GetFormat(Bind::TCallback& Arguments);
	void				GetPngData(Bind::TCallback& Params);
	
	void									DoLoadFile(const std::string& Filename,std::function<void(const std::string&,const ArrayBridge<uint8_t>&)> OnMetaFound);
	void									DoSetLinearFilter(bool LinearFilter);
	void									GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	Opengl::TTexture&						GetTexture();
	std::shared_ptr<Opengl::TTexture>		GetTexturePtr();
	SoyPixelsImpl&							GetPixels();
	void									GetPixels(SoyPixelsImpl& CopyTarget);	//	safely copy pixels

	//	we consider version 0 uninitisalised
	size_t									GetLatestVersion() const;
	void									SetOpenglTexture(const Opengl::TAsset& Texture);
	void									OnOpenglTextureChanged(Opengl::TContext& Context);
	void									ReadOpenglPixels(SoyPixelsFormat::Type Format);
	void									SetPixels(const SoyPixelsImpl& NewPixels);
	void									SetPixels(std::shared_ptr<SoyPixelsImpl> NewPixels);
	void									SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels);
	SoyPixelsMeta							GetMeta();
	void									GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback);	//	lock & unlock pixels for processing
	void									OnPixelsChanged();	//	increase version

	void									SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer);

	
	void								SetSokolImage(uint32_t Handle);	//	set handle, no version
	void								OnSokolImageChanged();	//	increase version
	void								OnSokolImageUpdated();	//	now matching latest version
	bool								HasSokolImage();
	uint32_t							GetSokolImage(bool& LatestVersion);

	
protected:
	void								Free();
	
public:
	std::string							mName = "UninitialisedName";	//	for debug

protected:
	std::recursive_mutex				mPixelsLock;			//	not sure if we need it for the others?
	std::shared_ptr<SoyPixelsImpl>&		mPixels = mObject;
	size_t								mPixelsVersion = 0;			//	opengl texture changed

	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	std::function<void()>				mOpenglTextureDealloc;
	size_t								mOpenglTextureVersion = 0;	//	pixels changed
	std::shared_ptr<SoyPixels>			mOpenglClientStorage;	//	gr: apple specific client storage for texture. currently kept away from normal pixels for safety, but merge later

	uint32_t							mSokolImage = 0;
	size_t								mSokolImageVersion = 0;	//	pixels changed

public:
	//	temporary caching system for immediate mode glReadPixels
	std::shared_ptr<Array<uint8_t>>		mOpenglLastPixelReadBuffer;
	size_t								mOpenglLastPixelReadBufferVersion = 0;

protected:
	//	abstracted pixel buffer from media
	std::shared_ptr<TPixelBuffer>		mPixelBuffer;
	size_t								mPixelBufferVersion = 0;
	SoyPixelsMeta						mPixelBufferMeta;
	
	//	texture options
	bool								mLinearFilter = false;
	bool								mRepeating = false;
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

class ApiPop::TExternalDrive
{
public:
	TExternalDrive(const std::string& Label, const std::string& DevNode);
	~TExternalDrive();

public:
	std::string						mLabel;
	std::string						mDevNode;

	bool									mIsMounted = false;
	std::string						mMountPath;

	void 									MountDrive();
};