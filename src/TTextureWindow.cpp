#include "TTextureWindow.h"
#include "TOpenglWindow.h"
#include "SoyOpengl.h"
#include "PopOpengl.h"


TTextureWindow::TTextureWindow(std::string Name,vec2f Position,vec2f Size,TPopOpengl& Parent) :
	mParent		( Parent )
{
	mWindow.reset( new TOpenglWindow( Name, Position, Size ) );
	if ( !mWindow->IsValid() )
	{
		mWindow.reset();
		return;
	}
	
	mWindow->mOnRender.AddListener(*this,&TTextureWindow::OnOpenglRender);
}

TTextureWindow::~TTextureWindow()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
	
}


vec2f GetFrameBufferSize(GLint FrameBufferId)
{
	vec2f Error(200,200);

	
	auto BindScope = SoyScope( [FrameBufferId]{ glBindRenderbuffer( GL_RENDERBUFFER, FrameBufferId ); }, []{ glBindRenderbuffer( GL_RENDERBUFFER, 0 ); } );
	//	auto BindScope = SoyScope( [FrameBufferId]{ glBindRenderbuffer( GL_FRAMEBUFFER, FrameBufferId ); }, []{ glBindRenderbuffer( GL_FRAMEBUFFER, 0 ); } );
	Opengl_IsOkay();
	
	//	0 = screen
	//	glGetFramebufferAttachmentParameteriv( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,

	GLint FrameBufferObjectType;
	glGetFramebufferAttachmentParameteriv( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &FrameBufferObjectType );
	Opengl_IsOkay();
	
	
	if ( FrameBufferObjectType == GL_NONE )
		return Error;
	
	if ( FrameBufferObjectType == GL_FRAMEBUFFER_DEFAULT )
	{
		return Error;
	}
	
	if ( FrameBufferObjectType == GL_TEXTURE )
	{
		return Error;
	}

	if ( FrameBufferObjectType == GL_RENDERBUFFER )
	{
		GLint Width,Height;
		glGetRenderbufferParameteriv( GL_FRAMEBUFFER, GL_RENDERBUFFER_WIDTH, &Width );
		glGetRenderbufferParameteriv( GL_FRAMEBUFFER, GL_RENDERBUFFER_WIDTH, &Height );
		return vec2f( Width, Height );
	}

	Soy::Assert( false, std::stringstream() << "Unknown frame buffer object type " << FrameBufferObjectType );
	return Error;
}


void TTextureWindow::OnOpenglRender(Opengl::TRenderTarget& RenderTarget)
{
	
	/*
	//	load copy movie program
	if ( !mTextureCopyProgram.IsValid() )
	{
		auto& ErrorStream = std::Debug;
		mTextureCopyProgram = BuildProgram(
										"uniform highp mat4 Mvpm;\n"
										"attribute vec4 Position;\n"
										"attribute vec2 TexCoord;\n"
										"varying  highp vec2 oTexCoord;\n"
										"void main()\n"
										"{\n"
										"   gl_Position = Position;\n"
										"   oTexCoord = TexCoord;\n"
										"}\n"
										,
										"#extension GL_OES_EGL_image_external : require\n"
										"uniform samplerExternalOES Texture0;\n"
										"varying highp vec2 oTexCoord;\n"
										"void main()\n"
										"{\n"
										"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
										"}\n",
										   ErrorStream
										);

	}
	*/

	auto FrameBufferSize = RenderTarget.GetSize();
	
	
	Soy::TCamera Camera;
	Soy::Rectf OrthoRect( 0, 0, 1, 1 );
	//	note upside down order
	Camera.mProjectionMtx =	mathfu::OrthoHelper( OrthoRect.x, OrthoRect.w, OrthoRect.h, OrthoRect.y, Camera.mDepthNear, Camera.mDepthFar );
	Opengl::SetViewport( Soy::Rectf( FrameBufferSize ) );
	Opengl::ClearColour( Soy::TRgb(0,0,1) );
	Opengl::ClearDepth();
	glDisable(GL_DEPTH_TEST);
	
	
	if ( !mTestTexture )
	{
		SoyPixels mPendingTexture;
		mPendingTexture.Init( 256, 256, SoyPixelsFormat::RGB );
		BufferArray<char,3> Rgb;
		Rgb.PushBack( 255 );
		Rgb.PushBack( 255 );
		Rgb.PushBack( 0 );
		mPendingTexture.SetColour( GetArrayBridge(Rgb) );
		SoyPixelsMetaFull Meta( mPendingTexture.GetWidth(), mPendingTexture.GetHeight(), mPendingTexture.GetFormat() );
		
		mTestTexture.reset( new Opengl::TTexture( Meta, GL_TEXTURE_2D ) );
		mTestTexture->Copy( mPendingTexture, false, true );
	}

	
	Array<Opengl::TTexture> Textures;
	if ( mTestTexture )
		Textures.PushBack( *mTestTexture );
	for ( int rt=0;	rt<mParent.mRenderTargets.GetSize();	rt++ )
	{
		auto& RenderTarget = *mParent.mRenderTargets[rt];
		auto Texture = RenderTarget.GetTexture();
		if ( !Texture.IsValid() )
			continue;
		Textures.PushBack(Texture);
	}

	//	render all render target textures
	if ( !Textures.IsEmpty() )
	{
		Soy::Rectf Rect( 0,0,1.0,1/static_cast<float>(Textures.GetSize()) );
		
		for ( int rt=0;	rt<Textures.GetSize();	rt++ )
		{
			auto Texture = Textures[rt];
			if ( !Texture.IsValid() )
				continue;

			DrawQuad( Texture, Rect );
			Rect.y += Rect.h;
		}
	}
	
	Opengl_IsOkay();
}

