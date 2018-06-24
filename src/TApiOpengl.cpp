#include "TApiOpengl.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

using namespace v8;

const char DrawQuad_FunctionName[] = "DrawQuad";
const char ClearColour_FunctionName[] = "ClearColour";
const char SetUniform_FunctionName[] = "SetUniform";

void ApiOpengl::Bind(TV8Container& Container)
{
	Container.BindObjectType("OpenglWindow", TWindowWrapper::CreateTemplate );
	Container.BindObjectType("OpenglShader", TShaderWrapper::CreateTemplate );
}


TWindowWrapper::~TWindowWrapper()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
}

void TWindowWrapper::OnRender(Opengl::TRenderTarget& RenderTarget)
{
	mWindow->Clear( RenderTarget );
	
	//  call javascript
	TV8Container& Container = *mContainer;
	auto Runner = [&](Local<Context> context)
	{
		auto* isolate = context->GetIsolate();
		auto This = Local<Object>::New( isolate, this->mHandle );
		Container.ExecuteFunc( context, "OnRender", This );
	};
	Container.RunScoped( Runner );
}


void TWindowWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 1 )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "missing arg 0 (window name)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	
	
	String::Utf8Value WindowName( Arguments[0] );
	std::Debug << "Window Wrapper constructor (" << *WindowName << ")" << std::endl;
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWindow = new TWindowWrapper();
	
	TOpenglParams Params;
	Params.mDoubleBuffer = false;
	NewWindow->mWindow.reset( new TRenderWindow( *WindowName, Params ) );
	
	//	store persistent handle to the javascript object
	NewWindow->mHandle.Reset( Isolate, Arguments.This() );
	
	NewWindow->mContainer = &Container;
	
	auto OnRender = [NewWindow](Opengl::TRenderTarget& RenderTarget)
	{
		NewWindow->OnRender( RenderTarget );
	};
	NewWindow->mWindow->mOnRender.AddListener( OnRender );
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWindow ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}

v8::Local<v8::Value> TWindowWrapper::DrawQuad(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;

	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TWindowWrapper>( ThisHandle );
	//auto* This = reinterpret_cast<TWindowWrapper*>( Local<External>::Cast(ThisHandle)->Value() );
	
	if ( Arguments.Length() >= 1 )
	{
		auto ShaderHandle = Arguments[0];
		auto& Shader = TShaderWrapper::Get( Arguments[0] );

		auto OnShaderBindHandle = Arguments[1];
		std::function<void()> OnShaderBind = []{};
		if ( !OnShaderBindHandle->IsUndefined() )
		{
			auto OnShaderBindHandleFunc = v8::Local<Function>::Cast( OnShaderBindHandle );
			OnShaderBind = [&]
			{
				auto OnShaderBindThis = Params.mContext->Global();
				BufferArray<Local<Value>,1> Args;
				Args.PushBack( ShaderHandle );
				Params.mContainer.ExecuteFunc( Params.mContext, OnShaderBindHandleFunc, OnShaderBindThis, GetArrayBridge(Args) );
			};
		}
		
		This.mWindow->DrawQuad( *Shader.mShader, OnShaderBind );
	}
	else
	{
		This.mWindow->DrawQuad();
	}
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TWindowWrapper::ClearColour(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto* This = reinterpret_cast<TWindowWrapper*>( Local<External>::Cast(ThisHandle)->Value() );
	
	if ( Arguments.Length() != 3 )
		throw Soy::AssertException("Expecting 3 arguments for ClearColour(r,g,b)");

	auto Red = Local<Number>::Cast( Arguments[0] );
	auto Green = Local<Number>::Cast( Arguments[1] );
	auto Blue = Local<Number>::Cast( Arguments[2] );
	Soy::TRgb Colour( Red->Value(), Green->Value(), Blue->Value() );
		
	This->mWindow->ClearColour( Colour );
	return v8::Undefined(Params.mIsolate);
}


Local<FunctionTemplate> TWindowWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	
	//	add members
	Container.BindFunction<DrawQuad_FunctionName>( InstanceTemplate, DrawQuad );
	Container.BindFunction<ClearColour_FunctionName>( InstanceTemplate, ClearColour );
	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "x"), GetPointX, SetPointX);
	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "y"), GetPointY, SetPointY);
	
	//Point* p = ...;
	//Local<Object> obj = point_templ->NewInstance();
	//obj->SetInternalField(0, External::New(isolate, p));
	
	return ConstructorFunc;
}


