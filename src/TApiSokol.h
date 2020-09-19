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
	class TRenderCommand_Draw;
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


class Sokol::TRenderCommand_Draw : public TRenderCommandBase
{
public:
	static constexpr std::string_view	Name = "Draw";
	virtual const std::string_view	GetName() override	{	return Name;	};
	
	uint32_t	mGeometryHandle = {0};
	uint32_t	mShaderHandle = {0};
	//	uniforms
	//	triangle count
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
	class TUniform
	{
	public:
		size_t			GetDataSize() const;
		sg_uniform_type	mType = SG_UNIFORMTYPE_INVALID;
		std::string		mName;
		size_t			mArraySize = 1;
	};
public:
	sg_shader_uniform_block_desc	GetUniformBlockDescription() const;

	size_t			mPromiseRef = std::numeric_limits<size_t>::max();
	std::string		mVertSource;
	std::string		mFragSource;
	Array<TUniform>	mUniforms;
};

class Sokol::TCreateGeometry
{
public:
	sg_buffer_desc		GetVertexDescription() const;
	sg_buffer_desc		GetIndexDescription() const;
	sg_primitive_type	GetPrimitiveType() const	{	return SG_PRIMITIVETYPE_TRIANGLES;	}
	sg_index_type		GetIndexType() const		{	return mTriangleIndexes.IsEmpty() ? SG_INDEXTYPE_NONE : SG_INDEXTYPE_UINT32;	}
	int					GetVertexCount() const		{	return mTriangleCount*3;	}
	int					GetDrawVertexCount() const	{	return GetVertexCount();	}
	int					GetDrawVertexFirst() const	{	return 0;	}
	int					GetDrawInstanceCount() const	{	return 1;	}
	
	//	input
	size_t				mPromiseRef = std::numeric_limits<size_t>::max();
	Array<uint32_t>		mTriangleIndexes;

	//	output
	size_t				mTriangleCount = 0;
	Array<uint8_t>		mBufferData;
	sg_layout_desc		mVertexLayout = {0};	//	layout to go in a pipeline/binding
	sg_buffer			mVertexBuffer = {0};
	sg_buffer			mIndexBuffer = {0};
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
	std::map<uint32_t,Sokol::TCreateGeometry>	mGeometrys;

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
	//	cannot block (run()) because some JS engines run in a resolve and then hit a Sokol::Run()
	virtual void	Queue(std::function<void(sg_context)> Callback)=0;
};