Opengl::TContext* TTextureWindow::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}



void TTextureWindow::DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect)
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
		mBlitQuad = Opengl::CreateGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex );
	}

	//	allocate objects we need!
	if ( !mBlitShader.IsValid() )
	{
		/*
		auto VertShader =	"uniform vec4 Rect;\n"
							"attribute vec2 TexCoord;\n"
							"varying vec2 oTexCoord;\n"
							"void main()\n"
							"{\n"
							"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,0) * Rect.zwzw;\n"
							"   gl_Position.xy += Rect.xy;\n"
							"   oTexCoord = TexCoord;\n"
							"}\n";
		auto FragShader =	//"#extension GL_OES_EGL_image_external : require\n"
							//"uniform samplerExternalOES Texture0;\n"
							"uniform sampler2D Texture0;\n"
							"varying vec2 oTexCoord;\n"
							"void main()\n"
							"{\n"
							"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
							"}\n";
		*/
		
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
		"	oTexCoord = TexCoord;\n"
		"}\n";
		auto FragShader =
		"varying vec2 oTexCoord;\n"
		"uniform sampler2D Texture0;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = vec4(oTexCoord.x,oTexCoord.y,0,1);\n"
		//"	gl_FragColor = texture2D(Texture0,oTexCoord);\n"
		"}\n";

		mBlitShader = Opengl::BuildProgram( VertShader, FragShader, mBlitQuad.mVertexDescription, "Blit shader" );
	}
	
	
	//	do bindings
	auto Shader = mBlitShader.Bind();
	//Shader.SetUniform("Texture0", Texture );
	Shader.SetUniform("Rect", Soy::RectToVector(Rect) );
	mBlitQuad.Draw();
	
		
	
	/*
	 if ( mTextureCopyProgram.IsValid() )
	 {
		glUseProgram( mTextureCopyProgram.program );
	 //	UnitSquare.Draw();
		glUseProgram( 0 );
		Opengl_IsOkay();
	 }

	 
	 glEnable(GL_TEXTURE_2D);
	 if ( Texture.Bind() )
	 glColor3f(1,1,1);
	 else
	 glColor3f(1,0,0);
	 
	 {
	 glBegin(GL_QUADS);
	 {
	 glVertex3f(  Rect.Left(), Rect.Top(), z );
	 glTexCoord2f(  0.0,  0.0	);
	 
	 glVertex3f( Rect.Right(), Rect.Top(), z	);
	 glTexCoord2f(  1.0,  0.0	);
	 
	 glVertex3f( Rect.Right(), Rect.Bottom(), z	);
	 glTexCoord2f(  1.0,  1.0	);
	 
	 glVertex3f( Rect.Left(), Rect.Bottom(), z	);
	 glTexCoord2f(  0.0,  1.0	);
	 }
	 glEnd();
	 Texture.Unbind();
	 }
	 

	 */
}


