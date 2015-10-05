#include "Common.cl"
#include "Array.cl"

DECLARE_DYNAMIC_ARRAY(float4);


__kernel void ExtractHoughCorners(int OffsetHoughLineAIndex,
								  int OffsetHoughLineBIndex,
								  global float8* HoughLines,
								  int HoughLineCount,
								  global float4* HoughCorners,
								  float MinScore
								  )
{
	int HoughLineAIndex = get_global_id(0) + OffsetHoughLineAIndex;
	int HoughLineBIndex = get_global_id(1) + OffsetHoughLineBIndex;
	float8 HoughLineA = HoughLines[HoughLineAIndex];
	float8 HoughLineB = HoughLines[HoughLineBIndex];
	int CornerIndex = (HoughLineAIndex*HoughLineCount) + HoughLineBIndex;
	
	float2 Intersection = 0;
	float Score = 0;
	float w = 0;

	//	same-index will always intersect, (or be parallel?) score zero
	//	and we only need to compare lines once. so B must be >A
	if ( HoughLineAIndex < HoughLineBIndex )
	{
		float3 Intersection3 = GetLineLineInfiniteIntersection( HoughLineA.xyzw, HoughLineB.xyzw );
		Intersection = Intersection3.xy;
	
		//	just for neat output
		Intersection.x = round(Intersection.x);
		Intersection.y = round(Intersection.y);

		//	invalidate score if intersection was bad
		Score = HoughLineA[6] * HoughLineB[6];
		Score *= Intersection3.z;
		
		//	invalidate score if cross is very far away
		float FarCoord = 10000;
		if ( fabsf(Intersection.x) > FarCoord || fabsf(Intersection.y) > FarCoord )
			Score = -1;

		if ( Score < MinScore )
			Score = 0;
	}
	
	HoughCorners[CornerIndex] = (float4)( Intersection, Score, w );
}




__kernel void DrawHoughCorners(int OffsetIndex,__write_only image2d_t Frag,global float4* HoughCorners,float Zoom)
{
	int LineIndex = get_global_id(0) + OffsetIndex;
	float4 HoughCorner = HoughCorners[LineIndex];
	int2 wh = get_image_dim(Frag);
	float2 Corner = HoughCorner.xy;
	
	//	zoom coord from center
	float2 whf = (float2)(wh.x,wh.y);
	Corner -= whf/2.f;
	Corner *= Zoom;
	Corner += whf/2.f;
	
	float Score = HoughCorner.z;
	
	float4 Rgba = 1;
	Rgba.xyz = NormalToRgb( Score );
	
	
	int Radius = 10;
	for ( int y=-Radius;	y<=Radius;	y++ )
	{
		for ( int x=-Radius;	x<=Radius;	x++ )
		{
			int2 xy = (int2)(Corner.x+x,Corner.y+y);
			xy.x = clamp( xy.x, 0, wh.x-1 );
			xy.y = clamp( xy.y, 0, wh.y-1 );
			write_imagef( Frag, xy, Rgba );
		}
	}
}






// Thos SVD code requires rows >= columns.
#define M 9 // rows
#define N 9 // cols

static double SIGN(double a, double b)
{
	if(b > 0) {
		return fabs(a);
	}
	
	return -fabs(a);
}

static double PYTHAG(double a, double b)
{
	double at = fabs(a), bt = fabs(b), ct, result;
	
	if (at > bt)       { ct = bt / at; result = at * sqrt(1.0 + ct * ct); }
	else if (bt > 0.0) { ct = at / bt; result = bt * sqrt(1.0 + ct * ct); }
	else result = 0.0;
	return(result);
}

