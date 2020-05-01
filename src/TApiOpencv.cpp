#include "TApiOpencv.h"
#include "TApiCommon.h"
#include "MagicEnum/include/magic_enum.hpp"
#include "SoyLib/src/SoyRuntimeLibrary.h"

#if defined(TARGET_WINDOWS)
//#include "Libs/Opencv/include/opencv2/opencv.hpp"
//#include "Libs/Opencv/Contrib/modules/aruco/include/opencv2/aruco.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/aruco.hpp"
#else
#include "Libs/opencv2.framework/Headers/opencv.hpp"
#include "Libs/opencv2.framework/Headers/imgproc.hpp"
#include "Libs/opencv2.framework/Headers/aruco.hpp"
#endif

namespace ApiOpencv
{
	const char Namespace[] = "Pop.Opencv";
	
	void	FindContours(Bind::TCallback& Params);
	void	GetSaliencyRects(Bind::TCallback& Params);
	void	GetMoments(Bind::TCallback& Params);
	void	GetHogGradientMap(Bind::TCallback& Params);
	void	FindArucoMarkers(Bind::TCallback& Params);
	void	SolvePnp(Bind::TCallback& Params);
	void	Undistort(Bind::TCallback& Params);
	
	//	testing for ER conversion
	void	TestDepthToYuv844(Bind::TCallback& Params);
	void	TestDepthToYuv8_8_8(Bind::TCallback& Params);
	
	
	DEFINE_BIND_FUNCTIONNAME(FindContours);
	DEFINE_BIND_FUNCTIONNAME(GetSaliencyRects);
	DEFINE_BIND_FUNCTIONNAME(GetMoments);
	DEFINE_BIND_FUNCTIONNAME(GetHogGradientMap);
	DEFINE_BIND_FUNCTIONNAME(FindArucoMarkers);
	DEFINE_BIND_FUNCTIONNAME(SolvePnp);
	DEFINE_BIND_FUNCTIONNAME(Undistort);
	DEFINE_BIND_FUNCTIONNAME(TestDepthToYuv844);
	DEFINE_BIND_FUNCTIONNAME(TestDepthToYuv8_8_8);

	//	const
	DEFINE_BIND_FUNCTIONNAME(ArucoMarkerDictionarys);
	cv::Ptr<cv::aruco::Dictionary>	GetArucoDictionary(const std::string& Name);
	void							GetArucoDictionarys(ArrayBridge<std::string>&& Names);

	//	call this before using any opencv functions for more friendly error
	void		LoadDll();
	std::shared_ptr<Soy::TRuntimeLibrary> Dll;
}

void ApiOpencv::LoadDll()
{
	if (Dll)
		return;

#if defined(TARGET_WINDOWS)
	Dll.reset(new  Soy::TRuntimeLibrary("opencv_world410d.dll"));
	//Dll.reset(new  Soy::TRuntimeLibrary("opencv_world410.dll"));
#endif

}


void ApiOpencv::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindGlobalFunction<BindFunction::FindContours>( FindContours, Namespace );
	Context.BindGlobalFunction<BindFunction::GetSaliencyRects>( GetSaliencyRects, Namespace );
	Context.BindGlobalFunction<BindFunction::GetMoments>( GetMoments, Namespace );
	Context.BindGlobalFunction<BindFunction::GetHogGradientMap>( GetHogGradientMap, Namespace );
	Context.BindGlobalFunction<BindFunction::FindArucoMarkers>( FindArucoMarkers, Namespace );
	Context.BindGlobalFunction<BindFunction::SolvePnp>( SolvePnp, Namespace );
	Context.BindGlobalFunction<BindFunction::Undistort>( Undistort, Namespace );
	Context.BindGlobalFunction<BindFunction::TestDepthToYuv844>(TestDepthToYuv844, Namespace);
	Context.BindGlobalFunction<BindFunction::TestDepthToYuv8_8_8>(TestDepthToYuv8_8_8, Namespace );

	
	auto BindArucoDictionaryNames = [](Bind::TLocalContext& Context)
	{
		auto NamespaceObject = Context.mGlobalContext.GetGlobalObject( Context, Namespace );
		//constexpr auto DictionaryNames = magic_enum::enum_names<cv::aruco::PREDEFINED_DICITONARYNA>();
		BufferArray<std::string,100> DictionaryNames;
		GetArucoDictionarys( GetArrayBridge(DictionaryNames) );
		NamespaceObject.SetArray( BindFunction::ArucoMarkerDictionarys, GetArrayBridge(DictionaryNames) );
	};
	Context.Execute( BindArucoDictionaryNames );
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

