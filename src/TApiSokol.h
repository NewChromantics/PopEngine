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
	Array<std::shared_ptr<TRenderCommandBase>>	mCommands;
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

private:
	//	gr: sg_context isnt REQUIRED, but hints to implementations that they should be creating it
	void			OnPaint(sg_context Context,vec2x<size_t> ViewRect);
	void			InitDebugFrame(Sokol::TRenderCommands& Commands);

public:
	Bind::TPromiseQueueObjects<Sokol::TRenderCommands>	mPendingFrames;
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
};


