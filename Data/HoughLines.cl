#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
/*
 // Create a RenderTexture with enableRandomWrite flag and set it
 // with cs.SetTexture
 Texture2D<float4> EdgeTexture;
 int EdgeTextureWidth;
 int EdgeTextureHeight;
 RWTexture2D<float4> GraphTexture;
 int GraphTextureWidth;
 int GraphTextureHeight;
 RWStructuredBuffer<int> AngleXDistanceXChunks;
 StructuredBuffer<float> Angles;
 StructuredBuffer<float> Distances;
 int ChunkCount = 10;
 AppendStructuredBuffer<THoughLine> ExtractedLines;
 float ExtractMinScore = 0.5f;
 float ExtractMinJoinedScore = 1.0f;
 float HoughOriginX = 0.5f;
 float HoughOriginY = 0.5f;
 int HistogramMax = 1000;
 */

constant float HoughOriginX = 0.5f;
constant float HoughOriginY = 0.5f;
#define hypotenuse(o,a)	sqrt( (float)((a*a)+(o*o)) )

typedef struct tag_THoughLine
{
	float2	Start;
	float2	End;
	float	Score;
} THoughLine;


static float TimeAlongLine2(float2 Position,float2 Start,float2 End)
{
	float2 Direction = End - Start;
	float DirectionLength = length(Direction);
	float Projection = dot( Position - Start, Direction) / (DirectionLength*DirectionLength);
	
	return Projection;
}

static int GetHoughLineChunkIndex(THoughLine HoughLine,float2 Position,int ChunkCount)
{
	float Chunkf = TimeAlongLine2( Position, HoughLine.Start, HoughLine.End );
	int Chunk = (int)( Chunkf * ChunkCount );
	return Chunk;
}

static int GetAngleXDistanceXChunkIndex(int AngleIndex,int DistanceIndex,int ChunkIndex,int DistanceCount,int ChunkCount,int AngleXDistanceXChunkCount)
{
	int AngleXDistanceXChunkIndex = (AngleIndex * DistanceCount * ChunkCount);
	AngleXDistanceXChunkIndex += DistanceIndex * ChunkCount;
	AngleXDistanceXChunkIndex += ChunkIndex;
	
	int MaxAngleXDistanceXChunkIndex = AngleXDistanceXChunkCount-1;
	//	just in case...
	return max(0,min(MaxAngleXDistanceXChunkIndex,AngleXDistanceXChunkIndex));
}


static float GetHoughDistance(float2 Position,float2 Origin,float Angle)
{
	//	http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	float2 xy = Position - Origin;
	float Cos = cos( radians(Angle) );
	float Sin = sin( radians(Angle) );
	float r = Cos*xy.x + Sin*xy.y;
	return r;
}

static int GetHoughDistanceIndex(float Distance,global float* Distances,int DistanceCount)
{
	//	calc this with range & floor
	for ( int i=DistanceCount-1;	i>0;	i-- )
	{
		if ( Distance >= Distances[i] )
			return i;
	}
	return 0;
}


//	same as THoughLine.cginc!
static THoughLine GetHoughLine(float Angle,float Distance,float2 Origin)
{
	//	UV space lines
	float Length = hypotenuse(1,1);
	
	float rho = Distance;
	float theta = radians(Angle);
	float Cos = cos( theta );
	float Sin = sin( theta );
	
	//	center of the line
	float2 Center = (float2)( Cos*rho, Sin*rho ) + Origin;
	
	//	scale by an arbirtry number, but still want to be resolution-independent
	//float Length = 100;
	
	float2 Offset = (float2)( Length*-Sin, Length*Cos );
	
	THoughLine Line;
	Line.Start = Center + Offset;
	Line.End = Center - Offset;
	Line.Score = 0;
	return Line;
}
 