SoyPixelsFormat::Type GetPixelsType(int MatrixType)
{
	switch ( MatrixType )
	{
		case CV_8UC1:	return SoyPixelsFormat::Greyscale;
	
		default:
			break;
	}

	std::stringstream Error;
	Error << "Unhandled format " << MatrixType << " for conversion from opencv matrix type";
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


void GetPixels(SoyPixelsImpl& Pixels,cv::Mat& ImageMatrix)
{
	auto Height = ImageMatrix.rows;
	auto Width = ImageMatrix.cols;
	auto Type = GetPixelsType( ImageMatrix.type() );
	auto* Data = ImageMatrix.ptr();
	auto DataSize = Height * Width * 1;
	
	SoyPixelsRemote ImageMatrixPixels( Data, Width, Height, DataSize, Type );
	Pixels.Copy( ImageMatrixPixels );
}

cv::Mat GetMatrix(ArrayBridge<float>&& Floats,size_t Columns,const std::string& Context)
{
	if ( (Floats.GetSize() % Columns) != 0 )
	{
		std::stringstream Error;
		Error << "Elements(x" << Floats.GetSize() << ") in " << Context << " doesn't align to " << Columns;
		throw Soy::AssertException( Error );
	}
	
	auto Rows = Floats.GetSize() / Columns;
	auto Cols = Columns;
	auto Type = CV_32F;//CV_32FC(Columns);
	
	
	cv::Mat Matrix( Rows, Cols, Type, Floats.GetArray() );
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
	
	auto LocalContext = Params.mLocalContext.mLocalContext;
									
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
		auto Object = Params.mContext.CreateObjectInstance( Params.mLocalContext );
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
			
			auto Array = Bind::GetArray( LocalContext, GetArrayBridge(AllPoints) );
			return Array;
		}
		else
		{
			Array<Bind::TObject> PointObjects;
			for ( auto i=0;	i<Points.size();	i++ )
			{
				auto Obj = GetPointObject(Points[i]);
				PointObjects.PushBack(Obj);
			}
			auto Array = Bind::GetArray( LocalContext, GetArrayBridge( PointObjects ) );
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
			
			auto ArrayValue = JsCore::GetValue( Params.GetContextRef(), Array );
			//Params.Return( ArrayValue );	return;
			ContourArrays.PushBack( ArrayValue );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << " (skipped " << i << ")" << std::endl;
		}
	}

	auto ContoursArray = JsCore::GetArray( Params.GetContextRef(), GetArrayBridge(ContourArrays) );
	Params.Return( ContoursArray );
}


void ApiOpencv::GetArucoDictionarys(ArrayBridge<std::string>&& Names)
{
	constexpr auto EnumNames = magic_enum::enum_names<cv::aruco::PREDEFINED_DICTIONARY_NAME>();
	for ( auto EnumName : EnumNames )
	{
		std::string EnumNameStr( EnumName );
		Names.PushBack( EnumNameStr );
	}
}

cv::Ptr<cv::aruco::Dictionary> ApiOpencv::GetArucoDictionary(const std::string& Name)
{
	auto Enum = magic_enum::enum_cast<cv::aruco::PREDEFINED_DICTIONARY_NAME>(Name);
	if ( !Enum.has_value() )
		throw Soy::AssertException( std::string("Unknown dictionary ") + Name);
	
	auto DictionaryEnum = *Enum;
	auto Dictionary = cv::aruco::getPredefinedDictionary( DictionaryEnum );
	return Dictionary;
}

