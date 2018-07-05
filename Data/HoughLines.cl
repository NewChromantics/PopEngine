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

/*
float TimeAlongLine2(float2 Position,float2 Start,float2 End)
{
	float2 Direction = End - Start;
	float DirectionLength = length(Direction);
	float Projection = dot( Position - Start, Direction) / (DirectionLength*DirectionLength);
	
	return Projection;
}

int GetHoughLineChunkIndex(THoughLine HoughLine,float2 Position,int ChunkCount)
{
	float Chunkf = TimeAlongLine2( Position, HoughLine.Start, HoughLine.End );
	int Chunk = (int)( Chunkf * ChunkCount );
	return Chunk;
}
*/
kernel void CalcAngleXDistanceXChunks(int xFirst,
										int yFirst,
										int AngleIndexFirst,
										global float* Angles,
									  	global float* Distances,
										image2d_t EdgeTexture
									  )
{
	uint3 id = (uint3)(xFirst,yFirst,AngleIndexFirst);
	id.x += get_global_id(0);
	id.y += get_global_id(1);
	id.z += get_global_id(2);
	
	int x = id.x;
	int y = id.y;
	int AngleIndex = id.z;
	float Angle = Angles[AngleIndex];
	
	//	read edge
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float4 Edge4 = read_imagef( EdgeTexture, Sampler, (int2)(x,y) );
	bool Edge = (Edge4.x > 0.5f);
	if ( !Edge )
		return;
	
	float2 HoughOrigin = float2( HoughOriginX, HoughOriginY );
	/*
	//	calc hough distance from position & angle
	float2 uv = float2( x / (float)EdgeTextureWidth, y / (float)EdgeTextureHeight );
	float HoughDistance = GetHoughDistance( uv, HoughOrigin, Angle );
	int HoughDistanceIndex = GetHoughDistanceIndex( HoughDistance );
	
	THoughLine Line = GetHoughLine( Angle, HoughDistance, HoughOrigin );
	int ChunkIndex = GetHoughLineChunkIndex( Line, uv, ChunkCount );
	
	int AngleXDistanceXChunkIndex = GetAngleXDistanceXChunkIndex( AngleIndex, HoughDistanceIndex, ChunkIndex );
	
	//	gr: this is writing odd values
	InterlockedAdd( AngleXDistanceXChunks[AngleXDistanceXChunkIndex], 1 );
	 */
}


/*
[numthreads(32,32,1)]
void GraphAngleXDistances(uint3 id : SV_DispatchThreadID)
{
	int x = id.x;
	int y = id.y;
	
	float u = x / (float)GraphTextureWidth;
	float v = y / (float)GraphTextureHeight;
	
	int AngleIndex = u * GetAngleCount();
	int DistanceIndex = v * GetDistanceCount();
	
	int HitCount = 0;
	for ( int c=0;	c<ChunkCount;	c++ )
	{
		int AngleXDistanceXChunkIndex = GetAngleXDistanceXChunkIndex(AngleIndex, DistanceIndex, c );
		HitCount += AngleXDistanceXChunks[AngleXDistanceXChunkIndex];
	}
	
	float HitMax = GetHistogramMax();
	float Score = HitCount / HitMax;
	Score = min( 1, Score );
	
	GraphTexture[id.xy] = float4( Score, Score, 0, 1.0f );
}
 */


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
