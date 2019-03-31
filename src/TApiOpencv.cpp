#include "TApiOpencv.h"
#include "TApiCommon.h"

#include "Libs/opencv2.framework/Headers/opencv.hpp"
#include "Libs/opencv2.framework/Headers/imgproc.hpp"


namespace ApiOpencv
{
	const char Namespace[] = "Opencv";
	
	void	FindContours(Bind::TCallback& Params);
	void	GetSaliencyRects(Bind::TCallback& Params);
	void	GetMoments(Bind::TCallback& Params);
	void	GetHogGradientMap(Bind::TCallback& Params);

	const char FindContours_FunctionName[] = "FindContours";
	const char GetSaliencyRects_FunctionName[] = "GetSaliencyRects";
	const char GetMoments_FunctionName[] = "GetMoments";
	const char GetHogGradientMap_FunctionName[] = "GetHogGradientMap";
}


void ApiOpencv::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindGlobalFunction<FindContours_FunctionName>( FindContours, Namespace );
	Context.BindGlobalFunction<GetSaliencyRects_FunctionName>( GetSaliencyRects, Namespace );
	Context.BindGlobalFunction<GetMoments_FunctionName>( GetMoments, Namespace );
	Context.BindGlobalFunction<GetHogGradientMap_FunctionName>( GetHogGradientMap, Namespace );
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

cv::Mat GetMatrix(Bind::TCallback &Params,size_t ArgumentIndex,uint8_t ThresholdMin)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(ArgumentIndex);
	
	SoyPixels PixelsMask;
	Image.GetPixels( PixelsMask );
	PixelsMask.SetFormat( SoyPixelsFormat::Greyscale );
	
	//	threshold the image
	{
		auto& PixelsArray = PixelsMask.GetPixelsArray();
		for ( auto p=0;	p<PixelsArray.GetSize();	p++ )
		{
			if ( PixelsArray[p] < ThresholdMin )
				PixelsArray[p] = 0;
		}
	}
	
	//	https://docs.opencv.org/3.4.2/da/d72/shape_example_8cpp-example.html#a1
	//cv::InputArray InputArray( GetMatrix(PixelsMask ) );
	auto InputArray = GetMatrix( PixelsMask );
	
	return InputArray;
}

void ApiOpencv::GetHogGradientMap(Bind::TCallback &Params)
{
	auto InputImage = GetMatrix( Params, 0, 10 );

	cv::HOGDescriptor Hog;
	// Size(128,64), //winSize
	// Size(16,16), //blocksize
	// Size(8,8), //blockStride,
	// Size(8,8), //cellSize,
	// 9, //nbins,
	// 0, //derivAper,
	// -1, //winSigma,
	// 0, //histogramNormType,
	// 0.2, //L2HysThresh,
	// 0 //gammal correction,
	// //nlevels=64
	//);
	
	// void HOGDescriptor::compute(const Mat& img, vector<float>& descriptors,
	//                             Size winStride, Size padding,
	//                             const vector<Point>& locations) const
	std::vector<float> descriptorsValues;
	std::vector<cv::Point> locations;
	locations.push_back( cv::Point(100,100) );
	locations.push_back( cv::Point(200,200) );
	cv::Size winStride(8,8);
	cv::Size padding(0,0);
	Hog.compute( InputImage, descriptorsValues, winStride, padding, locations);

	auto NonZeroCount = 0;
	for ( auto i=0;	i<descriptorsValues.size();	i++ )
	{
		auto Descriptor = descriptorsValues[i];
		if ( Descriptor == 0.0f )
			continue;
		std::Debug << "Found descriptor: "  << Descriptor << std::endl;
		NonZeroCount++;
	}
	std::Debug << "Found " << NonZeroCount << "/" << descriptorsValues.size() << " non-zero descriptors" << std::endl;
	
	Array<float> RectFloats;
	auto PushRect = [&](Soy::Rectf& Rect)
	{
		RectFloats.PushBack( Rect.x );
		RectFloats.PushBack( Rect.y );
		RectFloats.PushBack( Rect.w );
		RectFloats.PushBack( Rect.h );
	};
	
	for ( auto i=0;	i<locations.size();	i++ )
	{
		auto& Pos = locations[i];
		auto Size = 20;
		Soy::Rectf Rect( Pos.x-Size/2, Pos.y-Size/2, Size, Size );
		PushRect( Rect );
	}

	Params.Return( GetArrayBridge(RectFloats) );
}



