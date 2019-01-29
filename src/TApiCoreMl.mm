#include "TApiCoreMl.h"
#include "TApiCommon.h"
#include "SoyAvf.h"

//#import "MobileNet.h"
#import "TinyYOLO.h"
#import "Hourglass.h"
#import "Cpm.h"

//	openpose model from here
//	https://github.com/infocom-tpo/SwiftOpenPose
#import "MobileOpenPose.h"

using namespace v8;

const char Yolo_FunctionName[] = "Yolo";
const char Hourglass_FunctionName[] = "Hourglass";
const char Cpm_FunctionName[] = "Cpm";
const char OpenPose_FunctionName[] = "OpenPose";

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
	void		RunCpm(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);
	void		RunOpenPose(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);

private:
	void		RunPoseModel(MLMultiArray* ModelOutput,const SoyPixelsImpl& Pixels,std::function<std::string(size_t)> GetKeypointName,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject);

private:
	//MobileNet*	mMobileNet = nullptr;
	TinyYOLO*		mTinyYolo = nullptr;
	hourglass*		mHourglass = nullptr;
	cpm*			mCpm = nullptr;
	MobileOpenPose*	mOpenPose = nullptr;
};

CoreMl::TInstance::TInstance()
{
	//mMobileNet = [[MobileNet alloc] init];
	mTinyYolo = [[TinyYOLO alloc] init];
	mHourglass = [[hourglass alloc] init];
	mCpm = [[cpm alloc] init];
	mOpenPose = [[MobileOpenPose alloc] init];
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

	//	https://github.com/tucan9389/PoseEstimation-CoreML/blob/master/PoseEstimation-CoreML/JointViewController.swift#L230
	const std::string KeypointLabels[] =
	{
		"Top", "Neck",
		"RightShoulder", "RightElbow", "RightWrist",
		"LeftShoulder", "LeftElbow", "LeftWrist",
		"RightHip", "RightKnee", "RightAnkle",
		"LeftHip", "LeftKnee", "LeftAnkle",
	};

	auto GetKeypointName = [&](size_t Index)
	{
		return KeypointLabels[Index];
	};
	
	RunPoseModel( Output.hourglass_out_3__0, Pixels, GetKeypointName, EnumObject );
}

void CoreMl::TInstance::RunCpm(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	auto PixelBuffer = Avf::PixelsToPixelBuffer(Pixels);
	
	NSError* Error = nullptr;
	
	auto Output = [mCpm predictionFromImage__0:PixelBuffer error:&Error];
	if ( Error )
		throw Soy::AssertException( Error );
	
	//	https://github.com/tucan9389/PoseEstimation-CoreML/blob/master/PoseEstimation-CoreML/JointViewController.swift#L230
	const std::string KeypointLabels[] =
	{
		"Top", "Neck",
		"RightShoulder", "RightElbow", "RightWrist",
		"LeftShoulder", "LeftElbow", "LeftWrist",
		"RightHip", "RightKnee", "RightAnkle",
		"LeftHip", "LeftKnee", "LeftAnkle",
	};
	auto GetKeypointName = [&](size_t Index)
	{
		return KeypointLabels[Index];
	};

	RunPoseModel( Output.Convolutional_Pose_Machine__stage_5_out__0, Pixels, GetKeypointName, EnumObject );
}