void ApiOpencv::FindArucoMarkers(Bind::TCallback &Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	SoyPixels OrigPixels;
	Image.GetPixels(OrigPixels);

	auto InputArray = GetMatrix( OrigPixels );

	auto DictionaryName = Params.GetArgumentString(1);
	auto Dictionary = GetArucoDictionary( DictionaryName );
	
	std::vector<std::vector<cv::Point> > Contours;
	auto DetectorParams = cv::aruco::DetectorParameters::create();

	auto CornerRefinementMethod = static_cast<cv::aruco::CornerRefineMethod>( DetectorParams->cornerRefinementMethod );
	if ( !Params.IsArgumentUndefined(2) )
	{
		auto MethodName = Params.GetArgumentString(2);
		auto Method = magic_enum::enum_cast<cv::aruco::CornerRefineMethod>(MethodName);
		if ( !Method.has_value() )
		{
			std::stringstream Error;
			Error << "No CornerRefineMethod named " << MethodName;
			throw Soy::AssertException(Error);
		}
		CornerRefinementMethod = *Method;
	}
	else
	{
		auto DefaultName = magic_enum::enum_name<cv::aruco::CornerRefineMethod>( CornerRefinementMethod );
		std::Debug << "Using default corner refinement method: " << DefaultName << std::endl;
	}
	DetectorParams->cornerRefinementMethod = CornerRefinementMethod;
	

	std::vector<int> FoundIds;
	std::vector<std::vector<cv::Point2f>> FoundCorners;
	std::vector<std::vector<cv::Point2f>> RejectedCorners;

	//	3x3 camera matrix
	cv::InputArray cameraMatrix = cv::noArray();
	
	{
		Soy::TScopeTimerPrint Timer("cv::aruco::detectMarkers",20);
		cv::aruco::detectMarkers( InputArray, Dictionary, FoundCorners, FoundIds, DetectorParams, RejectedCorners, cameraMatrix );
	}
	
	auto GetCornerObject = [&](std::vector<cv::Point2f>& Corners,int Id)
	{
		Array<float> CornerFloats;
		for ( auto c=0;	c<Corners.size();	c++ )
		{
			CornerFloats.PushBack( Corners[c].x );
			CornerFloats.PushBack( Corners[c].y );
		}
		
		auto CornerObject = Params.mLocalContext.mGlobalContext.CreateObjectInstance( Params.mLocalContext );
		CornerObject.SetArray("Points", GetArrayBridge(CornerFloats) );
		if ( Id >= 0 )
			CornerObject.SetInt("Id", Id );
		return CornerObject;
	};
	
	//	output
	auto Results = Params.mLocalContext.mGlobalContext.CreateObjectInstance( Params.mLocalContext );
	Array<Bind::TObject> FoundCornerObjects;
	Array<Bind::TObject> RejectedCornerObjects;
	for ( auto i=0;	i<FoundCorners.size();	i++ )
	{
		auto& Corner = FoundCorners[i];
		auto Id = FoundIds[i];
		auto CornerObject = GetCornerObject( Corner, Id );
		FoundCornerObjects.PushBack(CornerObject);
	}
	
	for ( auto i=0;	i<RejectedCorners.size();	i++ )
	{
		auto& Corner = RejectedCorners[i];
		auto Id = -1;
		auto CornerObject = GetCornerObject( Corner, Id );
		RejectedCornerObjects.PushBack(CornerObject);
	}
	
	Results.SetArray("Markers", GetArrayBridge(FoundCornerObjects) );
	Results.SetArray("Rejects", GetArrayBridge(RejectedCornerObjects) );

	Params.Return(Results);
}

void GetArray(ArrayBridge<float>&& Values,cv::Mat& Matrix)
{
	for ( auto r=0;	r<Matrix.rows;	r++ )
		for ( auto c=0;	c<Matrix.cols;	c++ )
			Values.PushBack( Matrix.at<float>(c,r) );

}