void TRenderWindow::Clear(Opengl::TRenderTarget &RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	//Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
	Opengl::ClearDepth();
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	
	auto OpenglContext = this->GetContext();
	Opengl_IsOkay();
}


void TRenderWindow::ClearColour(Soy::TRgb Colour)
{
	Opengl::ClearColour( Colour );
}


Opengl::TGeometry& TRenderWindow::GetBlitQuad()
{
	if ( !mBlitQuad )
	{
		//	make mesh
		struct TVertex
		{
			vec2f	uv;
		};
		class TMesh
		{
			public:
			TVertex	mVertexes[4];
		};
		TMesh Mesh;
		Mesh.mVertexes[0].uv = vec2f( 0, 0);
		Mesh.mVertexes[1].uv = vec2f( 1, 0);
		Mesh.mVertexes[2].uv = vec2f( 1, 1);
		Mesh.mVertexes[3].uv = vec2f( 0, 1);
		Array<size_t> Indexes;
		
		Indexes.PushBack( 0 );
		Indexes.PushBack( 1 );
		Indexes.PushBack( 2 );
		
		Indexes.PushBack( 2 );
		Indexes.PushBack( 3 );
		Indexes.PushBack( 0 );
		
		//	for each part of the vertex, add an attribute to describe the overall vertex
		SoyGraphics::TGeometryVertex Vertex;
		auto& UvAttrib = Vertex.mElements.PushBack();
		UvAttrib.mName = "TexCoord";
		UvAttrib.SetType<vec2f>();
		UvAttrib.mIndex = 0;	//	gr: does this matter?
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	}
	
	return *mBlitQuad;
}

void TRenderWindow::DrawQuad()
{
	//	allocate objects we need!
	if ( !mDebugShader )
	{
		auto& BlitQuad = GetBlitQuad();
		auto& Context = *GetContext();
		
		auto VertShader =
		"#version 410\n"
		//"uniform vec4 Rect;\n"
		"const vec4 Rect = vec4(0,0,1,1);\n"
		"in vec2 TexCoord;\n"
		"out vec2 uv;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
		"   gl_Position.xy *= Rect.zw;\n"
		"   gl_Position.xy += Rect.xy;\n"
		//	move to view space 0..1 to -1..1
		"	gl_Position.xy *= vec2(2,2);\n"
		"	gl_Position.xy -= vec2(1,1);\n"
		"	uv = vec2(TexCoord.x,1-TexCoord.y);\n"
		"}\n";
		auto FragShader =
		"#version 410\n"
		"in vec2 uv;\n"
		//"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = vec4(uv.x,uv.y,0,1);\n"
		"}\n";
		
		mDebugShader.reset( new Opengl::TShader( VertShader, FragShader, BlitQuad.mVertexDescription, "Blit shader", Context ) );
	}
	
	DrawQuad( *mDebugShader, []{} );
}


void TRenderWindow::DrawQuad(Opengl::TShader& Shader,std::function<void()> OnBind)
{
	auto& BlitQuad = GetBlitQuad();
	
	//	do bindings
	auto ShaderBound = Shader.Bind();
	OnBind();
	//ShaderBound.SetUniform("Rect", Soy::RectToVector(Rect) );
	BlitQuad.Draw();
	Opengl_IsOkay();

}






TShaderWrapper::~TShaderWrapper()
{
	//	todo: opengl deferrefed delete
}


void TShaderWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 3 )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "missing arguments (Window,FragSource,VertSource)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	
	//	gr: auto catch this
	try
	{
		auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
		
		//	access to context!
		auto& Window = TWindowWrapper::Get( Arguments[0] );
		auto& OpenglContext = *Window.mWindow->GetContext();
		String::Utf8Value VertSource( Arguments[1] );
		String::Utf8Value FragSource( Arguments[2] );

		//	this needs to be deffered to be on the opengl thread (or at least wait for context to initialise)
		std::function<Opengl::TGeometry&()> GetGeo = [&Window]()-> Opengl::TGeometry&
		{
			auto& Geo = Window.mWindow->GetBlitQuad();
			return Geo;
		};
		//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
		//		but it also needs to know of the V8container to run stuff
		//		cyclic hell!
		auto* NewShader = new TShaderWrapper();
		NewShader->mHandle.Reset( Isolate, Arguments.This() );
		NewShader->mContainer = &Container;

		NewShader->CreateShader( OpenglContext, GetGeo, *VertSource, *FragSource );
		
		//	set fields
		This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewShader ) );
		
		// return the new object back to the javascript caller
		Arguments.GetReturnValue().Set( This );
	}
	catch(std::exception& e)
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, e.what() ));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
}

