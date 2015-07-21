in vec2 fTexCoord;
uniform sampler2D cylinderfiltered;
uniform float2 cylinderfiltered_TexelWidthHeight;

//uniform int SampleRadius = 8;	//	range (0,30)


/*
 //	make a grid spiraling out from bottom right, and apply for 4 corners
 00	aa	dd	hh	mm	ss	a	i
 bb	cc	ee	ii	nn	tt	b	j
 ff	gg	jj	oo	uu	c	k
 ll	kk	pp	vv	d	l
 rr	qq  ww	e	m
 xx	yy	f	n
 g	h	o
 p	q
 
 
 */

//	giant list of coords to check, spiraling outwards
#define RadiusOffsetCount	43+1
uniform float2 RadiusOffsets[RadiusOffsetCount] = float2[](

	//0 		a 			b 			c 			d 			e 			f	 		g
	float2(0,0),	float2(1,0),	float2(0,1),	float2(1,1),	float2(2,0),	float2(2,1),	float2(0,2),	float2(1,2),
	//h			i			j			k			l
	float2(3,0),	float2(3,1),	float2(2,2),	float2(1,3),	float2(0,3),
	//m			n			o			p			q			r
	float2(4,0),	float2(4,1),	float2(3,2),	float2(2,3),	float2(1,4),	float2(0,4),
	//s			t			u			v			w			x			y
	float2(5,0),	float2(5,1),	float2(4,2),	float2(3,3),	float2(2,4),	float2(0,5),	float2(1,5),
	
	//	a		b			c			d			e			f			g			h
	float2(6,0),	float2(6,1),	float2(5,2),	float2(4,3),	float2(3,4),	float2(4,5),	float2(1,6),	float2(0,6),
	
	//	i		j			k			l			m			n			o			p			q
	float2(7,0),	float2(7,1),	float2(6,2),	float2(5,3),	float2(4,4),	float2(3,5),	float2(2,6),	float2(1,7),	float2(0,7),
														   
	float2(0,0)
);
#define MaxRadiusDistance	float(7)	//	if a const, dx sees 0

/*
//	to stop noisy blobs, check at waves
#define RadiusOffsetWaveStartCount	7
const int RadiusOffsetWaveStarts[RadiusOffsetWaveStartCount] =
{
	7,	//	gg
	12,	//	ll
	18,	//	rr
	25,	//	yy
	33,	//	h
	42,	//	q
	9999,
};
*/


float4 GetSample(float2 PixelOffset)
{
	//	work out uv
	float2 uv = fTexCoord + (PixelOffset * cylinderfiltered_TexelWidthHeight.xy);
	return texture2D( cylinderfiltered, uv );
}

bool IsGoodSample(float4 Sample)
{
	return Sample.w > 0.5f;
}

float GetValue(float4 Sample)
{
	return Sample.g;
}

float2 Update(float2 BigNeighbourAndValue,float2 Offset,float BaseValue)
{
	Offset = float2(1,1);
	float4 MatchSample = GetSample( Offset );
						
	if ( !IsGoodSample(MatchSample) )
		return BigNeighbourAndValue;
						
	float MatchValue = GetValue( MatchSample );
	BigNeighbourAndValue.y = max( BigNeighbourAndValue.y, MatchValue );
	BigNeighbourAndValue.x += (MatchValue < BaseValue) ? 1 : 0;
	
	return BigNeighbourAndValue;
}


void main()
{
	float4 BaseSample = GetSample( float2(0,0) );

	if ( !IsGoodSample( BaseSample ) )
	{
		gl_FragColor = BaseSample*float4(1,1,1,0);
		return;
	}

	float BaseValue = GetValue( BaseSample );
	
	float BiggerNeighbourRadius = 0;
	float BigValue = GetValue(BaseSample);

	float2 BigNeighbourAndValue = float2( BiggerNeighbourRadius, BigValue );
	
	for ( int r=0;	r<12;	r++ )
	{
		BigNeighbourAndValue = Update( BigNeighbourAndValue, RadiusOffsets[r]*float2(-1,-1), BaseValue );
		BigNeighbourAndValue = Update( BigNeighbourAndValue, RadiusOffsets[r]*float2( 1,-1), BaseValue );
		BigNeighbourAndValue = Update( BigNeighbourAndValue, RadiusOffsets[r]*float2( 1, 1), BaseValue );
		BigNeighbourAndValue = Update( BigNeighbourAndValue, RadiusOffsets[r]*float2(-1, 1), BaseValue );
	}

	BiggerNeighbourRadius = BigNeighbourAndValue.x;
	BigValue = BigNeighbourAndValue.y;
	
	//	we're best if we keep alpha
	float WeAreBiggestf = (BiggerNeighbourRadius == 0) ? 1 : 0;
	BaseSample.w = WeAreBiggestf;
	//	gl_FragColor = BaseSample;
	gl_FragColor = float4(1,1,1,WeAreBiggestf);
}