// Returns 1 on success, fail otherwise
static int dsvd(float *a, int m, int n, float *w, float *v)
{
	//	float w[N];
	//	float v[N*N];

	int flag, i, its, j, jj, k, l, nm;
	double c, f, h, s, x, y, z;
	double anorm = 0.0, g = 0.0, scale = 0.0;
	double rv1[N];
	
	if (m < n)
	{
		//fprintf(stderr, "#rows must be > #cols \n");
		return(-1);
	}
	
	
	//	Householder reduction to bidiagonal form
	for (i = 0; i < n; i++)
	{
		//left-hand reduction
		l = i + 1;
		rv1[i] = scale * g;
		g = s = scale = 0.0;
		if (i < m)
		{
			for (k = i; k < m; k++)
				scale += fabsf(a[k*n+i]);
			
			if (scale )
			{
				for (k = i; k < m; k++)
				{
					a[k*n+i] /= scale;
					s += a[k*n+i] * a[k*n+i];
				}
				
				f = (double)a[i*n+i];
				g = -SIGN(sqrt(s), f);
				h = f * g - s;
				a[i*n+i] = (float)(f - g);
				if (i != n - 1)
				{
					for (j = l; j < n; j++)
					{
						for (s = 0.0, k = i; k < m; k++)
							s += ((double)a[k*n+i] * (double)a[k*n+j]);
						f = s / h;
						for (k = i; k < m; k++)
							a[k*n+j] += (float)(f * (double)a[k*n+i]);
					}
				}
				for (k = i; k < m; k++)
					a[k*n+i] = (float)((double)a[k*n+i]*scale);
				
			}
			
		}
		w[i] = (float)(scale * g);
	
		/// right-hand reduction
		g = s = scale = 0.0;
		if (i < m && i != n - 1)
		{
			for (k = l; k < n; k++)
				scale += fabs((double)a[i*n+k]);
			if (scale)
			{
				for (k = l; k < n; k++)
				{
					a[i*n+k] = (float)((double)a[i*n+k]/scale);
					s += ((double)a[i*n+k] * (double)a[i*n+k]);
				}
				f = (double)a[i*n+l];
				g = -SIGN(sqrt(s), f);
				h = f * g - s;
				a[i*n+l] = (float)(f - g);
				for (k = l; k < n; k++)
					rv1[k] = (double)a[i*n+k] / h;
				if (i != m - 1)
				{
					for (j = l; j < m; j++)
					{
						for (s = 0.0, k = l; k < n; k++)
							s += ((double)a[j*n+k] * (double)a[i*n+k]);
						for (k = l; k < n; k++)
							a[j*n+k] += (float)(s * rv1[k]);
					}
				}
				for (k = l; k < n; k++)
					a[i*n+k] = (float)((double)a[i*n+k]*scale);
			}
		}
		anorm = max(anorm, (fabs((double)w[i]) + fabs(rv1[i])));
	 
	}

	// accumulate the right-hand transformation
	for (i = n - 1; i >= 0; i--)
	{
		if (i < n - 1)
		{
			if (g)
			{
				for (j = l; j < n; j++)
					v[j*n+i] = (float)(((double)a[i*n+j] / (double)a[i*n+l]) / g);
				// double division to avoid underflow
				for (j = l; j < n; j++)
				{
					for (s = 0.0, k = l; k < n; k++)
						s += ((double)a[i*n+k] * (double)v[k*n+j]);
					for (k = l; k < n; k++)
						v[k*n+j] += (float)(s * (double)v[k*n+i]);
				}
			}
			for (j = l; j < n; j++)
				v[i*n+j] = v[j*n+i] = 0.0;
		}
		v[i*n+i] = 1.0;
		g = rv1[i];
		l = i;
	}
	
	//accumulate the left-hand transformation
	for (i = n - 1; i >= 0; i--)
	{
		l = i + 1;
		g = (double)w[i];
		if (i < n - 1)
			for (j = l; j < n; j++)
				a[i*n+j] = 0.0;
		if (g)
		{
			g = 1.0 / g;
			if (i != n - 1)
			{
				for (j = l; j < n; j++)
				{
					for (s = 0.0, k = l; k < m; k++)
						s += ((double)a[k*n+i] * (double)a[k*n+j]);
					f = (s / (double)a[i*n+i]) * g;
					for (k = i; k < m; k++)
						a[k*n+j] += (float)(f * (double)a[k*n+i]);
				}
			}
			for (j = i; j < m; j++)
				a[j*n+i] = (float)((double)a[j*n+i]*g);
		}
		else
		{
			for (j = i; j < m; j++)
				a[j*n+i] = 0.0;
		}
		++a[i*n+i];
	}
	
	// diagonalize the bidiagonal form
	for (k = n - 1; k >= 0; k--)
	{                           // loop over singular values
		for (its = 0; its < 30; its++)
		{                       // loop over allowed iterations
			flag = 1;
			for (l = k; l >= 0; l--)
			{                     // test for splitting
				nm = l - 1;
				if (fabs(rv1[l]) + anorm == anorm)
				{
					flag = 0;
					break;
				}
				if (fabs((double)w[nm]) + anorm == anorm)
					break;
			}
			if (flag)
			{
				c = 0.0;
				s = 1.0;
				for (i = l; i <= k; i++)
				{
					f = s * rv1[i];
					if (fabs(f) + anorm != anorm)
					{
						g = (double)w[i];
						h = PYTHAG(f, g);
						w[i] = (float)h;
						h = 1.0 / h;
						c = g * h;
						s = (- f * h);
						for (j = 0; j < m; j++)
						{
							y = (double)a[j*n+nm];
							z = (double)a[j*n+i];
							a[j*n+nm] = (float)(y * c + z * s);
							a[j*n+i] = (float)(z * c - y * s);
						}
					}
				}
			}
			z = (double)w[k];
			if (l == k)
			{                  //convergence
				if (z < 0.0)
				{              // make singular value nonnegative
					w[k] = (float)(-z);
					for (j = 0; j < n; j++)
						v[j*n+k] = (-v[j*n+k]);
				}
				break;
			}
			if (its >= 30) {
				//free((void*) rv1);
				//fprintf(stderr, "No convergence after 30,000! iterations \n");
				return(0);
			}
			
			///shift from bottom 2 x 2 minor
			x = (double)w[l];
			nm = k - 1;
			y = (double)w[nm];
			g = rv1[nm];
			h = rv1[k];
			f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
			g = PYTHAG(f, 1.0);
			f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g, f))) - h)) / x;
			
			// next QR transformation
			c = s = 1.0;
			for (j = l; j <= nm; j++)
			{
				i = j + 1;
				g = rv1[i];
				y = (double)w[i];
				h = s * g;
				g = c * g;
				z = PYTHAG(f, h);
				rv1[j] = z;
				c = f / z;
				s = h / z;
				f = x * c + g * s;
				g = g * c - x * s;
				h = y * s;
				y = y * c;
				for (jj = 0; jj < n; jj++)
				{
					x = (double)v[jj*n+j];
					z = (double)v[jj*n+i];
					v[jj*n+j] = (float)(x * c + z * s);
					v[jj*n+i] = (float)(z * c - x * s);
				}
				z = PYTHAG(f, h);
				w[j] = (float)z;
				if (z)
				{
					z = 1.0 / z;
					c = f * z;
					s = h * z;
				}
				f = (c * g) + (s * y);
				x = (c * y) - (s * g);
				for (jj = 0; jj < m; jj++)
				{
					y = (double)a[jj*n+j];
					z = (double)a[jj*n+i];
					a[jj*n+j] = (float)(y * c + z * s);
					a[jj*n+i] = (float)(z * c - y * s);
				}
			}
			rv1[l] = 0.0;
			rv1[k] = f;
			w[k] = (float)x;
		}
	}
 
	
	return(1);
}



