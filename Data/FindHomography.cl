

kernel void GetTestHomography(volatile global float16* ResultHomographys)
{
	ResultHomographys[0] = (float16)( 1,0,0,0,	0,1,0,0,	0,0,1,0,	0,0,0,1	);
}



static float16 GetHomographyMatrix(float16 Match4x2,float16 Truth4x2)
{
	float2 m0 = (float2)( Match4x2[0], Match4x2[1] );
	float2 m1 = (float2)( Match4x2[2], Match4x2[3] );
	float2 m2 = (float2)( Match4x2[4], Match4x2[5] );
	float2 m3 = (float2)( Match4x2[6], Match4x2[7] );
	float2 t0 = (float2)( Truth4x2[0], Truth4x2[1] );
	float2 t1 = (float2)( Truth4x2[2], Truth4x2[3] );
	float2 t2 = (float2)( Truth4x2[4], Truth4x2[5] );
	float2 t3 = (float2)( Truth4x2[6], Truth4x2[7] );
	
	return (float16)( 1,0,0,0,	0,1,0,0,	0,0,1,0,	0,0,0,1	);
}

static float Range01(float Min,float Max,float Value)
{
	float Range = (Value-Min) / (Max-Min);
	Range = max( 0.0f, Range );
	Range = min( 1.0f, Range );
	return Range;
}

static float FindHomography(float16 MatchRect,float16 TruthRect,global float2* MatchCorners,int MatchCornerCount,global float2* TruthCorners,int TruthCornerCount,float MaxMatchDistance,float16* ResultHomography)
{
	float16 Homo = GetHomographyMatrix( MatchRect, TruthRect );
	*ResultHomography = Homo;
	
	//	score the other truths by finding a match nearby
	float Score = 0;
	/*
	for ( int t=0;	t<TruthCornerCount;	t++ )
	{
		//	need to exclude sources here really
		float2 Truth2 = TruthCorners[t];
		float4 TruthInverse4 = HomographyMatrix * float4( Truth2.x, Truth2.y, 0, 1 );
		float2 TruthInverse2 = TruthInverse4.xy;
		
		float TClosest = 999;
		
		for ( int m=0;	m<MatchCornerCount;	m++ )
		{
			float2 Match2 = MatchCorners[m];
			float Distance = length( Match2, TruthInverse2 );
			if ( Distance > MaxMatchDistance )
				continue;
			TClosest = min( TClosest, Distance );
		}
		
		float TScore = 1 - Range01( 0, MaxMatchDistance, TClosest );
		Score += TScore;
	}
	Score /= (TruthCornerCount);
	*/

	return Score;
}


kernel void FindHomographies(	volatile global float16* ResultHomographys,
								volatile global float* ResultScores,
							 
								int MatchRectIndexFirst,
								global float16* MatchRects,
								int MatchRectCount,
								global float2* MatchCorners,
							 	int MatchCornerCount,
							 
							 	int TruthRectIndexFirst,
								global float16* TruthRects,
								int TruthRectCount,
							 	global float2* TruthCorners,
								int TruthCornerCount,

								float MaxMatchDistance
								)
{
	int MatchIndex = get_global_id(0) + MatchRectIndexFirst;
	int TruthIndex = get_global_id(1) + TruthRectIndexFirst;
	int ResultIndex = (TruthIndex*MatchRectCount) + MatchIndex;

	float16 TruthRect0 = TruthRects[TruthIndex];
	
	//	our rects might be in the wrong order, need to cycle coords (and reverse?)
	float16 MatchRect0 = MatchRects[MatchIndex];
	float16 ResultHomography;
	float ResultScore = FindHomography( MatchRect0, TruthRect0, MatchCorners, MatchCornerCount, TruthCorners, TruthCornerCount, MaxMatchDistance, &ResultHomography );
	
	ResultHomographys[ResultIndex] = ResultHomography;
	ResultScores[ResultIndex] = ResultScore;
}
