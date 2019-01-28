#include "TApiCoreMl.h"
#include "TApiCommon.h"
#include "SoyAvf.h"

//#import "MobileNet.h"
#import "TinyYOLO.h"
#import "Hourglass.h"

using namespace v8;

const char Yolo_FunctionName[] = "Yolo";
const char Hourglass_FunctionName[] = "Hourglass";

const char CoreMl_TypeName[] = "CoreMl";


void ApiCoreMl::Bind(TV8Container& Container)
{
	Container.BindObjectType( TCoreMlWrapper::GetObjectTypeName(), TCoreMlWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TCoreMlWrapper> );
}


class CoreMl::TInstance
{
public:
	TInstance();
	
	//void		RunMobileNet(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);
	void		RunYolo(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);
	void		RunHourglass(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);

	//MobileNet*	mMobileNet = nullptr;
	TinyYOLO*	mTinyYolo = nullptr;
	hourglass*	mHourglass = nullptr;
};

CoreMl::TInstance::TInstance()
{
	//mMobileNet = [[MobileNet alloc] init];
	mTinyYolo = [[TinyYOLO alloc] init];
	mHourglass = [[hourglass alloc] init];
}
/*
void CoreMl::TInstance::RunMobileNet(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	auto PixelBuffer = Avf::PixelsToPixelBuffer(Pixels);

	CVImageBufferRef ImageBuffer = PixelBuffer;
	NSError* Error = nullptr;
	auto Output = [mMobileNet predictionFromImage:ImageBuffer error:&Error];
	
	if ( Error )
		throw Soy::AssertException( Error );
	
	throw Soy::AssertException("Enum output");
}
*/

