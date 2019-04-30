#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"



namespace ApiOpengl
{
	void	Bind(Bind::TContext& Context);
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
	void	EnableBlend(bool Enable);
	
	void	DrawQuad();
	void	DrawQuad(Opengl::TShader& Shader,std::function<void()> OnShaderBind);
	
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


	
	
extern const char Opengl_Window_TypeName[];
class TWindowWrapper : public Bind::TObjectWrapper<Opengl_Window_TypeName,TRenderWindow>, public TOpenglContextWrapper
{
public:
	TWindowWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper		( Context, This ),
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

	//	these are context things
	//	immediate calls, so... maybe try and mix the context settings
	static void			DrawQuad(Bind::TCallback& Arguments);
	static void			ClearColour(Bind::TCallback& Arguments);
	static void			EnableBlend(Bind::TCallback& Arguments);
	static void			SetViewport(Bind::TCallback& Arguments);
	static void			Render(Bind::TCallback& Arguments);
	static void			RenderChain(Bind::TCallback& Arguments);
	static void			RenderToRenderTarget(Bind::TCallback& Params);
	
	//	window specific
	static void			GetScreenRect(Bind::TCallback& Arguments);
	static void			SetFullscreen(Bind::TCallback& Arguments);
	static void			IsFullscreen(Bind::TCallback& Arguments);

	virtual std::shared_ptr<Opengl::TContext>	GetOpenglContext() override {	return mWindow->GetContext();	}

protected:
	Bind::TContext&		GetOpenglJsCoreContext();

public:
	std::shared_ptr<TRenderWindow>&	mWindow = mObject;
	
	Opengl::TRenderTarget*			mActiveRenderTarget = nullptr;	//	hack until render target is it's own [temp?] object
	
	std::shared_ptr<Bind::TContext>	mOpenglJsCoreContext;
};





	
extern const char Opengl_Shader_TypeName[];
class TShaderWrapper: public Bind::TObjectWrapper<Opengl_Shader_TypeName,Opengl::TShader>
{
public:
	TShaderWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper		( Context, This )
	{
	}
	~TShaderWrapper();
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 		Construct(Bind::TCallback& Params) override;

	static void			Constructor(Bind::TCallback& Params);
	static void			SetUniform(Bind::TCallback& Params);
	void				DoSetUniform(Bind::TCallback& Params,const SoyGraphics::TUniform& Uniform);

	void				CreateShader(std::shared_ptr<Opengl::TContext>& Context,const char* VertSource,const char* FragSource);
	
public:
	std::shared_ptr<Opengl::TShader>&	mShader = mObject;
	std::function<void()>				mShaderDealloc;

	//	which is right!
	Opengl::TContext*					mOpenglContextPtr = nullptr;
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
};