void ApiOpencv::GetSaliencyRects(Bind::TCallback &Params)
{
	/*
	 cv::saliency::ObjectnessBING::computeSaliencyImpl	(	InputArray 	image,
	 OutputArray 	objectnessBoundingBox
	 )
	 */
	throw Soy::AssertException("not implemennted");
}


void ApiOpencv::GetMoments(Bind::TCallback &Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	
	SoyPixels PixelsMask;
	Image.GetPixels( PixelsMask );
	PixelsMask.SetFormat( SoyPixelsFormat::Greyscale );
	
	//	threshold the image
	{
		auto& PixelsArray = PixelsMask.GetPixelsArray();
		for ( auto p=0;	p<PixelsArray.GetSize();	p++ )
		{
			if ( PixelsArray[p] < 100 )
			PixelsArray[p] = 0;
		}
	}
	
	//	https://docs.opencv.org/3.4.2/da/d72/shape_example_8cpp-example.html#a1
	//cv::InputArray InputArray( GetMatrix(PixelsMask ) );
	auto InputArray = GetMatrix(PixelsMask );
	
	//cv::Moments ourMoment; //moments variable
	Soy::TScopeTimerPrint Timer("cv::moments",0);
	auto Moment = cv::moments( InputArray ); //calculat all the moment of image
	Timer.Stop();
	double moment10 = Moment.m10; //extract spatial moment 10
	double moment01 = Moment.m01; //extract spatial moment 01
	double area = Moment.m00; //extract central moment 00

	//	normalisation from... somewhere else
	auto x = Moment.m10 / Moment.m00;
	auto y = Moment.m01 / Moment.m00;
	//	size seems like a general matrix scalar, but not actually mappable to pixels...?
	//auto Size = Moment.m00 / PixelsMask.GetWidth() / 10.0f;
	auto Size = 50;
	
	Soy::Rectf Rect( x-Size/2, y-Size/2, Size, Size );
	BufferArray<float,4> RectFloats;
	RectFloats.PushBack( Rect.x );
	RectFloats.PushBack( Rect.y );
	RectFloats.PushBack( Rect.w );
	RectFloats.PushBack( Rect.h );

	Params.Return( GetArrayBridge(RectFloats) );
}