void CoreMl::TInstance::RunYolo(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	auto PixelBuffer = Avf::PixelsToPixelBuffer(Pixels);
	
	CVImageBufferRef ImageBuffer = PixelBuffer;
	NSError* Error = nullptr;
	
	auto Output = [mTinyYolo predictionFromImage:PixelBuffer error:&Error];
	if ( Error )
		throw Soy::AssertException( Error );

	//	parse grid output
	//	https://github.com/hollance/YOLO-CoreML-MPSNNGraph/blob/master/TinyYOLO-CoreML/TinyYOLO-CoreML/YOLO.swift
	MLMultiArray* Grid = Output.grid;
	if ( Grid.count != 125*13*13 )
	{
		std::stringstream Error;
		Error << "expected 125*13*13(21125) outputs, got " << Grid.count;
		throw Soy::AssertException(Error.str());
	}
	
	//	from https://github.com/hollance/YOLO-CoreML-MPSNNGraph/blob/a1241f3cf1fd155039c45ad4f681bb5f22897654/Common/Helpers.swift
	const std::string ClassLabels[] =
	{
		"aeroplane", "bicycle", "bird", "boat", "bottle", "bus", "car", "cat",
		"chair", "cow", "diningtable", "dog", "horse", "motorbike", "person",
		"pottedplant", "sheep", "sofa", "train", "tvmonitor"
	};
	
	
	//	from hollance/YOLO-CoreML-MPSNNGraph
	// The 416x416 image is divided into a 13x13 grid. Each of these grid cells
	// will predict 5 bounding boxes (boxesPerCell). A bounding box consists of
	// five data items: x, y, width, height, and a confidence score. Each grid
	// cell also predicts which class each bounding box belongs to.
	//
	// The "features" array therefore contains (numClasses + 5)*boxesPerCell
	// values for each grid cell, i.e. 125 channels. The total features array
	// contains 125x13x13 elements.
	
	std::function<float(int)> GetGridIndexValue;
	switch ( Grid.dataType )
	{
		case MLMultiArrayDataTypeDouble:
		GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<double*>( Grid.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
		break;
		
		case MLMultiArrayDataTypeFloat32:
		GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<float*>( Grid.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
		break;
		
		case MLMultiArrayDataTypeInt32:
		GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<int32_t*>( Grid.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
		break;
		
		default:
			throw Soy::AssertException("Unhandled grid data type");
	}
	
	auto gridHeight = 13;
	auto gridWidth = 13;
	auto blockWidth = Pixels.GetWidth() / gridWidth;
	auto blockHeight = Pixels.GetHeight() / gridHeight;
	auto boxesPerCell = 5;
	auto numClasses = 20;
	
	auto channelStride = Grid.strides[0].intValue;
	auto yStride = Grid.strides[1].intValue;
	auto xStride = Grid.strides[2].intValue;
	
	auto GetIndex = [&](int channel,int x,int y)
	{
		return channel*channelStride + y*yStride + x*xStride;
	};
	
	auto GetGridValue = [&](int channel,int x,int y)
	{
		return GetGridIndexValue( GetIndex( channel, x, y ) );
	};
	
	//	https://github.com/hollance/YOLO-CoreML-MPSNNGraph/blob/master/TinyYOLO-CoreML/TinyYOLO-CoreML/Helpers.swift#L217
	auto Sigmoid = [](float x)
	{
		return 1.0f / (1.0f + exp(-x));
	};
/*
	//	https://stackoverflow.com/a/10733861/355753
	auto Sigmoid = [](float x)
	{
		return x / (1.0f + abs(x));
	};
	*/
	for ( auto cy=0;	cy<gridHeight;	cy++ )
	for ( auto cx=0;	cx<gridWidth;	cx++ )
	for ( auto b=0;	b<boxesPerCell;	b++ )
	{
		// For the first bounding box (b=0) we have to read channels 0-24,
		// for b=1 we have to read channels 25-49, and so on.
		auto channel = b*(numClasses + 5);
		auto tx = GetGridValue(channel    , cx, cy);
		auto ty = GetGridValue(channel + 1, cx, cy);
		auto tw = GetGridValue(channel + 2, cx, cy);
		auto th = GetGridValue(channel + 3, cx, cy);
		auto tc = GetGridValue(channel + 4, cx, cy);
		
		// The confidence value for the bounding box is given by tc. We use
		// the logistic sigmoid to turn this into a percentage.
		auto confidence = Sigmoid(tc);
		if ( confidence < 0.01f )
			continue;
		std::Debug << "Confidence: " << confidence << std::endl;
		
		// The predicted tx and ty coordinates are relative to the location
		// of the grid cell; we use the logistic sigmoid to constrain these
		// coordinates to the range 0 - 1. Then we add the cell coordinates
		// (0-12) and multiply by the number of pixels per grid cell (32).
		// Now x and y represent center of the bounding box in the original
		// 416x416 image space.
		auto RectCenterx = (cx + Sigmoid(tx)) * blockWidth;
		auto RectCentery = (cy + Sigmoid(ty)) * blockHeight;
		
		// The size of the bounding box, tw and th, is predicted relative to
		// the size of an "anchor" box. Here we also transform the width and
		// height into the original 416x416 image space.
		//	let anchors: [Float] = [1.08, 1.19, 3.42, 4.41, 6.63, 11.38, 9.42, 5.11, 16.62, 10.52]
		float anchors[] = { 1.08, 1.19, 3.42, 4.41, 6.63, 11.38, 9.42, 5.11, 16.62, 10.52 };
		auto RectWidth = exp(tw) * anchors[2*b    ] * blockWidth;
		auto RectHeight = exp(th) * anchors[2*b + 1] * blockHeight;
		//auto RectWidth = exp(tw) * blockWidth;
		//auto RectHeight = exp(th) * blockHeight;
		
		
		//	iterate through each class
		float BestClassScore = 0;
		int BestClassIndex = -1;
		for ( auto ClassIndex=0;	ClassIndex<numClasses;	ClassIndex++)
		{
			auto Score = GetGridValue( channel + 5 + ClassIndex, cx, cy );
			if ( ClassIndex > 0 && Score < BestClassScore )
				continue;
	
			BestClassIndex = ClassIndex;
			BestClassScore = Score;
		}
		//auto Score = BestClassScore * confidence;
		auto Score = confidence;
		auto& Label = ClassLabels[BestClassIndex];
		
		auto x = RectCenterx - (RectWidth/2);
		auto y = RectCentery - (RectHeight/2);
		auto w = RectWidth;
		auto h = RectHeight;
		//	normalise output
		x /= Pixels.GetWidth();
		y /= Pixels.GetHeight();
		w /= Pixels.GetWidth();
		h /= Pixels.GetHeight();
		EnumObject( Label, Score, Soy::Rectf( x,y,w,h ) );
		
	}
}


void NSArray_NSNumber_ForEach(NSArray<NSNumber*>* Numbers,std::function<void(int64_t)> Enum)
{
	auto Size = [Numbers count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto* Num = [Numbers objectAtIndex:i];
		auto Integer = [Num integerValue];
		int64_t Integer64 = Integer;
		Enum( Integer64 );
	}
}

void ExtractFloatsFromMultiArray(MLMultiArray* MultiArray,ArrayBridge<int>&& Dimensions,ArrayBridge<float>&& Values)
{
	//	get dimensions
	NSArray_NSNumber_ForEach( MultiArray.shape, [&](int64_t DimSize)	{	Dimensions.PushBack(DimSize);	} );
	
	//	convert all values now
	//	get a functor for the different types
	std::function<float(int)> GetGridIndexValue;
	switch ( MultiArray.dataType )
	{
		case MLMultiArrayDataTypeDouble:
			GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<double*>( MultiArray.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
			break;
			
		case MLMultiArrayDataTypeFloat32:
			GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<float*>( MultiArray.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
			break;
			
		case MLMultiArrayDataTypeInt32:
			GetGridIndexValue = [&](int Index)
		{
			auto* GridValues = reinterpret_cast<int32_t*>( MultiArray.dataPointer );
			return static_cast<float>( GridValues[Index] );
		};
			break;
			
		default:
			throw Soy::AssertException("Unhandled grid data type");
	}
	
	//	todo: this should match dim
	auto ValueCount = MultiArray.count;
	for ( auto i=0;	i<ValueCount;	i++ )
	{
		//	this could probably be faster than using the functor
		auto Valuef = GetGridIndexValue(i);
		Values.PushBack(Valuef);
	}
}

void CoreMl::TInstance::RunHourglass(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	auto PixelBuffer = Avf::PixelsToPixelBuffer(Pixels);
	
	NSError* Error = nullptr;
	
	auto Output = [mHourglass predictionFromImage__0:PixelBuffer error:&Error];
	if ( Error )
		throw Soy::AssertException( Error );

	Array<int> Dim;
	Array<float> Values;
	ExtractFloatsFromMultiArray( Output.hourglass_out_3__0, GetArrayBridge(Dim), GetArrayBridge(Values) );
	
	if ( Values.GetSize() != 14*48*48 )
	{
		std::stringstream Error;
		Error << "expected 14*48*48(32256) outputs, got " << Values.GetSize();
		throw Soy::AssertException(Error.str());
	}
	
	//	parse Hourglass data
	//	https://github.com/tucan9389/PoseEstimation-CoreML/blob/master/PoseEstimation-CoreML/JointViewController.swift#L135
	auto KeypointCount = Dim[0];
	auto HeatmapWidth = Dim[2];
	auto HeatmapHeight = Dim[1];
	auto GetValue = [&](int Keypoint,int HeatmapX,int HeatmapY)
	{
		auto Index = Keypoint * (HeatmapWidth*HeatmapHeight);
		//Index += HeatmapX*(HeatmapHeight);
		//Index += HeatmapY;
		Index += HeatmapY*(HeatmapWidth);
		Index += HeatmapX;
		return Values[Index];
	};

	//	https://github.com/tucan9389/PoseEstimation-CoreML/blob/master/PoseEstimation-CoreML/JointViewController.swift#L230
	const std::string KeypointLabels[] =
	{
		"Top", "Neck",
		"RightShoulder", "RightElbow", "RightWrist",
		"LeftShoulder", "LeftElbow", "LeftWrist",
		"RightHip", "RightKnee", "RightAnkle",
		"LeftHip", "LeftKnee", "LeftAnkle",
	};
	
	static bool EnumAllResults = false;
	auto MinConfidence = 0.01f;
	
	for ( auto k=0;	k<KeypointCount;	k++)
	{
		auto KeypointLabel = KeypointLabels[k];
		Soy::Rectf BestRect;
		float BestScore = -1;
		auto EnumKeypoint = [&](const std::string& Label,float Score,const Soy::Rectf& Rect)
		{
			if ( EnumAllResults )
			{
				EnumObject( Label, Score, Rect );
				return;
			}
			if ( Score < BestScore )
				return;
			BestRect = Rect;
			BestScore = Score;
		};

		for ( auto x=0;	x<HeatmapWidth;	x++)
		{
			for ( auto y=0;	y<HeatmapHeight;	y++)
			{
				auto Confidence = GetValue( k, x, y );
				if ( Confidence < MinConfidence )
					continue;
				
				auto xf = x / static_cast<float>(HeatmapWidth);
				auto yf = y / static_cast<float>(HeatmapHeight);
				auto wf = 1 / static_cast<float>(HeatmapWidth);
				auto hf = 1 / static_cast<float>(HeatmapHeight);
				auto Rect = Soy::Rectf( xf, yf, wf, hf );
				EnumKeypoint( KeypointLabel, Confidence, Rect );
			}
		}
		
		//	if only outputting best, do it
		if ( !EnumAllResults && BestScore > 0 )
		{
			auto KeypointLabel = KeypointLabels[k];
			EnumObject( KeypointLabel, BestScore, BestRect );
		}
		
	}
	
	
}

void TCoreMlWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	mCoreMl.reset( new CoreMl::TInstance );
}

Local<FunctionTemplate> TCoreMlWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	Container.BindFunction<Yolo_FunctionName>( InstanceTemplate, Yolo );
	Container.BindFunction<Hourglass_FunctionName>( InstanceTemplate, Hourglass );

	return ConstructorFunc;
}



v8::Local<v8::Value> TCoreMlWrapper::Yolo(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TCoreMlWrapper>( ThisHandle );
	auto& Image = v8::GetObject<TImageWrapper>( Arguments[0] );

	auto& CoreMl = *This.mCoreMl;
	SoyPixels Pixels;
	Image.GetPixels(Pixels);
	Pixels.ResizeFastSample(416,416);
	Pixels.SetFormat( SoyPixelsFormat::RGBA );

	Array<Local<Value>> Objects;
	
	auto OnDetected = [&](const std::string& Label,float Score,Soy::Rectf Rect)
	{
		//std::Debug << "Detected rect " << Label << " " << static_cast<int>(Score*100.0f) << "% " << Rect << std::endl;
		auto Object = v8::Object::New( Params.mIsolate );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "Label"), v8::String::NewFromUtf8( Params.mIsolate, Label.c_str() ) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "Score"), v8::Number::New(Params.mIsolate, Score) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "x"), v8::Number::New(Params.mIsolate, Rect.x) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "y"), v8::Number::New(Params.mIsolate, Rect.y) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "w"), v8::Number::New(Params.mIsolate, Rect.w) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "h"), v8::Number::New(Params.mIsolate, Rect.h) );
		Objects.PushBack(Object);
	};
	CoreMl.RunYolo(Pixels,OnDetected);
	
	auto GetElement = [&](size_t Index)
	{
		return Objects[Index];
	};
	auto ObjectsArray = v8::GetArray( *Params.mIsolate, Objects.GetSize(), GetElement);

	return ObjectsArray;
}