void CoreMl::TInstance::RunOpenPose(const SoyPixelsImpl& Pixels,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	auto PixelBuffer = Avf::PixelsToPixelBuffer(Pixels);
	
	NSError* Error = nullptr;
	
	auto Output = [mOpenPose predictionFromImage:PixelBuffer error:&Error];
	if ( Error )
		throw Soy::AssertException( Error );
	
	//	https://github.com/infocom-tpo/SwiftOpenPose/blob/9745c0074dfe7d98265a325e25d2e2bb3d91d3d1/SwiftOpenPose/Sources/Estimator.swift#L122
	//	heatmap rowsxcols is width/8 and height/8 which is 48, which is dim[1] and dim[2]
	//	but 19 features? (dim[0] = 3*19)
	//	https://github.com/tucan9389/PoseEstimation-CoreML/blob/master/PoseEstimation-CoreML/JointViewController.swift#L230
	const std::string KeypointLabels[] =
	{
		"Head"
	};
	auto GetKeypointName = [&](size_t Index)
	{
		if ( Index < sizeofarray(KeypointLabels) )
			return KeypointLabels[Index];
		
		std::stringstream KeypointName;
		KeypointName << "Label_" << Index;
		return KeypointName.str();
	};
	
	auto* ModelOutput = Output.net_output;
	//RunPoseModel( Output.net_output, Pixels, GetKeypointName, EnumObject );

	if ( !ModelOutput )
		throw Soy::AssertException("No output from model");
	
	Array<int> Dim;
	Array<float> Values;
	ExtractFloatsFromMultiArray( ModelOutput, GetArrayBridge(Dim), GetArrayBridge(Values) );
	
	auto KeypointCount = Dim[0];
	auto HeatmapWidth = Dim[1];
	auto HeatmapHeight = Dim[2];
	auto GetValue = [&](int Keypoint,int HeatmapX,int HeatmapY)
	{
		auto Index = Keypoint * (HeatmapWidth*HeatmapHeight);
		Index += HeatmapX*(HeatmapHeight);
		Index += HeatmapY;
		return Values[Index];
	};
	
	//	same as dim
	//	heatRows = imageWidth / 8
	//	heatColumns = imageHeight / 8
	//	KeypointCount is 57 = 19 + 38
	auto HeatRows = HeatmapWidth;
	auto HeatColumns = HeatmapHeight;

	//	https://github.com/infocom-tpo/SwiftOpenPose/blob/9745c0074dfe7d98265a325e25d2e2bb3d91d3d1/SwiftOpenPose/Sources/Estimator.swift#L127
	//	https://github.com/eugenebokhan/iOS-OpenPose/blob/master/iOSOpenPose/iOSOpenPose/CoreML/PoseEstimatior.swift#L72
	//	code above splits into pafMat and HeatmapMat
	//	gr: row 18 is full of much bigger numbers...
	//	0..18 look good though
	auto HeatMatRows = 19;
	auto HeatMatCols = HeatRows*HeatColumns;
	float heatMat[HeatMatRows * HeatMatCols];//[19*HeatRows*HeatColumns];
	float pafMat[Values.GetSize() - sizeofarray(heatMat)];//[38*HeatRows*HeatColumns];
	//let heatMat = Matrix<Double>(rows: 19, columns: heatRows*heatColumns, elements: data )
	//let separateLen = 19*heatRows*heatColumns
	//let pafMat = Matrix<Double>(rows: 38, columns: heatRows*heatColumns, elements: Array<Double>(mm[separateLen..<mm.count]))
	for ( auto i=0;	i<sizeofarray(heatMat);	i++ )
		heatMat[i] = Values[i];
	for ( auto i=0;	i<sizeofarray(pafMat);	i++ )
		pafMat[i] = Values[sizeofarray(heatMat)+i];
	
	//	pull coords from heat map
	using vec2i = vec2x<int32_t>;
	Array<vec2i> Coords;
	for ( auto r=0;	r<HeatMatRows;	r++ )
	{
		auto nms = GetRemoteArray( &heatMat[r*HeatMatCols], HeatMatCols );
		
		std::Debug << "r=" << r << " [ ";
		for ( int i=0;	i<nms.GetSize();	i++ )
		{
			int fi = nms[i] * 100;
			std::Debug << fi << " ";
		}
		std::Debug << " ]" << std::endl;
		
		//	row 18 has massive numbers and coords look wrong...
		if ( r==18 )
			continue;
		//if ( r != 18 )
		//	continue;
		//	get biggest in nms
		//	gr: I think the original code goes through and gets rid of the not-biggest
		//		or only over the threshold?
		//		but the swift code is then doing the check twice if that's the case (and doest need the opencv filter at all)
		/*
		openCVWrapper.maximum_filter(&data,
									 data_size: Int32(data.count),
									 data_rows: dataRows,
									 mask_size: maskSize,
									 threshold: threshold)
		 */
		
		auto PushCoord = [&](int Index,float Score)
		{
			if ( Score < 0.01f )
				return;
			
			auto y = Index / HeatRows;
			auto x = Index % HeatRows;
			Coords.PushBack( vec2i(x,y) );
			
			auto KeypointLabel = GetKeypointName(r);
			auto xf = x / static_cast<float>(HeatRows);
			auto yf = y / static_cast<float>(HeatColumns);
			auto wf = 1 / static_cast<float>(HeatRows);
			auto hf = 1 / static_cast<float>(HeatColumns);
			auto Rect = Soy::Rectf( xf, yf, wf, hf );
			EnumObject( KeypointLabel, Score, Rect );
		};
		
		/*
		auto BiggestCol = 0;
		auto BiggestVal = -1.0f;
		for ( auto c=0;	c<nms.GetSize();	c++ )
		{
			if ( nms[c] <= BiggestVal )
				continue;
			BiggestCol = c;
			BiggestVal = nms[c];
		}
		
		PushCoord( BiggestCol, BiggestVal );
	*/
		for ( auto c=0;	c<nms.GetSize();	c++ )
		{
			PushCoord( c, nms[c] );
		}
		
		
	}
	/*
	var coords = [[(Int,Int)]]()
	for i in 0..<heatMat.rows-1
	 {
		 var nms = Array<Double>(heatMat.row(i))
	 	nonMaxSuppression(&nms, dataRows: Int32(heatColumns), maskSize: 5, threshold: _nmsThreshold)
	 	 let c = nms.enumerated().filter{ $0.1 > _nmsThreshold }.map
	 		{
	 	x in
			 return ( x.0 / heatRows , x.0 % heatRows )
	 		}
	 	coords.append(c)
	 }
	*/

}