static float16 CalcHomography(float2 src[4],float2 dst[4])
{
	// This version does not normalised the input data, which is contrary to what Multiple View Geometry says.
	// I included it to see what happens when you don't do this step.
	
	float X[M*N]; // M,N #define inCUDA_SVD.cu
	
	for(int i=0; i < 4; i++)
	{
		float srcx = src[i].x;
		float srcy = src[i].y;
		float dstx = dst[i].x;
		float dsty = dst[i].y;
		
		int y1 = (i*2 + 0)*N;
		int y2 = (i*2 + 1)*N;
		
		// First row
		X[y1+0] = 0.f;
		X[y1+1] = 0.f;
		X[y1+2] = 0.f;
		
		X[y1+3] = -srcx;
		X[y1+4] = -srcy;
		X[y1+5] = -1.f;
		
		X[y1+6] = dsty*srcx;
		X[y1+7] = dsty*srcy;
		X[y1+8] = dsty;
		
		// Second row
		X[y2+0] = srcx;
		X[y2+1] = srcy;
		X[y2+2] = 1.f;
		
		X[y2+3] = 0.f;
		X[y2+4] = 0.f;
		X[y2+5] = 0.f;
		
		X[y2+6] = -dstx*srcx;
		X[y2+7] = -dstx*srcy;
		X[y2+8] = -dstx;
	}
	
	// Fill the last row
	float srcx = src[3].x;
	float srcy = src[3].y;
	float dstx = dst[3].x;
	float dsty = dst[3].y;
	
	int y = 8*N;
	X[y+0] = -dsty*srcx;
	X[y+1] = -dsty*srcy;
	X[y+2] = -dsty;
	
	X[y+3] = dstx*srcx;
	X[y+4] = dstx*srcy;
	X[y+5] = dstx;
	
	X[y+6] = 0;
	X[y+7] = 0;
	X[y+8] = 0;
	
	float w[N];
	float v[N*N];
	
	float16 ret_H = 0;
	int ret = dsvd(X, M, N, w, v);
	
	if(ret == 1)
	{
		// Sort
		float smallest = w[0];
		int col = 0;
		
		for(int i=1; i < N; i++) {
			if(w[i] < smallest) {
				smallest = w[i];
				col = i;
			}
		}
		
		ret_H[0] = v[0*N + col];
		ret_H[1] = v[1*N + col];
		ret_H[2] = v[2*N + col];
		ret_H[3] = v[3*N + col];
		ret_H[4] = v[4*N + col];
		ret_H[5] = v[5*N + col];
		ret_H[6] = v[6*N + col];
		ret_H[7] = v[7*N + col];
		ret_H[8] = v[8*N + col];
	}
	
	return ret_H;
}