void ApiOpencv::SolvePnp(Bind::TCallback& Params)
{
	Array<float> Object3;	//	3d points on plane
	Array<float> Object2;	//	2d points
	Array<float> CameraProjectionMatrix;	//	3x3
	Array<float> DistortionCoefs;

	Params.GetArgumentArray( 0, GetArrayBridge(Object3) );
	Params.GetArgumentArray( 1, GetArrayBridge(Object2) );
	Params.GetArgumentArray( 2, GetArrayBridge(CameraProjectionMatrix) );
	if ( !Params.IsArgumentUndefined(3) )
		Params.GetArgumentArray( 3, GetArrayBridge(DistortionCoefs) );
	
	auto Object3Mat = GetMatrix( GetArrayBridge(Object3), 3, "Object 3D points" );
	auto Object2Mat = GetMatrix( GetArrayBridge(Object2), 2, "Object 2D points" );
	auto CameraMat = GetMatrix( GetArrayBridge(CameraProjectionMatrix), 3, "Camera projection matrix 3x3" );
	
	cv::Mat DistortionMat;
	if ( !DistortionCoefs.IsEmpty() )
		DistortionMat = GetMatrix( GetArrayBridge(DistortionCoefs), 5, "Distortion Coeffs" );
	//cv::noArray();
	
	cv::Mat RotationVec( 3, 1, CV_32FC1);
	cv::Mat TranslationVec( 3, 1, CV_32FC1);
	
	{
		Soy::TScopeTimerPrint Timer("cv::aruco::solvePnP",10);
	
		//Mat opoints = Object3Mat.getMat();
		//int npoints = std::max(opoints.checkVector(3, CV_32F), opoints.checkVector(3, CV_64F));
		
		try
		{
			cv::solvePnP( Object3Mat, Object2Mat, CameraMat, DistortionMat, RotationVec, TranslationVec );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			throw;
		}
	}
	

	//	the vectors from SolvePnp forms the matrix to place the object in camera space
	//	therefore the matrix is ObjectToView/ObjectTo[CameraObject] (in -1..1, -1..1 space as SolvePnp takes no projection, it's in the normalised frustum)
	//	object transform in camera space
	auto ObjectTranslation = TranslationVec;
	cv::Mat ObjectRotation;
	Rodrigues(RotationVec, ObjectRotation);
	ObjectRotation.convertTo(ObjectRotation, CV_32F);
	ObjectTranslation.convertTo(ObjectTranslation, CV_32F);

	cv::Mat ObjectToCamera = cv::Mat::eye(4, 4, CV_32F);
	ObjectRotation.copyTo( ObjectToCamera.rowRange(0, 3).colRange(0, 3) );
	ObjectTranslation.copyTo( ObjectToCamera.rowRange(0, 3).col(3) );

	//	we want the inverse, to get Camera position relative to object[space]
	//	https://github.com/fta2012/WiimotePositionTrackingDemo/blob/master/demo.cpp#L207
	//	"Find the inverse of the extrinsic matrix (should be the same as just calling extrinsic.inv())"
	// 	"inverse of a rotational matrix is its transpose"
	cv::Mat CameraRotation = ObjectRotation.inv();
	
	//	un-rotate the translation
	//	gr: DONT do -mtx which just negates everything and makes things more confusing
	//	camera pos in object space
	cv::Mat CameraTranslation = CameraRotation * ObjectTranslation;
	//	camera space to object space (cameralocal->objectlocal)
	cv::Mat CameraToObject = cv::Mat::eye(4, 4, CV_32F);
	CameraRotation.copyTo( CameraToObject.rowRange(0, 3).colRange(0, 3) );
	CameraTranslation.copyTo( CameraToObject.rowRange(0, 3).col(3) );
	
	//	now that we don't negate everything, the X is the only coordinate space that's backwards for our engine
	float NegateXAxisMatrix[] =
	{
		-1,0,0,
		0,1,0,
		0,0,1,
	};
	cv::Mat NegateXAxisMat( 3, 3, CV_32F, NegateXAxisMatrix );
	CameraTranslation = NegateXAxisMat * CameraTranslation;
	
	//	gr: lets output seperate things instead of one matrix...
	BufferArray<float,3> PosArray;
	PosArray.PushBack( CameraTranslation.at<float>(0) );
	PosArray.PushBack( CameraTranslation.at<float>(1) );
	PosArray.PushBack( CameraTranslation.at<float>(2) );
	
	BufferArray<float,3*3> RotArray;
	RotArray.PushBack( CameraRotation.at<float>(0,0) );
	RotArray.PushBack( CameraRotation.at<float>(1,0) );
	RotArray.PushBack( CameraRotation.at<float>(2,0) );
	RotArray.PushBack( CameraRotation.at<float>(0,1) );
	RotArray.PushBack( CameraRotation.at<float>(1,1) );
	RotArray.PushBack( CameraRotation.at<float>(2,1) );
	RotArray.PushBack( CameraRotation.at<float>(0,2) );
	RotArray.PushBack( CameraRotation.at<float>(1,2) );
	RotArray.PushBack( CameraRotation.at<float>(2,2) );

	auto Output = Params.mLocalContext.mGlobalContext.CreateObjectInstance( Params.mLocalContext );
	
	Output.SetArray("Translation", GetArrayBridge(PosArray) );
	Output.SetArray("Rotation", GetArrayBridge(RotArray) );
	Params.Return(Output);
}

