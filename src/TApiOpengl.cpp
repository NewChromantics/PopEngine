#include "TApiOpengl.h"
#include "SoyOpenglWindow.h"

using namespace v8;

const char DrawQuad_FunctionName[] = "DrawQuad";
const char ClearColour_FunctionName[] = "ClearColour";

void ApiOpengl::Bind(TV8Container& Container)
{
	Container.BindObjectType("OpenglWindow", TWindowWrapper::CreateTemplate );
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
	auto* Container = reinterpret_cast<TV8Container*>( Local<External>::Cast( Arguments.Data() )->Value() );
	
	
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
	
	NewWindow->mContainer = Container;
	
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
	auto* This = reinterpret_cast<TWindowWrapper*>( Local<External>::Cast(ThisHandle)->Value() );
	
	Soy::Rectf Rect(0,0,1,1);
	This->mWindow->DrawQuad( Rect );
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


void TRenderWindow::DrawQuad(Soy::Rectf Rect)
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
	
	//	allocate objects we need!
	if ( !mBlitShader )
	{
		auto& Context = *GetContext();
		
		auto VertShader =
		"#version 410\n"
		"uniform vec4 Rect;\n"
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
		
		mBlitShader.reset( new Opengl::TShader( VertShader, FragShader, mBlitQuad->mVertexDescription, "Blit shader", Context ) );
	}
	
	//	do bindings
	auto Shader = mBlitShader->Bind();
	Shader.SetUniform("Rect", Soy::RectToVector(Rect) );
	mBlitQuad->Draw();
	Opengl_IsOkay();
}