#define Index3x3(r,c)	((r*3)+c)

static float16 GetMatrix3x3Inverse(float16 m)
{
	float det =
	m[Index3x3(0,0)]*
	m[Index3x3(1,1)]*
	m[Index3x3(2,2)]+
	m[Index3x3(1,0)]*
	m[Index3x3(2,1)]*
	m[Index3x3(0,2)]+
	m[Index3x3(2,0)]*
	m[Index3x3(0,1)]*
	m[Index3x3(1,2)]-
	m[Index3x3(0,0)]*
	m[Index3x3(2,1)]*
	m[Index3x3(1,2)]-
	m[Index3x3(2,0)]*
	m[Index3x3(1,1)]*
	m[Index3x3(0,2)]-
	m[Index3x3(1,0)]*
	m[Index3x3(0,1)]*
	m[Index3x3(2,2)];
	
	float16 inv = 0;
	inv[Index3x3(0,0)] = m[Index3x3(1,1)]*m[Index3x3(2,2)] - m[Index3x3(1,2)]*m[Index3x3(2,1)];
	inv[Index3x3(0,1)] = m[Index3x3(0,2)]*m[Index3x3(2,1)] - m[Index3x3(0,1)]*m[Index3x3(2,2)];
	inv[Index3x3(0,2)] = m[Index3x3(0,1)]*m[Index3x3(1,2)] - m[Index3x3(0,2)]*m[Index3x3(1,1)];
//	inv[Index3x3(0,3].w = 0.f;
	
	inv[Index3x3(1,0)] = m[Index3x3(1,2)]*m[Index3x3(2,0)] - m[Index3x3(1,0)]*m[Index3x3(2,2)];
	inv[Index3x3(1,1)] = m[Index3x3(0,0)]*m[Index3x3(2,2)] - m[Index3x3(0,2)]*m[Index3x3(2,0)];
	inv[Index3x3(1,2)] = m[Index3x3(0,2)]*m[Index3x3(1,0)] - m[Index3x3(0,0)]*m[Index3x3(1,2)];
//	inv[Index3x3(1,3)].w = 0.f;
	
	inv[Index3x3(2,0)] = m[Index3x3(1,0)]*m[Index3x3(2,1)] - m[Index3x3(1,1)]*m[Index3x3(2,0)];
	inv[Index3x3(2,1)] = m[Index3x3(0,1)]*m[Index3x3(2,0)] - m[Index3x3(0,0)]*m[Index3x3(2,1)];
	inv[Index3x3(2,2)] = m[Index3x3(0,0)]*m[Index3x3(1,1)] - m[Index3x3(0,1)]*m[Index3x3(1,0)];
//	inv[Index3x3(2,3)].w = 0.f;
	
	inv[0] *= 1.0f / det;
	inv[1] *= 1.0f / det;
	inv[2] *= 1.0f / det;
	inv[3] *= 1.0f / det;
	inv[4] *= 1.0f / det;
	inv[5] *= 1.0f / det;
	inv[6] *= 1.0f / det;
	inv[7] *= 1.0f / det;
	inv[8] *= 1.0f / det;
	return inv;
}

