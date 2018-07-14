

kernel void GetTestHomography(	volatile global float16* ResultHomographys
							  )
{
	ResultHomographys[0] = (float16)( 1,0,0,0,	0,1,0,0,	0,0,1,0,	0,0,0,1	);
}
/*

static float16 GetHomographyMatrix(float2* Match4,float2* Truth4)
{
	
}

static float Range01(float Min,float Max,float Value)
{
	float Range = (Value-Min) / (Max-Min);
	Range = max( 0, Range );
	Range = min( 1, Range );
	return Range;
}

static void FindHomography(int4 MatchIndex4,int4 TruthIndex4,float2* Matchs,float2* Truths,int TruthCount,float MaxMatchDistance,float16* ResultHomography,float* ResultScore)
{
	//	get the matrix
	float2 Matchs4[4];
	Matchs4[0] = Matchs[MatchIndex4.x];
	Matchs4[1] = Matchs[MatchIndex4.y];
	Matchs4[2] = Matchs[MatchIndex4.z];
	Matchs4[3] = Matchs[MatchIndex4.w];
	float2 Truths4[4];
	Truths4[0] = Truths[TruthIndex4.x];
	Truths4[1] = Truths[TruthIndex4.y];
	Truths4[2] = Truths[TruthIndex4.z];
	Truths4[3] = Truths[TruthIndex4.w];

	float16 HomographyMatrix = GetHomographyMatrix( Matchs4, Truths4 );
	
	//	score the other truths by finding a match nearby
	float Score = 0;
	for ( int t=0;	t<TruthCount;	t++ )
	{
		if ( t == TruthIndex4.x )	continue;
		if ( t == TruthIndex4.y )	continue;
		if ( t == TruthIndex4.z )	continue;
		if ( t == TruthIndex4.w )	continue;
		
		float2 Truth2 = Truths[t];
		float4 TruthInverse4 = HomographyMatrix * float4( Truth2.x, Truth2.y, 0, 1 );
		float2 TruthInverse2 = TruthInverse4.xy;
		
		float TClosest = 999;
		
		for ( int m=0;	m<MatchCount;	m++ )
		{
			float2 Match2 = Matchs[m];
			float Distance = length( Match2, TruthInverse2 );
			if ( Distance > MaxMatchDistance )
				continue;
			TClosest = min( TClosest, Distance );
		}
		
		float TScore = 1 - Range01( 0, MaxMatchDistance, TClosest );
		Score += TScore;
	}
	Score /= (TruthCount-4);
	
	*ResultScore = Score;
	*ResultHomography = HomographyMatrix;
}



kernel void FindHomographies(	int MatchSetIndexFirst,
								global float2* MatchPositions,
								global int4* MatchSets,
								int MatchSetCount,
							 
								int TruthSetIndexFirst,
								global float2* TruthPositions,
								global int4* TruthSets,
								int TruthSetCount,
								int TruthPositionCount,
							 
							  	volatile global float16* ResultHomographys,
								volatile global float* ResultScores,
								int ResultCount,
							 
								float MaxMatchDistance
								)
{
	int MatchSetIndex = get_global_id(0) + MatchSetIndexFirst;
	int TruthSetIndex = get_global_id(1) + TruthSetIndexFirst;
	int ResultIndex = (MatchSetIndex * MatchSetCount) + TruthSetIndex;
	
	int4 MatchSet = MatchSets[MatchSetIndex];
	int4 TruthSet = TruthSets[TruthSetIndex];

	
	float16 ResultHomography;
	float ResultScore;
	
	FindHomography( MatchSet, TruthSet, MatchPositions, TruthPositions, TruthPositionCount, MaxMatchDistance, &ResultHomography, &ResultScore );
	
	ResultHomographys[ResultIndex] = ResultHomography;
	ResultScores[ResultIndex] = ResultScore;

}
 */