//#define TEST_OUTPUT
#include "PopDepthToYuv/Depth16ToYuv.c"

void TestDepthToYuv844_Opencv(uint16_t* Depth16,int Width,int Height,cv::Mat& LumaPlane,cv::Mat& ChromaUvPlane,uint16_t DepthMin,uint16_t DepthMax,int ChromaRanges)
{
	ApiOpencv::LoadDll();

	EncodeParams_t Params;
	Params.DepthMax = DepthMax;
	Params.DepthMin = DepthMin;
	Params.ChromaRangeCount = ChromaRanges;

	auto LumaSize = Width * Height;
	auto ChromaSize = (Width/2) * (Height/2);
	
	//	write to opencv matrixes
	LumaPlane = cv::Mat( Height, Width, CV_8UC1 );
	ChromaUvPlane = cv::Mat( Height/2, Width, CV_8UC1 );
	//cv::Mat luminance(Height, Width,CV_8UC1);
	//cv::Mat uv(ChromaHeight,ChromaWidth*2,CV_8UC1);	//	width*2 as its 2 interleaved values
	auto& luminance = LumaPlane;
	auto& uv = ChromaUvPlane;
	
	auto WriteYuv = [&](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV)
	{
		auto ux = (x / 2) * 2;
		auto vx = ux + 1;
		auto uy = y / 2;
		auto vy = y / 2;

		luminance.at<uint8_t>(y, x) = Luma;
		uv.at<uint8_t>(uy, ux) = ChromaU;
		uv.at<uint8_t>(vy, vx) = ChromaV;
	};
	auto WriteCAPI = [](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV, void* WriteYuvPtr)
	{
		auto& Functor = *reinterpret_cast<decltype(WriteYuv)*>(WriteYuvPtr);
		Functor(x, y, Luma, ChromaU, ChromaV);
	};
	
	auto OnErrorCAPI = [](const char* Error,void* This)
	{
		//	todo throw exception, but needs a more complicated This for the call
		std::Debug << "Depth16ToYuv error; " << Error << std::endl;
	};

	Depth16ToYuv(Depth16, Width, Height, Params, WriteCAPI, OnErrorCAPI, &WriteYuv);
}


void ApiOpencv::TestDepthToYuv844(Bind::TCallback &Params)
{
	//	pull out the input depth image
	auto& DepthImage = Params.GetArgumentPointer<TImageWrapper>(0);
	auto& DepthPixels = DepthImage.GetPixels();
	auto DepthMin = Params.GetArgumentInt(1);
	auto DepthMax = Params.GetArgumentInt(2);
	auto ChromaRanges = Params.GetArgumentInt(3);

	uint16_t* Depth16 = reinterpret_cast<uint16_t*>(DepthPixels.GetPixelsArray().GetArray());
	auto w = DepthPixels.GetWidth();
	auto h = DepthPixels.GetHeight();

	cv::Mat LumaPlane;
	cv::Mat ChromaUvPlane;

	TestDepthToYuv844_Opencv(Depth16, w, h, LumaPlane, ChromaUvPlane, DepthMin, DepthMax, ChromaRanges);

	SoyPixelsRemote LumaPlanePixels(LumaPlane.data, LumaPlane.cols, LumaPlane.rows, LumaPlane.total(), SoyPixelsFormat::Luma_Ntsc);
	SoyPixelsRemote ChromaUvPlanePixels(ChromaUvPlane.data, ChromaUvPlane.cols / 2, ChromaUvPlane.rows, ChromaUvPlane.total(), SoyPixelsFormat::ChromaUV_44);

	//	merge back to Yuv844
	SoyPixels Yuv844Pixels(SoyPixelsMeta(w, h, SoyPixelsFormat::Yuv_844_Full));
	BufferArray<std::shared_ptr<SoyPixelsImpl>, 2> Planes;
	Yuv844Pixels.SplitPlanes(GetArrayBridge(Planes));
	Planes[0]->Copy(LumaPlanePixels);
	Planes[1]->Copy(ChromaUvPlanePixels);

	//	return an image
	auto& Context = Params.mContext;
	auto ImageObject = Context.CreateObjectInstance(Params.mLocalContext, TImageWrapper::GetTypeName());
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.SetPixels(Yuv844Pixels);
	//Image.SetPixels(LumaPlanePixels);
	//Image.SetPixels(DepthPixels);
	Params.Return(ImageObject);
}

