#pragma once
#include "TBind.h"
#include "SoyWindow.h"
#include "SoyImageProxy.h"
#include "SoySokol.h"

namespace ApiSokol
{
	void Bind(Bind::TContext &Context);

	class TSokolContextWrapper;

	DECLARE_BIND_TYPENAME(Sokol_Context);
}

class SoyImageProxy;

//	non-js-api sokol
namespace Sokol
{
	//	this doesn't need to be sokol specific
	//	gr: maybe a better way than objects, but we could pool them
	class TRenderCommandBase;
	class TRenderCommand_Draw;
	class TRenderCommand_SetRenderTarget;
	class TRenderCommand_UpdateImage;	//	internal texture update
	class TRenderCommands;

	class TShader;
	class TCreateShader;
	class TCreateGeometry;

	void	ParseRenderCommand(std::function<void(std::shared_ptr<Sokol::TRenderCommandBase>)> PushCommand,const std::string_view& Name,Bind::TCallback& Params,std::function<Sokol::TShader&(uint32_t)>& GetShader);
}

class Sokol::TRenderCommandBase
{
public:
	virtual const std::string_view	GetName()=0;
};


class Sokol::TRenderCommand_SetRenderTarget : public TRenderCommandBase
{
public:
	static constexpr std::string_view	Name = "SetRenderTarget";
	virtual const std::string_view	GetName() override { return Name; };
	
	bool							IsClearColour() const	{	return mClearColour.a > 0.f;	}

	std::shared_ptr<SoyImageProxy>	mTargetTexture = nullptr;		//	if null, render to screen
	SoyPixelsFormat::Type			mReadBackFormat = SoyPixelsFormat::Invalid;
	sg_color						mClearColour = {1,0,1,1};	//	if no alpha, we don't clear
};

class Sokol::TRenderCommand_Draw : public TRenderCommandBase
{
public:
	static constexpr std::string_view	Name = "Draw";
	virtual const std::string_view		GetName() override	{	return Name;	};
	
	void			ParseUniforms(Bind::TObject& UniformsObject,Sokol::TShader& Shader);
	
	uint32_t		mGeometryHandle = {0};
	uint32_t		mShaderHandle = {0};

	//	uniforms, parsed and written immediately into a block when parsing
	Array<uint8_t>	mUniformBlock;

	std::map<size_t,std::shared_ptr<SoyImageProxy>>	mImageUniforms;	//	texture slot -> texture
	std::map<size_t,std::string>					mDebug_ImageUniformNames;	//	texture slot -> uniform name
};

class Sokol::TRenderCommand_UpdateImage : public TRenderCommandBase
{
public:
	static constexpr std::string_view	Name = "UpdateImage";
	virtual const std::string_view		GetName() override	{	return Name;	};
	
	std::shared_ptr<SoyImageProxy>		mImage;
	bool															mIsRenderTarget = false;
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
	class TImageUniform
	{
	public:
		std::string		mName;
		//std::shared_ptr<TImageWrapper>	mImage;
	};
public:
	void				EnumUniformBlockDescription(std::function<void(const sg_shader_uniform_block_desc&,size_t)> OnImageDesc) const;
	void				EnumImageDescriptions(std::function<void(const sg_shader_image_desc&,size_t)> OnImageDesc) const;
	
	const TUniform*			GetUniform(const std::string& Name,size_t& DataOffset);
	const TImageUniform*	GetImageUniform(const std::string& Name,size_t& ImageIndex);
	size_t				GetUniformBlockSize() const;

	size_t				mPromiseRef = std::numeric_limits<size_t>::max();
	std::string			mVertSource;
	std::string			mFragSource;
	Array<TUniform>		mUniforms;
	Array<TImageUniform>		mImageUniforms;
	Array<std::string>	mAttributes;
};


class Sokol::TShader
{
public:
	TCreateShader		mShaderMeta;	//	currently need to hold onto this for the uniform info
	sg_shader			mShader = {0};
};

class Sokol::TCreateGeometry
{
public:
	sg_buffer_desc		GetVertexDescription() const;
	sg_buffer_desc		GetIndexDescription() const;
	sg_primitive_type	GetPrimitiveType() const	{	return SG_PRIMITIVETYPE_TRIANGLES;	}
	sg_index_type		GetIndexType() const		{	return mTriangleIndexes.IsEmpty() ? SG_INDEXTYPE_NONE : SG_INDEXTYPE_UINT32;	}
	int					GetVertexCount() const		{	return mVertexCount;	}
	int					GetDrawVertexCount() const	{	return GetVertexCount();	}
	int					GetDrawVertexFirst() const	{	return 0;	}
	int					GetDrawInstanceCount() const	{	return 1;	}
	
	//	input
	size_t				mPromiseRef = std::numeric_limits<size_t>::max();
	Array<uint32_t>		mTriangleIndexes;

	//	output
	size_t				mVertexCount = 0;
	Array<float>		mBufferData;
	sg_layout_desc		mVertexLayout = {0};	//	layout to go in a pipeline/binding
	size_t				GetVertexLayoutBufferSlots() const;
	sg_buffer			mVertexBuffer = {0};
	sg_buffer			mIndexBuffer = {0};
};


class ApiSokol::TSokolContextWrapper : public Bind::TObjectWrapper<BindType::Sokol_Context,Sokol::TContext>
{
public:
	TSokolContextWrapper(Bind::TContext &Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate &Template);
	virtual void	Construct(Bind::TCallback &Params) override;

	//	gr: can't decide if this should be here and reflect the rendered view which matches the view rendered into
	//		or if the user should be probing the RenderView control for size
	void			GetScreenRect(Bind::TCallback& Params);
	//	gr: would prefer a name like, WaitForRender to indicate it's async
	void			Render(Bind::TCallback& Params);

	//	also async
	void			CreateShader(Bind::TCallback& Params);
	void			CreateGeometry(Bind::TCallback& Params);

private:
	//	gr: sg_context isnt REQUIRED, but hints to implementations that they should be creating it
	void			OnPaint(sg_context Context,vec2x<size_t> ViewRect);
	void			InitDebugFrame(Sokol::TRenderCommands& Commands);
	void			InitDefaultAssets();

	Sokol::TRenderCommands			ParseRenderCommands(Bind::TLocalContext& Context,Bind::TArray& CommandArray);

	void			QueueImageDelete(sg_image Image);
	void			FreeImageDeletes();
	
public:
	vec2x<size_t>					mLastRect;

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
	std::map<uint32_t,Sokol::TShader>	mShaders;
	std::map<uint32_t,Sokol::TCreateGeometry>	mGeometrys;
	std::shared_ptr<SoyImageProxy>	mNullTexture;
	std::mutex						mPendingDeleteImagesLock;
	Array<sg_image>					mPendingDeleteImages;

	std::shared_ptr<Sokol::TContext>&					mSokolContext = mObject;
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


