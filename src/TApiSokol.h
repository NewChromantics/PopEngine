#pragma once
#include "TBind.h"
#include "SoyWindow.h"
#include "sokol/sokol_gfx.h"

namespace ApiSokol
{
	void Bind(Bind::TContext &Context);

	class TSokolContextWrapper;

	DECLARE_BIND_TYPENAME(Context);
}

//	non-js-api sokol
namespace Sokol
{
	class TContext;
	class TContextParams;	//	or ViewParams?

	std::shared_ptr<TContext>	Platform_CreateContext(std::shared_ptr<SoyWindow> Window,TContextParams Params);

	//	this doesn't need to be sokol specific
	//	gr: maybe a better way than objects, but we could pool them
	class TRenderCommandBase;
	class TRenderCommand_Clear;
	class TRenderCommands;

	class TCreateShader;
	class TCreateGeometry;

	static std::shared_ptr<Sokol::TRenderCommandBase>	ParseRenderCommand(const std::string_view& Name,Bind::TCallback& Params);
	static TRenderCommands								ParseRenderCommands(Bind::TLocalContext& Context,Bind::TArray& CommandArray);
}

class Sokol::TContextParams
{
public:
	std::function<void(sg_context,vec2x<size_t>)>	mOnPaint;		//	render callback
	std::string							mViewName;		//	try to attach to existing views
	size_t								mFramesPerSecond = 60;
};

class Sokol::TRenderCommandBase
{
public:
	virtual const std::string_view	GetName()=0;
};

class Sokol::TRenderCommand_Clear : public TRenderCommandBase
{
public:
	static constexpr std::string_view	Name = "Clear";
	virtual const std::string_view	GetName() override	{	return Name;	};

	float		mColour[4] = {1,0,1,1};
};


class Sokol::TRenderCommands
{
public:
	size_t		mPromiseRef = std::numeric_limits<size_t>::max();
	Array<std::shared_ptr<TRenderCommandBase>>	mCommands;
};

class Sokol::TCreateShader
{
public:
	size_t		mPromiseRef = std::numeric_limits<size_t>::max();
	std::string	mVertSource;
	std::string	mFragSource;
};

class Sokol::TCreateGeometry
{
public:
	size_t			mPromiseRef = std::numeric_limits<size_t>::max();
	sg_buffer_desc	mBufferDescription;
	Array<uint8_t>	mBufferData;
};


class ApiSokol::TSokolContextWrapper : public Bind::TObjectWrapper<BindType::Context,Sokol::TContext>
{
public:
	TSokolContextWrapper(Bind::TContext &Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate &Template);
	virtual void	Construct(Bind::TCallback &Params) override;

	//	gr: would prefer a name like, WaitForRender to indicate it's async
	void			Render(Bind::TCallback& Params);

	//	also async
	void			CreateShader(Bind::TCallback& Params);
	void			CreateGeometry(Bind::TCallback& Params);

private:
	//	gr: sg_context isnt REQUIRED, but hints to implementations that they should be creating it
	void			OnPaint(sg_context Context,vec2x<size_t> ViewRect);
	void			InitDebugFrame(Sokol::TRenderCommands& Commands);

public:
	Bind::TPromiseMap				mPendingFramePromises;
	Array<Sokol::TRenderCommands>	mPendingFrames;
	std::mutex						mPendingFramesLock;

	Bind::TPromiseMap				mPendingShaderPromises;
	Array<Sokol::TCreateShader>		mPendingShaders;
	std::mutex						mPendingShadersLock;
	
	Bind::TPromiseMap				mPendingGeometryPromises;
	Array<Sokol::TCreateGeometry>	mPendingGeometrys;
	std::mutex						mPendingGeometrysLock;

	//	allocated objects and their javascript handle[value]
	std::map<uint32_t,sg_shader>	mShaders;

	std::shared_ptr<Sokol::TContext>&					mSokolContext = mObject;
	//Bind::TPersistent							mWindow;
	//std::shared_ptr<SoyWindow>				mSoyWindow;
	Sokol::TRenderCommands		mLastFrame;	//	 if we get a required paint, but no pending renders, we re-render the last frame
};



class Sokol::TContext
{
public:
	virtual void	RequestPaint()	{};	//	wake up render threads if the context isn't already auto-rendering
	//std::function<void(sg_context,vec2x<size_t>)>	mOnPaint;
	
	//	execute something on the context[thread]
	//	may need a Queue rather than blocking, but makes sense to
	//	make this blocking and then the sokol wrapper can deal with
	//	high level queueing
	virtual void	Run(std::function<void(sg_context)> Callback)=0;
};


