#include "TFilterWindow.h"
#include <SoyOpenglWindow.h>
#include <SoyOpengl.h>
#include "TFilter.h"


TFilterWindow::TFilterWindow(std::string Name,vec2f Position,vec2f Size,TFilter& Parent) :
	mParent		( Parent ),
	mZoom		( 0.f ),
	mZoomPosPx	( 0,0 )
{
	mWindow.reset( new TOpenglWindow( Name, Position, Size ) );
	if ( !mWindow->IsValid() )
	{
		mWindow.reset();
		return;
	}
	
	mWindow->mOnRender.AddListener(*this,&TFilterWindow::OnOpenglRender);
	mWindow->mOnMouseDown.AddListener( *this, &TFilterWindow::OnMouseDown );
	mWindow->mOnMouseMove.AddListener( *this, &TFilterWindow::OnMouseMove );
	mWindow->mOnMouseUp.AddListener( *this, &TFilterWindow::OnMouseUp );
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
	
	//	zoom viewport
	if ( mZoomFunc )
		mZoomFunc();
	
	
	Soy::Rectf ZoomOutViewport(0,0,1,1);
	Soy::Rectf ZoomInViewport(0,0,1,1);
	
	{
		auto FrameRect = Soy::Rectf( FrameBufferSize );
		static float MaxZoomScale = 8.f;
		float ZoomScale = 1.f + (MaxZoomScale);

		vec2f Center = mZoomPosPx;
		//Center.x = FrameRect.w - Center.x;
		Center.y = FrameRect.h - Center.y;
		Center.x /= FrameBufferSize.w;
		Center.y /= FrameBufferSize.h;
		
		ZoomInViewport.w /= ZoomScale;
		ZoomInViewport.h /= ZoomScale;
		ZoomInViewport.x = Center.x - (ZoomInViewport.w/2.f);
		ZoomInViewport.y = Center.y - (ZoomInViewport.h/2.f);
	}
	Soy::Rectf Viewport;
	Viewport.x = Soy::Lerp( ZoomOutViewport.x, ZoomInViewport.x, mZoom );
	Viewport.y = Soy::Lerp( ZoomOutViewport.y, ZoomInViewport.y, mZoom );
	Viewport.w = Soy::Lerp( ZoomOutViewport.w, ZoomInViewport.w, mZoom );
	Viewport.h = Soy::Lerp( ZoomOutViewport.h, ZoomInViewport.h, mZoom );
	RenderTarget.SetViewportNormalised( Viewport );

	Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
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
			if ( Frame.mFrameTexture.IsValid() )
			{
				DrawQuad( Frame.mFrameTexture, TileRect );
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

void TFilterWindow::OnMouseDown(const TMousePos& Pos)
{
	mZoomPosPx = Pos;
	mZoomFunc = [this]()
	{
		static float ZoomSpeed = 0.1f;
		if ( mZoom < 1.f )
			mZoom = std::min( 1.f, mZoom + ZoomSpeed );
	};
}

void TFilterWindow::OnMouseMove(const TMousePos& Pos)
{
	mZoomPosPx = Pos;
}

void TFilterWindow::OnMouseUp(const TMousePos& Pos)
{
	mZoomFunc = [this]()
	{
		static float ZoomSpeed = 0.1f;
		if ( mZoom > 0.f )
			mZoom = std::max( 0.f, mZoom - ZoomSpeed );
	};
}


void TFilterWindow::DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect)
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
		mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex, *GetContext() ) );
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