void EnumFloatArray(v8::Local<v8::Value> ValueHandle,ArrayBridge<float>& FloatArray);

void EnumFloatArray(v8::Local<v8::Value> ValueHandle,ArrayBridge<float>&& FloatArray)
{
	EnumFloatArray( ValueHandle, FloatArray );
}

void EnumFloatArray(v8::Local<v8::Value> ValueHandle,ArrayBridge<float>& FloatArray)
{
	if ( ValueHandle->IsNumber() )
	{
		auto ValueFloat = Local<Number>::Cast( ValueHandle );
		FloatArray.PushBack( ValueFloat->Value() );
	}
	else if ( ValueHandle->IsArray() )
	{
		//	we recursively expand arrays
		//	really we should only allow one level deep and check against the uniform (to allow arrays of vec4)
		auto ValueArray = Local<v8::Array>::Cast( ValueHandle );
		for ( auto i=0;	i<ValueArray->Length();	i++ )
		{
			auto ElementHandle = ValueArray->Get(i);
			EnumFloatArray( ElementHandle, FloatArray );
		}
	}
	else
	{
		throw Soy::AssertException("Unhandled element type [in array]");
	}
}

v8::Local<v8::Value> TShaderWrapper::SetUniform(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TShaderWrapper>( ThisHandle );
	auto pShader = This.mShader;
	auto& Shader = *pShader;
	
	auto* UniformName = *String::Utf8Value(Arguments[0]);
	auto Uniform = Shader.GetUniform( UniformName );
	if ( !Uniform.IsValid() )
	{
		std::stringstream Error;
		Error << "Shader missing uniform " << UniformName;
		throw Soy::AssertException(Error.str());
	}

	//	get type from args
	//	gr: we dont have vector types yet, so use arrays
	auto ValueHandle = Arguments[1];
	
	if ( SoyGraphics::TElementType::IsImage(Uniform.mType) )
	{
		//	gr: we're not using the shader state, so we currently need to manually track bind count at high level
		auto BindIndexHandle = Arguments[2];
		if ( !BindIndexHandle->IsNumber() )
			throw Soy::AssertException("Currently need to pass texture bind index (increment from 0). SetUniform(Name,Image,BindIndex)");
		auto BindIndex = Local<Number>::Cast( BindIndexHandle )->Int32Value();
		
		//	get the image
		auto& Image = v8::GetObject<TImageWrapper>(ValueHandle);
		//	gr: planning ahead
		auto OnTextureLoaded = [&Image,pShader,Uniform,BindIndex]()
		{
			pShader->SetUniform( Uniform, Image.GetTexture(), BindIndex );
		};
		auto OnTextureError = [](const std::string& Error)
		{
			std::Debug << "Error loading texture " << Error << std::endl;
			std::Debug << "Todo: relay to promise" << std::endl;
		};
		Image.GetTexture( OnTextureLoaded, OnTextureError );
	}
	else if ( SoyGraphics::TElementType::IsFloat(Uniform.mType) )
	{
		BufferArray<float,100> Floats;
		EnumFloatArray( ValueHandle, GetArrayBridge(Floats) );
		Shader.SetUniform( Uniform, GetArrayBridge(Floats) );
	}
	else
	{
		throw Soy::AssertException("Currently only image & float uniform supported");
	}

	return v8::Undefined(Params.mIsolate);
}

Local<FunctionTemplate> TShaderWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	InstanceTemplate->SetInternalFieldCount(2);
	
	Container.BindFunction<SetUniform_FunctionName>( InstanceTemplate, SetUniform );

	return ConstructorFunc;
}

void TShaderWrapper::CreateShader(Opengl::TContext& Context,std::function<Opengl::TGeometry&()> GetGeo,const char* VertSource,const char* FragSource)
{
	//	this needs to be deffered along with the context..
	//	the TShader constructor needs to return a promise really
	if ( !Context.IsInitialised() )
		throw Soy::AssertException("Opengl context not yet initialised");
	
	auto& Geo = GetGeo();
	std::string VertSourceStr( VertSource );
	std::string FragSourceStr( FragSource );
	mShader.reset( new Opengl::TShader( VertSourceStr, FragSourceStr, Geo.mVertexDescription, "Shader", Context ) );

}

