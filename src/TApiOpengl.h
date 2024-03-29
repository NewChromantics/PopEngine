#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

namespace Win32
{
	class TOpenglContext;
}

namespace ApiOpengl
{
	void	Bind(Bind::TContext& Context);

	//	gr: this will evolve into a generic structured buffer type
	class TTriangleBufferWrapper;
	
	//	this needs to be sorted properly!
	class TRenderTargetWrapper;
	class TWindowWrapper;
	
	DECLARE_BIND_TYPENAME(Window);
	DECLARE_BIND_TYPENAME(Shader);
	DECLARE_BIND_TYPENAME(TriangleBuffer);
}



class TRenderWindow : public TOpenglWindow
{
public:
	TRenderWindow(const std::string& Name,Soy::Rectf Rect,const TOpenglParams& Params) :
		TOpenglWindow	( Name, Rect, Params)
	{
	}
	~TRenderWindow()
	{
	}
	
	void	Clear(Opengl::TRenderTarget& RenderTarget);
	void	ClearColour(Soy::TRgb Colour);
	void	SetBlendModeBlit();
	void	SetBlendModeAlpha();
	void	SetBlendModeMax();
	
	void	DrawGeometry(Opengl::TGeometry& Geometry,Opengl::TShader& Shader,std::function<void()>& OnShaderBind);

	Opengl::TGeometry&	GetBlitQuad();
	
public:
	std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	
	std::shared_ptr<Opengl::TShader>	mDebugShader;
};




//	merge these soon so they share functions!
class TOpenglContextWrapper
{
public:
	virtual ~TOpenglContextWrapper()	{}
	virtual std::shared_ptr<Opengl::TContext>	GetOpenglContext()=0;
};


class ApiOpengl::TRenderTargetWrapper
{
public:
	virtual void		GetRenderContext(Bind::TCallback& Params) = 0;
/* todo:	virtual Opengl::TContext&	GetContext()=0;
	void				DrawQuad(Bind::TCallback& Arguments);
	void				DrawGeometry(Bind::TCallback& Arguments);
	void				ClearColour(Bind::TCallback& Arguments);
	void				EnableBlend(Bind::TCallback& Arguments);
	void				SetViewport(Bind::TCallback& Arguments);
	void				Render(Bind::TCallback& Arguments);
	void				RenderChain(Bind::TCallback& Arguments);
	void				RenderToRenderTarget(Bind::TCallback& Params);
	void				GetTargetRect(Bind::TCallback& Arguments);
 */
};
 

class ApiOpengl::TWindowWrapper : public Bind::TObjectWrapper<BindType::Window,TRenderWindow>, public TOpenglContextWrapper, ApiOpengl::TRenderTargetWrapper
{
public:
	TWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context ),
		mActiveRenderTarget	(nullptr)
	{
	}
	~TWindowWrapper();
	

	void				OnRender(Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext);
	void				OnMouseFunc(const TMousePos& MousePos,SoyMouseButton::Type Button,const std::string& FuncName);
	void				OnKeyFunc(SoyKeyButton::Type Button,const std::string& FuncName);
	bool				OnTryDragDrop(ArrayBridge<std::string>& Filenames);
	void				OnDragDrop(ArrayBridge<std::string>& Filenames);
	void				OnClosed();

	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;

	virtual void		GetRenderContext(Bind::TCallback& Params) override;
	void				DrawQuad(Bind::TCallback& Arguments);
	void				DrawGeometry(Bind::TCallback& Arguments);
	void				ClearColour(Bind::TCallback& Arguments);
	void				SetBlendModeBlit(Bind::TCallback& Arguments);
	void				SetBlendModeAlpha(Bind::TCallback& Arguments);
	void				SetBlendModeMax(Bind::TCallback& Arguments);
	void				SetViewport(Bind::TCallback& Arguments);
	void				Render(Bind::TCallback& Arguments);
	void				RenderChain(Bind::TCallback& Arguments);
	void				RenderToRenderTarget(Bind::TCallback& Params);
	void				GetRenderTargetRect(Bind::TCallback& Arguments);
	
	//	window specific
	void				GetScreenRect(Bind::TCallback& Arguments);
	void				SetFullscreen(Bind::TCallback& Arguments);
	void				IsFullscreen(Bind::TCallback& Arguments);
	void				IsFullscreenSupported(Bind::TCallback& Arguments);
	void				IsMinimised(Bind::TCallback& Arguments);
	void				IsForeground(Bind::TCallback& Arguments);
	void				EnableRenderMinimised(Bind::TCallback& Arguments);
	void				EnableRenderBackground(Bind::TCallback& Arguments);

	virtual std::shared_ptr<Opengl::TContext>	GetOpenglContext() override {	return mWindow->GetContext();	}
	
	//	temp hack for openxr
	std::shared_ptr<Win32::TOpenglContext>		GetWin32OpenglContext();
	//Directx::TContext*							GetDirectxContext();
	
protected:
	Bind::TContext&		GetOpenglJsCoreContext();

public:
	std::shared_ptr<TRenderWindow>&	mWindow = mObject;
	
	Opengl::TRenderTarget*			mActiveRenderTarget = nullptr;	//	hack until render target is it's own [temp?] object
	
	std::shared_ptr<Bind::TContext>	mOpenglJsCoreContext;
};





class TShaderWrapper: public Bind::TObjectWrapper<ApiOpengl::BindType::Shader,Opengl::TShader>
{
public:
	TShaderWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	~TShaderWrapper();
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 		Construct(Bind::TCallback& Params) override;

	static void			Constructor(Bind::TCallback& Params);
	void				SetUniform(Bind::TCallback& Params);
	void				DoSetUniform(Bind::TCallback& Params,const SoyGraphics::TUniform& Uniform);

	void				CreateShader(std::shared_ptr<Opengl::TContext>& Context,const char* VertSource,const char* FragSource);
	
public:
	std::shared_ptr<Opengl::TShader>&	mShader = mObject;
	std::function<void()>				mShaderDealloc;

	//	which is right!
	Opengl::TContext*					mOpenglContextPtr = nullptr;
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
};



class ApiOpengl::TTriangleBufferWrapper : public Bind::TObjectWrapper<ApiOpengl::BindType::TriangleBuffer,Opengl::TGeometry>
{
public:
	TTriangleBufferWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	~TTriangleBufferWrapper();
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;
	
	void				CreateGeometry(const std::string& VertexName,ArrayBridge<float>&& VertexFloats,size_t VertexSize,ArrayBridge<uint32_t>&& Indexes);

public:
	std::shared_ptr<Opengl::TGeometry>&		mGeometry = mObject;
};


