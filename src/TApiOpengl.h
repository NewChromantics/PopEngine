#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"



namespace ApiOpengl
{
	void	Bind(TV8Container& Container);
}



class TRenderWindow : public TOpenglWindow
{
public:
	TRenderWindow(const std::string& Name,const TOpenglParams& Params) :
		TOpenglWindow	( Name, Soy::Rectf(0,0,100,100), Params)
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


	
	
extern const char Window_TypeName[];
class TWindowWrapper : public TObjectWrapper<Window_TypeName,TRenderWindow>, public TOpenglContextWrapper
{
public:
	TWindowWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper		( Container, This ),
		mActiveRenderTarget	(nullptr)
	{
	}
	~TWindowWrapper();
	

	void		OnRender(Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext);
	void		OnMouseFunc(const TMousePos& MousePos,SoyMouseButton::Type MouseButton,const std::string& MouseFuncName);
	bool		OnTryDragDrop(ArrayBridge<std::string>& Filenames);
	void		OnDragDrop(ArrayBridge<std::string>& Filenames);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(Bind::TCallback& Arguments) override;

	//	these are context things
	//	immediate calls, so... maybe try and mix the context settings
	static void			DrawQuad(Bind::TCallback& Arguments);
	static void			ClearColour(Bind::TCallback& Arguments);
	static void			EnableBlend(Bind::TCallback& Arguments);
	static void			SetViewport(Bind::TCallback& Arguments);
	static void			Render(Bind::TCallback& Arguments);
	static void			RenderChain(Bind::TCallback& Arguments);

	//	window specific
	static void			GetScreenRect(Bind::TCallback& Arguments);

	virtual std::shared_ptr<Opengl::TContext>	GetOpenglContext() override {	return mWindow->GetContext();	}

public:
	std::shared_ptr<TRenderWindow>&	mWindow = mObject;
	
	Opengl::TRenderTarget*			mActiveRenderTarget;	//	hack until render target is it's own [temp?] object
};



class TShaderWrapper
{
public:
	TShaderWrapper() :
		mContainer	( nullptr )
	{
	}
	~TShaderWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				SetUniform(v8::TCallback& Arguments);
	v8::Local<v8::Value>					DoSetUniform(v8::TCallback& Arguments);

	static TShaderWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TShaderWrapper>( Value, 0 );	}
	
	void									CreateShader(std::shared_ptr<Opengl::TContext>& Context,const char* VertSource,const char* FragSource);
	
public:
	Opengl::TContext*					mContext;
	std::shared_ptr<V8Storage<v8::Object>>	mHandle;
	std::shared_ptr<Opengl::TShader>	mShader;
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::function<void()>				mShaderDealloc;
	TV8Container*						mContainer;
	
	size_t								mCurrentTextureIndex = 0;	//	glActiveTexture
};

