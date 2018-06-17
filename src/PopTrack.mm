#include "PopTrack.h"
#include <TParameters.h>
#include <SoyDebug.h>
#include <TProtocolCli.h>
#include <TProtocolHttp.h>
#include <SoyApp.h>
#include <PopMain.h>
#include <TJobRelay.h>
#include <SoyPixels.h>
#include <SoyString.h>
#include <TFeatureBinRing.h>
#include <SortArray.h>
#include <TChannelLiteral.h>
#include <TChannelFile.h>
#include <SoyOpenglWindow.h>

#define FILTER_MAX_FRAMES	10
#define FILTER_MAX_THREADS	1
#define JOB_THREAD_COUNT	1

#include "include/libplatform/libplatform.h"
#include "include/v8.h"


namespace PopTrack
{
	namespace Private
	{
		//	keep alive after PopMain()
#if defined(TARGET_OSX_BUNDLE)
		std::shared_ptr<TPopTrack> gOpenglApp;
#endif
		
	}
	
	TPopTrack&	GetApp();
}



TPopTrack& PopTrack::GetApp()
{
	if ( !Private::gOpenglApp )
	{
		Private::gOpenglApp.reset( new TPopTrack("PopEngine") );
	}
	return *Private::gOpenglApp;
}



TPopAppError::Type PopMain()
{
	
	auto& App = PopTrack::GetApp();
	
#if !defined(TARGET_OSX_BUNDLE)
	//	run
	App.mConsoleApp.WaitForExit();
#endif

	return TPopAppError::Success;
}



TPopTrack::TPopTrack(const std::string& WindowName)
{
	Soy::Rectf Rect( 0, 0, 300, 300 );
	TOpenglParams Params;
	
	mWindow.reset( new TOpenglWindow( WindowName, Rect, Params ) );
	if ( !mWindow->IsValid() )
	{
		mWindow.reset();
		return;
	}
	
	mWindow->mOnRender.AddListener(*this,&TPopTrack::OnOpenglRender);
	
	TestV8();
}

TPopTrack::~TPopTrack()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
	
}



void TPopTrack::OnOpenglRender(Opengl::TRenderTarget& RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
	Opengl::ClearDepth();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	//	make rendering tile rect
	Soy::Rectf TileRect( 0, 0, 1,1);
	
	auto OpenglContext = this->GetContext();
	
	//DrawQuad( nullptr, TileRect );
	
	Opengl_IsOkay();
}

std::shared_ptr<Opengl::TContext> TPopTrack::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}



void TPopTrack::DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect)
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
		UvAttrib.mArraySize = 2;
	
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	}
	
	//	allocate objects we need!
	if ( !mBlitShader )
	{
		auto& Context = *GetContext();
		
		auto VertShader =
		"uniform vec4 Rect;\n"
		"attribute vec2 TexCoord;\n"
		"varying vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
		"   gl_Position.xy *= Rect.zw;\n"
		"   gl_Position.xy += Rect.xy;\n"
		//	move to view space 0..1 to -1..1
		"	gl_Position.xy *= vec2(2,2);\n"
		"	gl_Position.xy -= vec2(1,1);\n"
		"	oTexCoord = vec2(TexCoord.x,1-TexCoord.y);\n"
		"}\n";
		auto FragShader =
		"varying vec2 oTexCoord;\n"
		"uniform sampler2D Texture0;\n"
		"void main()\n"
		"{\n"
		//"	gl_FragColor = vec4(oTexCoord.x,oTexCoord.y,0,1);\n"
		"	gl_FragColor = texture2D(Texture0,oTexCoord);\n"
		"}\n";
		
		mBlitShader.reset( new Opengl::TShader( VertShader, FragShader, mBlitQuad->mVertexDescription, "Blit shader", Context ) );
	}
	
	//	do bindings
	auto Shader = mBlitShader->Bind();
	Shader.SetUniform("Texture0", Texture );
	Shader.SetUniform("Rect", Soy::RectToVector(Rect) );
	mBlitQuad->Draw();
	
}

class PopV8Allocator : public v8::ArrayBuffer::Allocator
{
public:
	
	virtual void* Allocate(size_t length) override;
	virtual void* AllocateUninitialized(size_t length) override;
	virtual void Free(void* data, size_t length) override;
};


auto JavascriptMain = R"V0G0N(

function test_function()
{
	return "hello";
}


)V0G0N";

void TPopTrack::TestV8()
{
	PopV8Allocator Allocator;
	//v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeICU(nullptr);
	//v8::V8::InitializeExternalStartupData(argv[0]);
	v8::V8::InitializeExternalStartupData(nullptr);
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	auto platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(platform);
	v8::V8::Initialize();
	// Create a new Isolate and make it the current one.
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &Allocator;
	
	v8::Isolate* isolate = v8::Isolate::New(create_params);
	{
		v8::Isolate::Scope isolate_scope(isolate);
		// Create a stack-allocated handle scope.
		v8::HandleScope handle_scope(isolate);
		// Create a new context.
		v8::Local<v8::Context> context = v8::Context::New(isolate);
		// Enter the context for compiling and running the hello world script.
		v8::Context::Scope context_scope(context);
		// Create a string containing the JavaScript source code.
		auto source = v8::String::NewFromUtf8(isolate, JavascriptMain, v8::NewStringType::kNormal).ToLocalChecked();
		
		/*
		Handle<v8::Object> global = context->Global();
		Handle<v8::Value> value = global->Get(String::New("test_function"));
		Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(value);
		Handle<Value> args[2];
		Handle<Value> js_result;
		int final_result;
		args[0] = v8::String::New("1");
		args[1] = v8::String::New("1");
		js_result = func->Call(global, 2, args);
		String::AsciiValue ascii(js_result);
		*/
		
		// Compile the source code.
		v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
		// Run the script to get the result.
		auto mainresult = script->Run(context).ToLocalChecked();
		
		auto ContextGlobal = context->Global();
		auto FuncNameKey = v8::String::NewFromUtf8( isolate, "test_function", v8::NewStringType::kNormal ).ToLocalChecked();
	
		//v8::String::NewFromUtf8(isolate, "'Hello' + ', World!'",v8::NewStringType::kNormal)
		auto FuncName = ContextGlobal->Get(FuncNameKey);
										   
		auto Func = v8::Handle<v8::Function>::Cast(FuncName);
		
		v8::Handle<v8::Value> args[0];
		auto result = Func->Call( context, Func, 0, args ).ToLocalChecked();
		
		
		// Convert the result to an UTF8 string and print it.
		v8::String::Utf8Value ResultStr(result);
		printf("result = %s\n", *ResultStr);

		v8::String::Utf8Value MainResultStr(mainresult);
		printf("MainResultStr = %s\n", *MainResultStr);
	}

}



void* PopV8Allocator::Allocate(size_t length)
{
	auto* Bytes = new uint8_t[length];
	for ( auto i=0;	i<length;	i++ )
		Bytes[i] = 0;
	return Bytes;
}

void* PopV8Allocator::AllocateUninitialized(size_t length)
{
	return Allocate( length );
}

void PopV8Allocator::Free(void* data, size_t length)
{
	auto* Bytes = static_cast<uint8_t*>( data );
	delete[] Bytes;
}



