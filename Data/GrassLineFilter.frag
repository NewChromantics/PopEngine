in vec2 uv;
uniform sampler2D GrassMask;
uniform sampler2D Hsl;
const float LumBrighterMin = 0.1;
const float GrassNeighbourMinScore = 0.2;

float2 GetUv()
{
	return float2( uv.x, 1-uv.y );
}

float GetLumSample(vec2 uvoffset)
{
	vec3 hsl = texture( Hsl, GetUv()+uvoffset ).xyz;
	return hsl.z;
}

float GetGrassMask(vec2 uvoffset)
{
	vec4 GrassMaskSample = texture( GrassMask, GetUv()+uvoffset );
	return GrassMaskSample.w;
}

void main()
{
	vec2 ImageSize = vec2( 1280, 720 );
	vec2 uvstep2 = 1.0 / ImageSize;
#define NeighbourCount	(3*3)
	float NeighbourLums[NeighbourCount];
	float NeighbourGrassMasks[NeighbourCount];
	vec2 NeighbourSteps[NeighbourCount] =
	vec2[](
		   vec2(-1,-1),	vec2(0,-1),	vec2(1,-1),
		   vec2(-1,0),	vec2(0,0),	vec2(1,-1),
		   vec2(-1,1),	vec2(0,1),	vec2(1,1)
		   );
	
	for ( int n=0;	n<NeighbourCount;	n++ )
	{
		vec2 UvOffset = NeighbourSteps[n] * uvstep2;
		NeighbourLums[n] = GetLumSample( UvOffset);
		NeighbourGrassMasks[n] = GetGrassMask( UvOffset );
	}
	
	float NeighbourGrassCount = 0;
	float BiggestDiff = 0;
#define THIS_INDEX	4
	float ThisLum = NeighbourLums[THIS_INDEX];
	for ( int n=0;	n<NeighbourCount;	n++ )
	{
		if ( n == THIS_INDEX )
			continue;
		NeighbourGrassCount += NeighbourGrassMasks[n];
		float Diff = ThisLum - NeighbourLums[n];
		BiggestDiff = max( Diff, BiggestDiff );
	}
	NeighbourGrassCount /= (NeighbourCount-1);
	
	if ( BiggestDiff > LumBrighterMin )
	{
		gl_FragColor = vec4(1,1,1,1);
		
		if ( NeighbourGrassCount < GrassNeighbourMinScore )
			gl_FragColor = vec4(0,0,0,1);
	}
	else
	{
		gl_FragColor = vec4(0,0,0,1);
	}

}
