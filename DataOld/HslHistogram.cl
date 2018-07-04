#include "Common.cl"


static int IncMax(int* Index,int Count)
{
	*Index++;
	*Index = min( *Index, Count-1 );
	return *Index;
}

kernel GenerateHslHistogram(global float4* HslHistogram,global int* HslHistogramCount,int MaxHistogramSize)
{
	float MatchSat = 0.3f;
	float MatchSatHigh = 0.6f;
	float MatchSatLow = 0.5f;
	float MatchLum = 0.3f;
	int HistogramCount = 0;
	
	float IsWhite = 1;
	float NotWhite = 0;
	
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.9f, IsWhite );	//	white
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.8f, IsWhite );	//	white
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.7f, IsWhite );	//	white
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.6f, IsWhite );	//	white
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.5f, IsWhite );	//	white
		
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0, 0, 0.1f, NotWhite );	//	black
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 0/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 20/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 50/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 90/360.f, 0.3f, 0.3f, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 90/360.f, 0.4f, 0.4f, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 90/360.f, 0.5f, 0.5f, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 150/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 190/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 205/360.f, MatchSat, MatchLum, NotWhite );
	HslHistogram[IncMax(&HistogramCount,MaxHistogramSize)] = (float4)( 290/360.f, MatchSat, MatchLum, NotWhite );
	
	*HslHistogramCount = HistogramCount;
}