v8::Local<v8::Value> TCoreMlWrapper::Hourglass(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TCoreMlWrapper>( ThisHandle );
	auto& Image = v8::GetObject<TImageWrapper>( Arguments[0] );
	
	auto& CoreMl = *This.mCoreMl;
	SoyPixels Pixels;
	Image.GetPixels(Pixels);
	//	maybe throw and make input correct at high level
	Pixels.ResizeFastSample(192,192);
	Pixels.SetFormat( SoyPixelsFormat::RGBA );
	
	Array<Local<Value>> Objects;
	
	auto OnDetected = [&](const std::string& Label,float Score,Soy::Rectf Rect)
	{
		//std::Debug << "Detected rect " << Label << " " << static_cast<int>(Score*100.0f) << "% " << Rect << std::endl;
		auto Object = v8::Object::New( Params.mIsolate );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "Label"), v8::String::NewFromUtf8( Params.mIsolate, Label.c_str() ) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "Score"), v8::Number::New(Params.mIsolate, Score) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "x"), v8::Number::New(Params.mIsolate, Rect.x) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "y"), v8::Number::New(Params.mIsolate, Rect.y) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "w"), v8::Number::New(Params.mIsolate, Rect.w) );
		Object->Set( v8::String::NewFromUtf8( Params.mIsolate, "h"), v8::Number::New(Params.mIsolate, Rect.h) );
		Objects.PushBack(Object);
	};
	CoreMl.RunHourglass(Pixels,OnDetected);
	
	auto GetElement = [&](size_t Index)
	{
		return Objects[Index];
	};
	auto ObjectsArray = v8::GetArray( *Params.mIsolate, Objects.GetSize(), GetElement);
	
	return ObjectsArray;
}