void ApiOpencv::FindContours(Bind::TCallback &Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	bool MakeRects = Params.IsArgumentBool(1) ? Params.GetArgumentBool(1) : false;
	bool NormaliseUv = Params.IsArgumentBool(2) ? Params.GetArgumentBool(2) : false;
	
	BufferArray<bool,4> Filter;
	if ( Params.IsArgumentArray(3) )
	{
		Params.GetArgumentArray( 3, GetArrayBridge(Filter) );
	}
	else
	{
		Filter.PushBack(true);
		Filter.PushBack(true);
		Filter.PushBack(true);
	}
	
	auto& OrigPixels = Image.GetPixels();
	
	SoyPixelsMeta PixelsMaskMeta(OrigPixels.GetWidth(), OrigPixels.GetHeight(), SoyPixelsFormat::Greyscale );
	SoyPixels PixelsMask(PixelsMaskMeta);
								 
	if ( OrigPixels.GetFormat() == SoyPixelsFormat::Greyscale )
	{
		//	copy
		PixelsMask.Copy( OrigPixels );
	}
	else if ( OrigPixels.GetFormat() == SoyPixelsFormat::RGBA )
	{
		bool Filter[3] = { true,false,false };
		//	filter & threshold
		auto& Input = OrigPixels.GetPixelsArray();
		auto& Output = PixelsMask.GetPixelsArray();
		for ( auto p=0;	p<Input.GetSize();	p+=4 )
		{
			auto r = Input[p+0] > 100;
			auto g = Input[p+1] > 100;
			auto b = Input[p+2] > 100;
			/*
			if ( r || g || b )
				std::Debug << "r=" << r << " g=" << g << " b=" <<b << std::endl;
			*/
			bool Mask = (r==Filter[0]) && (g==Filter[1]) && (b==Filter[2]);
			Output[p/4] = Mask ? 255 : 0;
		}
	}
	else
	{
		std::stringstream Error;
		Error << "ApiOpencv::FindContours Dont know what to do with " << OrigPixels.GetFormat();
		throw Soy::AssertException(Error.str());
	}
	
	
	//	https://docs.opencv.org/3.4.2/da/d72/shape_example_8cpp-example.html#a1
	//cv::InputArray InputArray( GetMatrix(PixelsMask ) );
	auto InputArray = GetMatrix( PixelsMask );
	std::vector<std::vector<cv::Point> > Contours;
	//cv::OutputArrayOfArrays Contours;
	//cv::OutputArray Hierarchy;
	
	auto Mode = cv::RETR_EXTERNAL;
	auto Method = cv::CHAIN_APPROX_SIMPLE;
	//auto Mode = cv::RETR_LIST;
	//auto Method = cv::CHAIN_APPROX_NONE;
	{
		Soy::TScopeTimerPrint Timer("cv::findContours",5);
		cv::findContours( InputArray, Contours, Mode, Method );
	}
									
	//	enumerate to arrays of points
	auto GetPointf = [&](const cv::Point& Point,float& x,float& y)
	{
		x = Point.x;
		y = Point.y;
		if ( NormaliseUv )
		{
			x /= PixelsMask.GetWidth();
			y /= PixelsMask.GetHeight();
		}
	};
	auto GetPointObject = [&](const cv::Point& Point)
	{
		auto Object = Params.mContext.CreateObjectInstance();
		float x,y;
		GetPointf( Point, x, y );
		Object.SetFloat("x", x);
		Object.SetFloat("y", y);
		return Object;
	};
	
	auto GetPoints = [&](const std::vector<cv::Point>& Points,Soy::Rectf& Rect)
	{
		auto EnumFlatArray = true;
		if ( EnumFlatArray )
		{
			//	enum giant array
			Array<float> AllPoints;
			for ( auto p=0;	p<Points.size();	p++)
			{
				float x,y;
				GetPointf( Points[p], x, y );
				
				if ( p==0 )
				{
					Rect.x = x;
					Rect.y = y;
				}
				Rect.Accumulate( x, y );
				AllPoints.PushBack( x );
				AllPoints.PushBack( y );
			}
			
			if ( MakeRects )
			{
				AllPoints.Clear();
				AllPoints.PushBack( Rect.Left() );
				AllPoints.PushBack( Rect.Top() );
				AllPoints.PushBack( Rect.GetWidth() );
				AllPoints.PushBack( Rect.GetHeight() );
			}
			
			auto Array = Params.mContext.CreateArray( GetArrayBridge(AllPoints) );
			return Array;
		}
		else
		{
			auto GetPointElement = [&](size_t Index)
			{
				return GetPointObject(Points[Index]);
			};
			auto Array = Params.mContext.CreateArray( Points.size(), GetPointElement );
			return Array;
		}
	};
	
	Array<JSValueRef> ContourArrays;
	for ( auto i=0;	i<Contours.size();	i++ )
	{
		try
		{
			//	auto discard ones that are too small (size=1 == 1 pixel)
			auto& ThisContour = Contours[i];
			//if ( ThisContour.size() < 4 )
			if ( ThisContour.size() == 1 )
				continue;
			
			Soy::Rectf Rect;
			auto Array = GetPoints( ThisContour, Rect );
			
			auto ArrayValue = JsCore::GetValue( Params.mContext.mContext, Array );
			//Params.Return( ArrayValue );	return;
			ContourArrays.PushBack( ArrayValue );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << " (skipped " << i << ")" << std::endl;
		}
	}

	auto ContoursArray = JsCore::GetArray( Params.mContext.mContext, GetArrayBridge(ContourArrays) );
	Params.Return( ContoursArray );
}
