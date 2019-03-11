#include "TApiOpencv.h"
#include "TApiCommon.h"

#include "Libs/opencv2.framework/Headers/opencv.hpp"
#include "Libs/opencv2.framework/Headers/imgproc.hpp"


namespace ApiOpencv
{
	const char Namespace[] = "Opencv";
	
	void	FindContours(Bind::TCallback& Params);
	
	const char FindContours_FunctionName[] = "FindContours";
}


void ApiOpencv::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindGlobalFunction<FindContours_FunctionName>( FindContours, Namespace );
}

int GetMatrixType(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::Greyscale:	return CV_8UC1;
		
		default:
		break;
	}
	
	std::stringstream Error;
	Error << "Unhandled format " << Format << " for conversion to opencv matrix type";
	throw Soy::AssertException( Error.str() );
}

cv::Mat GetMatrix(const SoyPixelsImpl& Pixels)
{
	auto Rows = Pixels.GetHeight();
	auto Cols = Pixels.GetWidth();
	auto Type = GetMatrixType( Pixels.GetFormat() );
	auto* Data = const_cast<uint8_t*>(Pixels.GetPixelsArray().GetArray());
	
	cv::Mat Matrix( Rows, Cols, Type, Data );
	return Matrix;
}


void ApiOpencv::FindContours(Bind::TCallback &Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	
	SoyPixels PixelsMask;
	Image.GetPixels( PixelsMask );
	PixelsMask.SetFormat( SoyPixelsFormat::Greyscale );
	
	//	https://docs.opencv.org/3.4.2/da/d72/shape_example_8cpp-example.html#a1
	//cv::InputArray InputArray( GetMatrix(PixelsMask ) );
	auto InputArray = GetMatrix(PixelsMask );
	std::vector<std::vector<cv::Point> > Contours;
	//cv::OutputArrayOfArrays Contours;
	//cv::OutputArray Hierarchy;
	
	auto Mode = cv::RETR_LIST;
	auto Method = cv::CHAIN_APPROX_NONE;
	cv::findContours( InputArray, Contours, Mode, Method );
	
	//	enumerate to arrays of points
	auto GetPoint = [&](const cv::Point& Point)
	{
		auto Object = Params.mContext.CreateObjectInstance();
		Object.SetFloat("x", Point.x);
		Object.SetFloat("y", Point.y);
		return Object;
	};
	
	auto GetPoints = [&](const std::vector<cv::Point>& Points)
	{
		auto GetPointElement = [&](size_t Index)
		{
			return GetPoint(Points[Index]);
		};
		auto Array = Params.mContext.CreateArray( Points.size(), GetPointElement );
		return Array;
	};
	
	Array<JSValueRef> ContourArrays;
	for ( auto i=0;	i<Contours.size();	i++ )
	{
		auto Array = GetPoints(Contours[i]);
		auto ArrayValue = JsCore::GetValue( Params.mContext.mContext, Array );
		ContourArrays.PushBack( ArrayValue );
	}
	auto ContoursArray = JsCore::GetArray( Params.mContext.mContext, GetArrayBridge(ContourArrays) );
	Params.Return( ContoursArray );
}
