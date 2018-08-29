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
	void		OnMouseFunc(const TMousePos& MousePos,const std::string& MouseFuncName);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	//	these are context things
	static v8::Local<v8::Value>				DrawQuad(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				ClearColour(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				SetViewport(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Render(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				RenderChain(const v8::CallbackInfo& Arguments);

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
	static v8::Local<v8::Value>				SetUniform(const v8::CallbackInfo& Arguments);
	v8::Local<v8::Value>					DoSetUniform(const v8::CallbackInfo& Arguments);

	static TShaderWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TShaderWrapper>( Value, 0 );	}
	
	void									CreateShader(std::shared_ptr<Opengl::TContext>& Context,const char* VertSource,const char* FragSource);
	
public:
	Opengl::TContext*					mContext;
	v8::Persistent<v8::Object>			mHandle;
	std::shared_ptr<Opengl::TShader>	mShader;
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::function<void()>				mShaderDealloc;
	TV8Container*						mContainer;
};