__kernel void HoughCornerHomography(int MatchIndexOffset,
									int TruthIndexOffset,
									global int4* MatchIndexes,
									global int4* TruthIndexes,
									int MatchIndexesCount,
									global float4* MatchCorners,
									global float2* TruthCorners,
									global float16* Homographys,
									global float16* HomographysInv
									)
{
	int MatchIndex = get_global_id(0) + MatchIndexOffset;
	int TruthIndex = get_global_id(1) + TruthIndexOffset;

	float2 MatchSampleCorners[4] =
	{
		MatchCorners[MatchIndexes[MatchIndex][0]].xy,
		MatchCorners[MatchIndexes[MatchIndex][1]].xy,
		MatchCorners[MatchIndexes[MatchIndex][2]].xy,
		MatchCorners[MatchIndexes[MatchIndex][3]].xy,
	};
	float2 TruthSampleCorners[4] =
	{
		TruthCorners[TruthIndexes[TruthIndex][0]].xy,
		TruthCorners[TruthIndexes[TruthIndex][1]].xy,
		TruthCorners[TruthIndexes[TruthIndex][2]].xy,
		TruthCorners[TruthIndexes[TruthIndex][3]].xy,
	};
	
	float16 Homography = CalcHomography( MatchSampleCorners, TruthSampleCorners );
	Homographys[(TruthIndex*MatchIndexesCount)+MatchIndex] = Homography;
	float16 HomographyInv = GetMatrix3x3Inverse( Homography );
	HomographysInv[(TruthIndex*MatchIndexesCount)+MatchIndex] = HomographyInv;
}