kernel void CalcAngleXDistanceXChunks(int xFirst,
										int yFirst,
										int AngleIndexFirst,
										global float* Angles,
									  	global float* Distances,
									  	global int* AngleXDistanceXChunks,
									  	int DistanceCount,
									 	int ChunkCount,
									  	int AngleXDistanceXChunkCount,
										image2d_t EdgeTexture
									  )
{
	int x = get_global_id(0) + xFirst;
	int y = get_global_id(1) + yFirst;
	int AngleIndex = get_global_id(2) + AngleIndexFirst;
	float Angle = Angles[AngleIndex];
	
	//	read edge
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float4 Edge4 = read_imagef( EdgeTexture, Sampler, (int2)(x,y) );
	bool Edge = (Edge4.x > 0.5f);
	if ( !Edge )
		return;
	
	float EdgeTextureWidthf = get_image_width(EdgeTexture);
	float EdgeTextureHeightf = get_image_height(EdgeTexture);

	float2 HoughOrigin = float2( HoughOriginX, HoughOriginY );
	
	//	calc hough distance from position & angle
	float2 uv = (float2)( x / EdgeTextureWidthf, y / EdgeTextureHeightf );

	float HoughDistance = GetHoughDistance( uv, HoughOrigin, Angle );
	int HoughDistanceIndex = GetHoughDistanceIndex( HoughDistance, Distances, DistanceCount );

	THoughLine Line = GetHoughLine( Angle, HoughDistance, HoughOrigin );
	int ChunkIndex = GetHoughLineChunkIndex( Line, uv, ChunkCount );

	int AngleXDistanceXChunkIndex = GetAngleXDistanceXChunkIndex( AngleIndex, HoughDistanceIndex, ChunkIndex, DistanceCount, ChunkCount, AngleXDistanceXChunkCount );
	
	//	gr: this is writing odd values
	atom_inc( &AngleXDistanceXChunks[AngleXDistanceXChunkIndex] );
}



kernel void GraphAngleXDistances(int xFirst,
						  int yFirst,
						 // write_only image2d_t GraphTexture,
						  int HistogramHitMax,
						  int AngleCount,
						  int DistanceCount,
						  int ChunkCount,
						  global int* AngleXDistanceXChunks,
						 int AngleXDistanceXChunkCount
						  )
{
	/*
	int x = get_global_id(0) + xFirst;
	int y = get_global_id(1) + yFirst;
	
	float u = x / (float)get_image_width(GraphTexture);
	float v = y / (float)get_image_height(GraphTexture);
	
	int AngleIndex = u * AngleCount;
	int DistanceIndex = v * DistanceCount;

	int HitCount = 0;
	for ( int c=0;	c<ChunkCount;	c++ )
	{
		int AngleXDistanceXChunkIndex = GetAngleXDistanceXChunkIndex(AngleIndex, DistanceIndex, c, DistanceCount, ChunkCount, AngleXDistanceXChunkCount );
		HitCount += AngleXDistanceXChunks[AngleXDistanceXChunkIndex];
	}
	
	float HitMax = HistogramHitMax;
	float Score = HitCount / HitMax;
	Score = min( 1.0f, Score );
	
	int2 PixelCoord = (int2)(x,y);
	float4 Colour = (float4)( Score, Score, 0, 1.0f );
	write_imagef( GraphTexture, PixelCoord, Colour );
 */
}



/*
[numthreads(32,32,1)]
void ExtractHoughLines(uint3 id : SV_DispatchThreadID)
{
	THoughLineMeta Line;
	Line.Origin = float2( HoughOriginX, HoughOriginY );
	Line.AngleIndex = id.x;
	Line.DistanceIndex = id.y;
	
	for ( int ChunkIndex=0;	ChunkIndex<ChunkCount;	ChunkIndex++ )
	{
		float Score = GetHoughLineChunkScore( Line, ChunkIndex, false );
		if ( Score < ExtractMinScore )
			continue;
		
		//	renormalise score
		if ( RENORMALISE_SCORE )
			Score = (Score-ExtractMinScore) / (1.0f-ExtractMinScore);
		PushLine( Line, Score, ChunkIndex, ChunkIndex );
	}
}
*/