void CoreMl::TInstance::RunPoseModel(MLMultiArray* ModelOutput,const SoyPixelsImpl& Pixels,std::function<std::string(size_t)> GetKeypointName,std::function<void(const std::string&,float,Soy::Rectf)> EnumObject)
{
	if ( !ModelOutput )
		throw Soy::AssertException("No output from model");

	Array<int> Dim;
	Array<float> Values;
	ExtractFloatsFromMultiArray( ModelOutput, GetArrayBridge(Dim), GetArrayBridge(Values) );
	
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

	
	static bool EnumAllResults = true;
	auto MinConfidence = 0.01f;
	
	for ( auto k=0;	k<KeypointCount;	k++)
	{
		auto KeypointLabel = GetKeypointName(k);
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
	Container.BindFunction<Cpm_FunctionName>( InstanceTemplate, Cpm );
	Container.BindFunction<OpenPose_FunctionName>( InstanceTemplate, OpenPose );

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



v8::Local<v8::Value> TCoreMlWrapper::Cpm(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TCoreMlWrapper>( ThisHandle );
	auto& Image = v8::GetObject<TImageWrapper>( Arguments[0] );
	
	auto& CoreMl = *This.mCoreMl;
	SoyPixels Pixels;
	Image.GetPixels(Pixels);
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
	CoreMl.RunCpm(Pixels,OnDetected);
	
	auto GetElement = [&](size_t Index)
	{
		return Objects[Index];
	};
	auto ObjectsArray = v8::GetArray( *Params.mIsolate, Objects.GetSize(), GetElement);
	
	return ObjectsArray;
}



v8::Local<v8::Value> TCoreMlWrapper::OpenPose(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TCoreMlWrapper>( ThisHandle );
	auto& Image = v8::GetObject<TImageWrapper>( Arguments[0] );
	
	auto& CoreMl = *This.mCoreMl;
	SoyPixels Pixels;
	Image.GetPixels(Pixels);
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
	CoreMl.RunOpenPose(Pixels,OnDetected);
	
	auto GetElement = [&](size_t Index)
	{
		return Objects[Index];
	};
	auto ObjectsArray = v8::GetArray( *Params.mIsolate, Objects.GetSize(), GetElement);
	
	return ObjectsArray;
}