__kernel void HoughLineHomography(int TruthPairIndexOffset,
									int HoughPairIndexOffset,
									global int4* TruthPairIndexes,
									global int4* HoughPairIndexes,
								  int TruthPairIndexCount,
								  int HoughPairIndexCount,
									global float8* HoughLines,
									global float8* TruthLines,
									global float16* Homographys,
									global float16* HomographysInv
									)
{
	int TruthPairIndex = get_global_id(0) + TruthPairIndexOffset;
	int HoughPairIndex = get_global_id(1) + HoughPairIndexOffset;

	//	indexes are vvhh
	int4 TruthIndexes = TruthPairIndexes[TruthPairIndex];
	int4 HoughIndexes = HoughPairIndexes[HoughPairIndex];
	
	//	grab the lines and find their intersections to get our four corners
	float8 SampleTruthLines[4] =
	{
		TruthLines[TruthIndexes[0]],
		TruthLines[TruthIndexes[1]],
		TruthLines[TruthIndexes[2]],
		TruthLines[TruthIndexes[3]],
	};
	float8 SampleHoughLines[4] =
	{
		HoughLines[HoughIndexes[0]],
		HoughLines[HoughIndexes[1]],
		HoughLines[HoughIndexes[2]],
		HoughLines[HoughIndexes[3]],
	};
	//	find v/h intersections
	float2 TruthCorners[4] =
	{
		GetRayRayIntersection( SampleTruthLines[0].xyzw, SampleTruthLines[2].xyzw ).xy,
		GetRayRayIntersection( SampleTruthLines[0].xyzw, SampleTruthLines[3].xyzw ).xy,
		GetRayRayIntersection( SampleTruthLines[1].xyzw, SampleTruthLines[2].xyzw ).xy,
		GetRayRayIntersection( SampleTruthLines[1].xyzw, SampleTruthLines[3].xyzw ).xy,
	};
	float2 HoughCorners[4] =
	{
		GetRayRayIntersection( SampleHoughLines[0].xyzw, SampleHoughLines[2].xyzw ).xy,
		GetRayRayIntersection( SampleHoughLines[0].xyzw, SampleHoughLines[3].xyzw ).xy,
		GetRayRayIntersection( SampleHoughLines[1].xyzw, SampleHoughLines[2].xyzw ).xy,
		GetRayRayIntersection( SampleHoughLines[1].xyzw, SampleHoughLines[3].xyzw ).xy,
	};
	
	float16 Homography = CalcHomography( HoughCorners, TruthCorners );
	float16 HomographyInv = GetMatrix3x3Inverse( Homography );
	Homographys[(TruthPairIndex*HoughPairIndexCount)+HoughPairIndex] = Homography;
	HomographysInv[(TruthPairIndex*HoughPairIndexCount)+HoughPairIndex] = HomographyInv;
}





static float2 Transform2ByMatrix3x3(float2 Position,float16 Homography3x3)
{
	float16 H = Homography3x3;
	float2 src = Position;
	float x = H[0]*src.x + H[1]*src.y + H[2];
	float y = H[3]*src.x + H[4]*src.y + H[5];
	float z = H[6]*src.x + H[7]*src.y + H[8];
	
	x /= z;
	y /= z;
	
	return (float2)(x,y);
}


__kernel void DrawHomographyCorners(int CornerIndexOffset,
									int HomographyIndexOffset,
									__write_only image2d_t Frag,
									global float4* HoughCorners,
									global float2* TruthCorners,
									int TruthCornerCount,
									float Zoom,
									global float16* Homographys
									)
{
	int CornerIndex = get_global_id(0) + CornerIndexOffset;
	int HomographyIndex = get_global_id(1) + HomographyIndexOffset;

	float4 HoughCorner = HoughCorners[CornerIndex];
	float2 Corner = HoughCorner.xy;
	float16 Homography = Homographys[HomographyIndex];
	
	//	transform corner
	float2 TransformedCorner = Transform2ByMatrix3x3( Corner, Homography );
	
	//	find nearest truth corner
	float MaxDistance = 50;
	float BestDistance = MaxDistance;
	for ( int t=0;	t<TruthCornerCount;	t++ )
	{
		float2 TruthCorner = TruthCorners[t];
		float Dist = distance( TruthCorner, TransformedCorner );
		if ( Dist <= 0 || Dist >= MaxDistance )
			continue;
		BestDistance = min( BestDistance, Dist );
	}

	
	float Score = BestDistance / MaxDistance;
	Score = 1.f - clamp( Score, 0.f, 1.f );
	//float Score = Homography[15];
	Corner = TransformedCorner;
	
	
	int Radius = 10;
	float4 Rgba = 1;
	Rgba.xyz = NormalToRgb( Score );

	
	if ( HomographyIndex == 0 )
	{
		Radius = 5;
		Corner = TruthCorners[ CornerIndex % TruthCornerCount];
		Rgba = 1;
	}
	else
	if ( Score < 0.01f )
	{
		return;
	}
	
	
	//	zoom coord from center
	int2 wh = get_image_dim(Frag);
	float2 whf = (float2)(wh.x,wh.y);
	Corner -= whf/2.f;
	Corner *= Zoom;
	Corner += whf/2.f;
	
	
	

	
	for ( int y=-Radius;	y<=Radius;	y++ )
	{
		for ( int x=-Radius;	x<=Radius;	x++ )
		{
			int2 xy = (int2)(Corner.x+x,Corner.y+y);
			xy.x = clamp( xy.x, 0, wh.x-1 );
			xy.y = clamp( xy.y, 0, wh.y-1 );
			write_imagef( Frag, xy, Rgba );
		}
	}
	
}




