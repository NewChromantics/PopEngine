
[numthreads(8,8,1)]
void CalcAngleXDistanceXChunks(uint3 id : SV_DispatchThreadID)
{
	int x = id.x;
	int y = id.y;
	int AngleIndex = id.z;
	float Angle = GetAngle( AngleIndex );
	
	//	read edge
	bool Edge = (EdgeTexture[id.xy].x > 0.5f);
	if ( !Edge )
		return;
	
	float2 HoughOrigin = float2( HoughOriginX, HoughOriginY );
	
	//	calc hough distance from position & angle
	float2 uv = float2( x / (float)EdgeTextureWidth, y / (float)EdgeTextureHeight );
	float HoughDistance = GetHoughDistance( uv, HoughOrigin, Angle );
	int HoughDistanceIndex = GetHoughDistanceIndex( HoughDistance );
	
	THoughLine Line = GetHoughLine( Angle, HoughDistance, HoughOrigin );
	int ChunkIndex = GetHoughLineChunkIndex( Line, uv, ChunkCount );
	
	int AngleXDistanceXChunkIndex = GetAngleXDistanceXChunkIndex( AngleIndex, HoughDistanceIndex, ChunkIndex );
	
	//	gr: this is writing odd values
	InterlockedAdd( AngleXDistanceXChunks[AngleXDistanceXChunkIndex], 1 );
}



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

