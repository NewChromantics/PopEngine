#define const	__constant


const float2 MaxRectSize = (float2)(80,100);

static bool MakeRectMatch(int4* Match,int2 TexCoord,float4 Sample)
{
	float2 RectSize = Sample.xy * MaxRectSize;

	//	alignment is relative to the rect
	float2 Alignment = (Sample.zw - (float2)(0.5f,0.5f)) * RectSize;
	
	//	if width or height zero then not a match
	if ( RectSize.x == 0 || RectSize.y == 0 )
		return false;

	//int2 Min = TexCoord - (int2)(Alignment);
	//int2 Max = Min + (int2)(RectSize);
	int2 Min = TexCoord;
	int2 Max = Min + (int2)(RectSize);
	
	//	store min/max of rect
	Match->x = Min.x;
	Match->y = Min.y;
	Match->z = Max.x;
	Match->w = Max.y;
	
	
	return true;
}



static void DrawLineHorz(int2 Start,int2 End,__write_only image2d_t Frag,float4 Colour)
{
	int y = Start.y;
	for ( int x=Start.x;	x<=End.x;	x++ )
	{
		write_imagef( Frag, (int2)(x,y), Colour );
	}
}

static void DrawLineVert(int2 Start,int2 End,__write_only image2d_t Frag,float4 Colour)
{
	int x = Start.x;
	for ( int y=Start.y;	y<=End.y;	y++ )
	{
		write_imagef( Frag, (int2)(x,y), Colour );
	}
}




__kernel void DrawRects(int OffsetX,int OffsetY,__read_only image2d_t rectfilter,__read_only image2d_t undistort,__write_only image2d_t Frag)
{
	int tx = get_global_id(0) + OffsetX;
	int ty = get_global_id(1) + OffsetY;
	
	
	//	copy the original for background. will get non-uniformly overwritten
	//	because of the paralell execution, but simpler setup
	{
		sampler_t CopySampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
		float4 CopyPixel = read_imagef( undistort, CopySampler, (int2)(tx,ty) );
		//write_imagef( Frag, (int2)(tx,ty), CopyPixel );
	}
	
	int2 wh = get_image_dim(rectfilter);
	
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float4 Sample = read_imagef( rectfilter, Sampler, (int2)(tx,ty) );

	int4 Match = (int4)( tx, ty, tx+10, ty+10 );
	if ( !MakeRectMatch( &Match, (int2)(tx,ty), Sample ) )
		return;

	float4 rgba = (float4)( tx/(float)wh.x, ty/(float)wh.y,1,1);
	
	
	int Minx = max( 0, min( wh.x-1, Match.x ) );
	int Miny = max( 0, min( wh.y-1, Match.y ) );
	int Maxx = max( 0, min( wh.x-1, Match.z ) );
	int Maxy = max( 0, min( wh.y-1, Match.w ) );
	/*
	int Minx = max( 0, min( wh.x-1, Match.x ) );
	int Miny = max( 0, min( wh.y-1, Match.y ) );
	int Maxx = max( 0, min( wh.x-1, Match.x+10 ) );
	int Maxy = max( 0, min( wh.y-1, Match.y+10 ) );
*/
	
	DrawLineHorz( (int2)(Minx,Miny), (int2)(Maxx,Miny), Frag, rgba );
	DrawLineHorz( (int2)(Minx,Maxy), (int2)(Maxx,Maxy), Frag, rgba );
	DrawLineVert( (int2)(Minx,Miny), (int2)(Minx,Maxy), Frag, rgba );
	DrawLineVert( (int2)(Maxx,Miny), (int2)(Maxx,Maxy), Frag, rgba );
}