__kernel void ScoreCornerHomographys(int HomographyIndexOffset,
									global float4* HoughCorners,
									 int HoughCornerCount,
									global float2* TruthCorners,
									int TruthCornerCount,
									 global float16* Homographys
									)
{
	int HomographyIndex = get_global_id(0) + HomographyIndexOffset;
	float16 Homography = Homographys[HomographyIndex];

	float MaxDistance = 20;
	float TotalScore = 0;
	for ( int CornerIndex=0;	CornerIndex<HoughCornerCount;	CornerIndex++ )
	{
		float4 HoughCorner = HoughCorners[CornerIndex];
		float2 Corner = HoughCorner.xy;
	
		//	transform corner
		float2 TransformedCorner = Transform2ByMatrix3x3( Corner, Homography );
		
		//	find nearest truth corner
		float BestDistance = MaxDistance;
		for ( int t=0;	t<TruthCornerCount;	t++ )
		{
			float2 TruthCorner = TruthCorners[t];
			float Dist = distance( TruthCorner, TransformedCorner );
			if ( Dist <= 0 || Dist >= MaxDistance )
				continue;
			BestDistance = min( BestDistance, Dist );
		}
		
		float Score = BestDistance / MaxDistance;
		Score = 1.f - clamp( Score, 0.f, 1.f );
		TotalScore += Score;
	}
	
	if ( HoughCornerCount > 0 )
		TotalScore /= (float)HoughCornerCount;
	
	if ( TotalScore < 0.8f )
		TotalScore = 0;
	
	//	write score in element 15
	Homographys[HomographyIndex][15] = TotalScore;
}



__kernel void DrawMaskOnFrame(int OffsetX,
							  int OffsetY,
							int HomographyIndexOffset,
							 global float16* Homographys,
							 global float16* HomographyInvs,
							   __read_only image2d_t Frame,
							 __read_only image2d_t Mask,
							 __write_only image2d_t Frag,
							  int DrawFrameOnMask,
							  int PixelSkip
							  )
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	int2 wh = get_image_dim( Frag );
	int HomographyIndex = get_global_id(2) + HomographyIndexOffset;

	if( PixelSkip>0 && (uv.x % (PixelSkip+1) == 0 ||uv.y % (PixelSkip+1) == 0) )
		return;
	
	//	read pixel
	float4 Rgba = 1;

	if ( DrawFrameOnMask )
	{
		Rgba = texture2D( Frame, uv );
	}
	else
	{
		//	dont read out of mask bounds
		int2 mask_wh = get_image_dim( Mask );
		if ( uv.x >= mask_wh.x || uv.y >= mask_wh.y )
			return;
		
		//float HomographyScore = Homographys[HomographyIndex][15];
		//Rgba.xyz = NormalToRgb(HomographyScore);
		Rgba.xyz = IndexToRgbRainbow((HomographyIndex+1)%20,20);
		int MaskIndex = RgbToIndex( texture2D( Mask, uv ).xyz, 1 );
		if ( MaskIndex > 0 )
			return;
	}
	
	//	transform uv from mask to frame
	bool UseInverse = DrawFrameOnMask ? false : true;
	float16 Homography = UseInverse ? HomographyInvs[HomographyIndex] : Homographys[HomographyIndex];
	float2 FrameUvf = Transform2ByMatrix3x3( (float2)(uv.x,uv.y), Homography );
	//float2 FrameUvf = (float2)(uv.x,uv.y);
	int2 FrameUv = (int2)(FrameUvf.x,FrameUvf.y);
	
	//	off screen
	int Border = 10;
	if ( FrameUv.x < Border || FrameUv.y < Border || FrameUv.x >= wh.x-Border || FrameUv.y >= wh.y-Border )
		return;

	write_imagef( Frag, FrameUv, Rgba );
	
}