void ApiOpencv::TestDepthToYuv8_8_8(Bind::TCallback &Params)
{
	//	pull out the input depth image
	auto& DepthImage = Params.GetArgumentPointer<TImageWrapper>(0);
	auto& DepthPixels = DepthImage.GetPixels();

	EncodeParams_t EncodeParams;
	EncodeParams.DepthMin = Params.GetArgumentInt(1);
	EncodeParams.DepthMax = Params.GetArgumentInt(2);
	EncodeParams.ChromaRangeCount = Params.GetArgumentInt(3);

	uint16_t* Depth16 = reinterpret_cast<uint16_t*>(DepthPixels.GetPixelsArray().GetArray());
	auto Width = DepthPixels.GetWidth();
	auto Height = DepthPixels.GetHeight();

	//	merge back to Yuv844
	SoyPixels YuvPixels(SoyPixelsMeta(Width, Height, SoyPixelsFormat::Yuv_8_8_8_Full));
	BufferArray<std::shared_ptr<SoyPixelsImpl>, 3> Planes;
	YuvPixels.SplitPlanes(GetArrayBridge(Planes));
	auto& LumaPlane = *Planes[0];
	auto& ChromaUPlane = *Planes[1];
	auto& ChromaVPlane = *Planes[2];

	auto WriteYuv = [&](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV)
	{
		auto cx = x / 2;
		auto cy = y / 2;
		LumaPlane.SetPixel(x, y, 0, Luma);
		ChromaUPlane.SetPixel(cx, cy, 0, ChromaU);
		ChromaVPlane.SetPixel(cx, cy, 0, ChromaV);
	};
	auto WriteCAPI = [](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV, void* WriteYuvPtr)
	{
		auto& Functor = *reinterpret_cast<decltype(WriteYuv)*>(WriteYuvPtr);
		Functor(x, y, Luma, ChromaU, ChromaV);
	};
	
	auto OnErrorCAPI = [](const char* Error,void* This)
	{
		//	todo throw exception, but needs a more complicated This for the call
		std::Debug << "Depth16ToYuv error; " << Error << std::endl;
	};
	
	Depth16ToYuv(Depth16, Width, Height, EncodeParams, WriteCAPI, OnErrorCAPI, &WriteYuv);

	//	return an image
	auto& Context = Params.mContext;
	auto ImageObject = Context.CreateObjectInstance(Params.mLocalContext, TImageWrapper::GetTypeName());
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.SetPixels(YuvPixels);
	Params.Return(ImageObject);
}


void ApiOpencv::Undistort(Bind::TCallback& Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	SoyPixels OrigPixels;
	Image.GetPixels(OrigPixels);
	
	auto InputImageMat = GetMatrix( OrigPixels );

	Array<float> CameraProjectionMatrix;	//	3x3
	Array<float> DistortionCoefs;
	Params.GetArgumentArray( 1, GetArrayBridge(CameraProjectionMatrix) );
	Params.GetArgumentArray( 2, GetArrayBridge(DistortionCoefs) );
	
	auto CameraMat = GetMatrix( GetArrayBridge(CameraProjectionMatrix), 3, "Camera projection matrix 3x3" );
	auto DistortionMat = GetMatrix( GetArrayBridge(DistortionCoefs), 5, "Distortion Coeffs" );
	
	cv::Mat UndistortedImage;
	cv::undistort( InputImageMat, UndistortedImage, CameraMat, DistortionMat );
	
	GetPixels( OrigPixels, UndistortedImage );
	Image.SetPixels( OrigPixels );
}




