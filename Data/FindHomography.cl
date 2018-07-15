#define GENERATE_TRUTH_TO_MATCH_MATRIX

kernel void GetTestHomography(volatile global float16* ResultHomographys)
{
	ResultHomographys[0] = (float16)( 1,0,0,0,	0,1,0,0,	0,0,1,0,	0,0,0,1	);
}


static void GaussianElimination(float A[8][9], int n)
{
	
	// originally by arturo castro - 08/01/2010
	//
	// ported to c from pseudocode in
	// http://en.wikipedia.org/wiki/Gaussian_elimination
	
	int i = 0;
	int j = 0;
	int m = n - 1;
	
	while (i < m && j < n)
	{
		// Find pivot in column j, starting in row i:
		int maxi = i;
		
		for (int k = i + 1; k < m; k++)
		{
			float a = fabs( A[k][j] );
			float b = fabs( A[maxi][j] );
			if ( a > b )
			{
				maxi = k;
			}
		}
		
		if (A[maxi][j] != 0)
		{
			//swap rows i and maxi, but do not change the value of i
			if (i != maxi)
				for (int k = 0; k < n; k++)
				{
					float aux = A[i][k];
					A[i][k] = A[maxi][k];
					A[maxi][k] = aux;
				}
			//Now A[i,j] will contain the old value of A[maxi,j].
			//divide each entry in row i by A[i,j]
			float A_ij = A[i][j];
			for (int k = 0; k < n; k++)
			{
				A[i][k] /= A_ij;
			}
			//Now A[i,j] will have the value 1.
			for (int u = i + 1; u < m; u++)
			{
				//subtract A[u,j] * row i from row u
				float A_uj = A[u][j];
				for (int k = 0; k < n; k++)
				{
					A[u][k] -= A_uj * A[i][k];
				}
				//Now A[u,j] will be 0, since A[u,j] - A[i,j] * A[u,j] = A[u,j] - 1 * A[u,j] = 0.
			}
			i++;
		}
		j++;
	}
	
	//back substitution
	for (int k = m - 2; k >= 0; k--)
	{
		for (int l = k + 1; l < n - 1; l++)
		{
			A[k][m] -= A[k][l] * A[l][m];
			//A[i*n+j]=0;
		}
	}
	
}


static float16 CalcHomography(float3* src,float3* dest)
{
	// originally by arturo castro - 08/01/2010
	//
	// create the equation system to be solved
	//
	// from: Multiple View Geometry in Computer Vision 2ed
	//       Hartley R. and Zisserman A.
	//
	// x' = xH
	// where H is the homography: a 3 by 3 matrix
	// that transformed to inhomogeneous coordinates for each point
	// gives the following equations for each point:
	//
	// x' * (h31*x + h32*y + h33) = h11*x + h12*y + h13
	// y' * (h31*x + h32*y + h33) = h21*x + h22*y + h23
	//
	// as the homography is scale independent we can let h33 be 1 (indeed any of the terms)
	// so for 4 points we have 8 equations for 8 terms to solve: h11 - h32
	// after ordering the terms it gives the following matrix
	// that can be solved with gaussian elimination:
	
	 float P[8][9] = {
		{-src[0].x, -src[0].y, -1,   0,   0,  0, src[0].x*dest[0].x, src[0].y*dest[0].x, -dest[0].x }, // h11
		{  0,   0,  0, -src[0].x, -src[0].y, -1, src[0].x*dest[0].y, src[0].y*dest[0].y, -dest[0].y }, // h12
		
		{-src[1].x, -src[1].y, -1,   0,   0,  0, src[1].x*dest[1].x, src[1].y*dest[1].x, -dest[1].x }, // h13
		{  0,   0,  0, -src[1].x, -src[1].y, -1, src[1].x*dest[1].y, src[1].y*dest[1].y, -dest[1].y }, // h21
		
		{-src[2].x, -src[2].y, -1,   0,   0,  0, src[2].x*dest[2].x, src[2].y*dest[2].x, -dest[2].x }, // h22
		{  0,   0,  0, -src[2].x, -src[2].y, -1, src[2].x*dest[2].y, src[2].y*dest[2].y, -dest[2].y }, // h23
		
		{-src[3].x, -src[3].y, -1,   0,   0,  0, src[3].x*dest[3].x, src[3].y*dest[3].x, -dest[3].x }, // h31
		{  0,   0,  0, -src[3].x, -src[3].y, -1, src[3].x*dest[3].y, src[3].y*dest[3].y, -dest[3].y }, // h32
	 };
	
	
	GaussianElimination( P, 9 );
	
	
	//	gr: to let us invert, need determinet to be non zero
	float m22 = 1;
	
	//	z = identity
	float4 Row0 = (float4)(P[0][8], P[3][8],	0, 	P[6][8]);
	float4 Row1 = (float4)(P[1][8], P[4][8],	0, 	P[7][8]);
	float4 Row2 = (float4)(0, 		0, 			m22, 0);
	float4 Row3 = (float4)(P[2][8], P[5][8],	0, 	1);

	float16 HomographyMtx = (float16)(Row0,Row1,Row2,Row3);
	
	return HomographyMtx;
}



