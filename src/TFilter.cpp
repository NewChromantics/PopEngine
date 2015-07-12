#include "TFilter.h"
#include "TFilterWindow.h"



class TOpenglJob_UploadPixels : public Opengl::TJob
{
public:
	TOpenglJob_UploadPixels(std::shared_ptr<SoyPixels>& Pixels,std::shared_ptr<TFilterFrame>& Frame) :
		mPixels	( Pixels ),
		mFrame	( Frame )
	{
	}
	
	virtual bool		Run(std::ostream& Error);

	std::shared_ptr<SoyPixels>		mPixels;
	std::shared_ptr<TFilterFrame>	mFrame;
};


class TFilterJobRun : public Opengl::TJob
{
public:
	TFilterJobRun(TFilter& Filter,std::shared_ptr<TFilterFrame>& Frame) :
		mFrame		( Frame ),
		mFilter		( &Filter )
	{
	}

	virtual bool		Run(std::ostream& Error);
	
	//	eek, no safe pointer here!
	TFilter*						mFilter;
	std::shared_ptr<TFilterFrame>	mFrame;
};



bool TOpenglJob_UploadPixels::Run(std::ostream& Error)
{
	auto& Frame = *mFrame;
	auto& Pixels = *mPixels;
	
	//	make texture if it doesn't exist
	if ( !Frame.mFrame.IsValid() )
	{
		SoyPixelsMetaFull Meta( Pixels.GetWidth(), Pixels.GetHeight(), Pixels.GetFormat() );
		Frame.mFrame = Opengl::TTexture( Meta, GL_TEXTURE_2D );
	}
	
	Frame.mFrame.Copy( Pixels, true, true );
	return true;
}


bool TFilterJobRun::Run(std::ostream& Error)
{
	auto& Frame = *mFrame;
	auto& FrameTexture = Frame.mFrame;
	auto& Filter = *mFilter;
	
	Soy::TRgb DebugClearColours[] =
	{
		Soy::TRgb(1,0,0),
		Soy::TRgb(1,1,0),
		Soy::TRgb(0,1,0),
		Soy::TRgb(0,1,1),
		Soy::TRgb(0,0,1),
		Soy::TRgb(1,0,1),
	};
	
	//	run through the shader chain
	for ( int s=0;	s<Filter.mShaders.GetSize();	s++ )
	{
		auto pShader = Filter.mShaders[s];
		if ( !pShader )
		{
			std::Debug << "Warning: Filter " << Filter.mName << " shader #" << s << " is null" << std::endl;
			continue;
		}
		
		if ( !pShader->mShader.IsValid() )
		{
			std::Debug << "Warning: Filter " << Filter.mName << " shader #" << s << " is invalid" << std::endl;
			continue;
		}
		
		//	get texture for this stage
		auto& StageTarget = Frame.mShaderTextures[pShader->mName];
		if ( !StageTarget.IsValid() )
		{
			SoyPixelsMetaFull Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), FrameTexture.GetFormat() );
			StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
		}
		
		//	gr: cache/pool these rendertargets?
		Opengl::TFboMeta FboMeta( pShader->mName, StageTarget.GetWidth(), StageTarget.GetHeight() );
		std::shared_ptr<Opengl::TRenderTarget> pRenderTarget( new Opengl::TRenderTargetFbo( FboMeta, StageTarget ) );
		auto& RenderTarget = *pRenderTarget;
		
		//	render this stage to the stage target fbo
		RenderTarget.Bind();
		{
			auto& StageShader = pShader->mShader;
			Opengl::ClearColour( DebugClearColours[s%sizeofarray(DebugClearColours)] );

			auto Shader = StageShader.Bind();
			//	gr: go through uniforms, find any named same as a shader and bind that shaders output
			for ( int u=0;	u<StageShader.mUniforms.GetSize();	u++ )
			{
				auto& Uniform = StageShader.mUniforms[u];
				//	gr: todo: check type
				auto UniformTexture = Frame.mShaderTextures.find(Uniform.mName);
				if ( UniformTexture == Frame.mShaderTextures.end() )
					continue;
				
				Shader.SetUniform( Uniform.mName.c_str(), UniformTexture->second );
			}
			Filter.mBlitQuad.Draw();
		}
		RenderTarget.Unbind();
	}
	
	return true;
}



TFilter::TFilter(const std::string& Name) :
	TFilterMeta		( Name )
{
	//	create window
	vec2f WindowPosition( 0, 0 );
	vec2f WindowSize( 400, 400 );
	
	mWindow.reset( new TFilterWindow( Name, WindowPosition, WindowSize, *this ) );
	
	auto CreateBlitGeo = [this]
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
		
		return true;
	};
	
	//	create blit geometry
	mWindow->GetContext()->PushJob( CreateBlitGeo );
}


void TFilter::LoadFrame(std::shared_ptr<SoyPixels>& Pixels,SoyTime Time)
{
	Soy::Assert( Time.IsValid(), "invalid frame time" );
	
	//	grab the frame (create if nececssary)
	std::shared_ptr<TFilterFrame> Frame;
	{
		auto FrameIt = mFrames.find( Time );
		if ( FrameIt == mFrames.end() )
		{
			Frame.reset( new TFilterFrame() );
			mFrames[Time] = Frame;
		}
		else
		{
			Frame = FrameIt->second;
		}
	}
	
	//	make up a job that holds the pixels to put it into a texture, then run to refresh everything
	auto& Context = GetContext();
	std::shared_ptr<Opengl::TJob> Job( new TOpenglJob_UploadPixels( Pixels, Frame ) );
	Context.PushJob( Job );
	
	//	trigger a run (maybe this will be automatic in future after success?)
	Run( Time );
}

void TFilter::Run(SoyTime Time)
{
	//	grab the frame
	auto FrameIt = mFrames.find( Time );
	Soy::Assert( FrameIt != mFrames.end(), "Frame does not exist" );
	std::shared_ptr<TFilterFrame> Frame = FrameIt->second;
	
	//	make up a job that holds the pixels to put it into a texture, then run to refresh everything
	auto& Context = GetContext();
	std::shared_ptr<Opengl::TJob> Job( new TFilterJobRun( *this, Frame ) );
	Context.PushJob( Job );
}

Opengl::TContext& TFilter::GetContext()
{
	auto* Context = mWindow->GetContext();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	return *Context;
}
	
