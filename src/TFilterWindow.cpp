#include "TFilterWindow.h"
#include "TOpenglWindow.h"
#include "SoyOpengl.h"
#include "TFilter.h"


TFilterWindow::TFilterWindow(std::string Name,vec2f Position,vec2f Size,TFilter& Parent) :
	mParent		( Parent )
{
	mWindow.reset( new TOpenglWindow( Name, Position, Size ) );
	if ( !mWindow->IsValid() )
	{
		mWindow.reset();
		return;
	}
	
	mWindow->mOnRender.AddListener(*this,&TFilterWindow::OnOpenglRender);
}

TFilterWindow::~TFilterWindow()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
	
}



void TFilterWindow::OnOpenglRender(Opengl::TRenderTarget& RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	Opengl::SetViewport( Soy::Rectf( FrameBufferSize ) );
	Opengl::ClearColour( Soy::TRgb(0.05f,0.05f,0) );
	Opengl::ClearDepth();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	//	collect N frames
	static int ShowMaxFrames = 3;
	static int SkipFirstNFrames = 0;

	int SkipCount = SkipFirstNFrames;
	Array<std::shared_ptr<TFilterFrame>> Frames;
	for ( auto fit=mParent.mFrames.rbegin();	fit!=mParent.mFrames.rend();	fit++ )
	{
		if ( SkipCount-- > 0 )
			continue;
		auto pFrame = fit->second;
		if ( !pFrame )
			continue;
		Frames.PushBack( pFrame );
		if ( Frames.GetSize() >= ShowMaxFrames )
			break;
	}
	
	auto FrameCount = Frames.GetSize();
	if ( FrameCount == 0 )
		return;
	size_t StageCount = 0;
	for ( int f=0;	f<Frames.GetSize();	f++ )
		StageCount = std::max( StageCount, Frames[f]->mStageData.size() );
	//	+1 for source texture
	StageCount++;
	
	//	make rendering tile rect
	Soy::Rectf TileRect( 0, 0, 1.f/static_cast<float>(StageCount), 1.f/static_cast<float>(FrameCount) );
	
	for ( int f=0;	f<Frames.GetSize();	f++ )
	{
		auto& Frame = *Frames[f];
		auto& StageDatas = Frame.mStageData;
		
		//	render source texture
		{
			if ( Frame.mFrame.IsValid() )
			{
				DrawQuad( Frame.mFrame, TileRect );
			}
			//	next col
			TileRect.x += TileRect.w;
		}
		
		for ( auto s=StageDatas.begin();	s!=StageDatas.end();	s++ )
		{
			//auto& StageName = s->first;
			auto& StageData = s->second;
			if ( StageData )
			{
				auto StageTexture = StageData->GetTexture();
			
				if ( StageTexture.IsValid() )
				{
					DrawQuad( StageTexture, TileRect );
				}
			}
			
			//	next col
			TileRect.x += TileRect.w;
		}
		
		//	next row
		TileRect.y += TileRect.h;
		TileRect.x = 0;
	}
	Opengl_IsOkay();
}

Opengl::TContext* TFilterWindow::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}



void TFilterWindow::DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect)
{
	if ( !mBlitQuad.IsValid() )
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
		Array<GLshort> Indexes;
		Indexes.PushBack( 0 );
		Indexes.PushBack( 1 );
		Indexes.PushBack( 2 );

		Indexes.PushBack( 2 );
		Indexes.PushBack( 3 );
		Indexes.PushBack( 0 );
		
		//	for each part of the vertex, add an attribute to describe the overall vertex
		Opengl::TGeometryVertex Vertex;
		auto& UvAttrib = Vertex.mElements.PushBack();
		UvAttrib.mName = "TexCoord";
		UvAttrib.mType = GL_FLOAT;
		UvAttrib.mIndex = 0;	//	gr: does this matter?
		UvAttrib.mArraySize = 2;
		UvAttrib.mElementDataSize = sizeof( Mesh.mVertexes[0].uv );
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad = Opengl::CreateGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex, *GetContext() );
	}

	//	allocate objects we need!
	if ( !mBlitShader.IsValid() )
	{
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

		mBlitShader = Opengl::BuildProgram( VertShader, FragShader, mBlitQuad.mVertexDescription, "Blit shader" );
	}
	
	//	do bindings
	auto Shader = mBlitShader.Bind();
	Shader.SetUniform("Texture0", Texture );
	Shader.SetUniform("Rect", Soy::RectToVector(Rect) );
	mBlitQuad.Draw();

}