static float16 GetHomographyMatrix(float16 Match4x2,float16 Truth4x2)
{
	float3 m[4];
	m[0] = (float3)( Match4x2[0], Match4x2[1], 0 );
	m[1] = (float3)( Match4x2[2], Match4x2[3], 0 );
	m[2] = (float3)( Match4x2[4], Match4x2[5], 0 );
	m[3] = (float3)( Match4x2[6], Match4x2[7], 0 );
	float3 t[4];
	t[0] = (float3)( Truth4x2[0], Truth4x2[1], 0 );
	t[1] = (float3)( Truth4x2[2], Truth4x2[3], 0 );
	t[2] = (float3)( Truth4x2[4], Truth4x2[5], 0 );
	t[3] = (float3)( Truth4x2[6], Truth4x2[7], 0 );

#if defined(GENERATE_TRUTH_TO_MATCH_MATRIX)
	return CalcHomography( m, t );
#else
	return CalcHomography( t, m );
#endif
	//return (float16)( 1,0,0,0,	0,1,0,0,	0,0,1,0,	0,0,0,1	);
}

static float Range01(float Min,float Max,float Value)
{
	float Range = (Value-Min) / (Max-Min);
	Range = max( 0.0f, Range );
	Range = min( 1.0f, Range );
	return Range;
}

static float MatrixMultiplyElement(float16 Matrix,float4 Vector,int Element,int Width)
{
	float Value = 0;
	for ( int k=0;	k<Width;	k++ )
	{
		Value += Matrix[Element * Width + k] * Vector[k];
	}
	return Value;
}

static float4 MatrixMultiply(float16 Matrix,float4 Vector)
{
	float4 xyzw;
	xyzw.x = MatrixMultiplyElement( Matrix, Vector, 0, 4 );
	xyzw.y = MatrixMultiplyElement( Matrix, Vector, 1, 4 );
	xyzw.z = MatrixMultiplyElement( Matrix, Vector, 2, 4 );
	xyzw.w = MatrixMultiplyElement( Matrix, Vector, 3, 4 );
	return xyzw;
}

static float FindHomography(float16 MatchRect,float16 TruthRect,global float2* MatchCorners,int MatchCornerCount,global float2* TruthCorners,int TruthCornerCount,float MaxMatchDistance,float16* ResultHomography)
{
	float16 HomographyMatrix = GetHomographyMatrix( MatchRect, TruthRect );
	*ResultHomography = HomographyMatrix;
	
	//	score the other truths by finding a match nearby
	float Score = 0;
	
	for ( int t=0;	t<TruthCornerCount;	t++ )
	{
		//	need to exclude sources here really
		float2 Truth2 = TruthCorners[t];
		float4 TruthInverse4 = MatrixMultiply( HomographyMatrix, (float4)( Truth2.x, Truth2.y, 0, 1 ) );
		float2 TruthInverse2 = TruthInverse4.xy / TruthInverse4.w;
		
		float TClosest = 999;
		
		for ( int m=0;	m<MatchCornerCount;	m++ )
		{
			float2 Match2 = MatchCorners[m];
			float4 MatchInverse4 = MatrixMultiply( HomographyMatrix, (float4)( Match2.x, Match2.y, 0, 1 ) );
			float2 MatchInverse2 = MatchInverse4.xy / MatchInverse4.w;
			
#if defined(GENERATE_TRUTH_TO_MATCH_MATRIX)
			float Distance = length( MatchInverse2 - Truth2 );
#else
			float Distance = length( Match2 - TruthInverse2 );
#endif
			TClosest = min( TClosest, Distance );
		}
		
		
		//if ( TClosest <= MaxMatchDistance )
		//	Score ++;
		
		float TScore = 1.0f - min( 1.0f, TClosest/MaxMatchDistance );
		//	square to favour better, but still count matches
		Score += TScore * TScore;
		
	}
	//Score /= (TruthCornerCount);
	
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
	float16 MatchRectOrig = MatchRects[MatchIndex];
#define ORDER_COUNT	8
	int4 Order[ORDER_COUNT] =
	{
		(int4)(0,1,2,3),
		(int4)(1,2,3,0),
		(int4)(2,3,0,1),
		(int4)(3,0,1,2),

		(int4)(0,3,2,1),
		(int4)(3,2,1,0),
		(int4)(2,1,0,3),
		(int4)(1,0,3,2),
	};
	
	float BestScore = 0;
	ResultScores[ResultIndex] = -1;
	for ( int o=0;	o<ORDER_COUNT;	o++ )
	{
		float16 MatchRect0;
		for ( int i=0;	i<4;	i++ )
		{
			int4 j4 = Order[o];
			int j = j4[i];
			MatchRect0[(i*2)+0] = MatchRectOrig[(j*2)+0];
			MatchRect0[(i*2)+1] = MatchRectOrig[(j*2)+1];
		}
	
		float16 ResultHomography;
		float ResultScore = FindHomography( MatchRect0, TruthRect0, MatchCorners, MatchCornerCount, TruthCorners, TruthCornerCount, MaxMatchDistance, &ResultHomography );
	
		if ( ResultScore > BestScore )
		{
			ResultHomographys[ResultIndex] = ResultHomography;
			ResultScores[ResultIndex] = ResultScore;
			BestScore = ResultScore;
		}
	}
}
