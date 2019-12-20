#include "TApiCoreMl.h"
#include "TApiCommon.h"
#include "SoyLib/src/SoyScope.h"
#include "SoyAssert.h"


#if defined(TARGET_WINDOWS)
#include "Libs/PopCoreMl/PopCoreMl.h"
#include "Libs/PopCoreMl/TCoreMl.h"
//#pragma comment(lib,"PopCoreml.lib")
using namespace CoreMl;
/*
namespace CoreMl
{
	class TObject {};
	class TModel
	{
	public:
		virtual void	GetLabels(ArrayBridge<std::string>&& Labels) {};
		virtual void	GetObjects(const SoyPixelsImpl& Pixels, std::function<void(const TObject&)>& EnumObject) {}
		virtual void	GetLabelMap(const SoyPixelsImpl& Pixels, std::shared_ptr<SoyPixelsImpl>& MapOutput, std::function<bool(const std::string&)>& FilterLabel) {}
		virtual void	GetLabelMap(const SoyPixelsImpl& Pixels, std::function<void(vec2x<size_t>, const std::string&, ArrayBridge<float>&&)> EnumLabelMap) {}
	};

	typedef TModel TYolo;
	typedef TModel THourglass;
	typedef TModel TCpm;
	typedef TModel TOpenPose;
	typedef TModel TPosenet;
	typedef TModel TSsdMobileNet;
	typedef TModel TMaskRcnn;
	typedef TModel TDeepLab;
	typedef TModel TAppleVisionFace;
}
*/
#endif

#if defined(TARGET_OSX)
#include "SoyAvf.h"
#include "Libs/PopCoreml.framework/Headers/TCoreMl.h"
#include "Libs/PopCoreml.framework/Headers/TYolo.h"
#include "Libs/PopCoreml.framework/Headers/TCpm.h"
#include "Libs/PopCoreml.framework/Headers/TDeepLab.h"
#include "Libs/PopCoreml.framework/Headers/THourglass.h"
#include "Libs/PopCoreml.framework/Headers/TMaskRcnn.h"
#include "Libs/PopCoreml.framework/Headers/TOpenPose.h"
#include "Libs/PopCoreml.framework/Headers/TPosenet.h"
#include "Libs/PopCoreml.framework/Headers/TSsdMobileNet.h"
#include "Libs/PopCoreml.framework/Headers/TAppleVisionFace.h"
#endif

class CoreMl::TInstance : public SoyWorkerJobThread
{
public:
	TInstance();

	TModel&		GetYolo()				{	return GetModel(mYolo);	}
	TModel&		GetHourglass()			{	return GetModel(mHourglass);	}
	TModel&		GetCpm()				{	return GetModel(mCpm);	}
	TModel&		GetOpenPose()			{	return GetModel(mOpenPose);	}
	TModel&		GetPosenet()			{	return GetModel(mPosenet);	}
	TModel&		GetSsdMobileNet()		{	return GetModel(mSsdMobileNet);	}
	TModel&		GetMaskRcnn()			{	return GetModel(mMaskRcnn);	}
	TModel&		GetDeepLab()			{	return GetModel(mDeepLab);	}
	TModel&		GetAppleVisionFace()	{ return GetModel(mAppleVisionFace); }
	TModel&		GetWinSkillSkeleton();
	
	template<typename TYPE>
	TModel&		GetModel(std::shared_ptr<TYPE>& mModel);


public:
	//	temp until we fix ownership problems of promises with nested lambdas
	Bind::TPromiseMap					mPromises;

private:
	std::shared_ptr<TYolo>				mYolo;
	std::shared_ptr<THourglass>			mHourglass;
	std::shared_ptr<TCpm>				mCpm;
	std::shared_ptr<TOpenPose>			mOpenPose;
	std::shared_ptr<TPosenet>			mPosenet;
	std::shared_ptr<TSsdMobileNet>		mSsdMobileNet;
	std::shared_ptr<TMaskRcnn>			mMaskRcnn;
	std::shared_ptr<TDeepLab>			mDeepLab;
	std::shared_ptr<TAppleVisionFace>	mAppleVisionFace;

	//	dll, currently allocs and stored in the DLL
	//	should turn into a map with names
	TModel*								mWinSkillSkeleton = nullptr;
};


namespace ApiCoreMl
{
	DEFINE_BIND_TYPENAME(CoreMl);

	DEFINE_BIND_FUNCTIONNAME(Yolo);
	DEFINE_BIND_FUNCTIONNAME(Hourglass);
	DEFINE_BIND_FUNCTIONNAME(HourglassLabelMap);
	DEFINE_BIND_FUNCTIONNAME(Cpm);
	DEFINE_BIND_FUNCTIONNAME(CpmLabelMap);
	DEFINE_BIND_FUNCTIONNAME(OpenPose);
	DEFINE_BIND_FUNCTIONNAME(OpenPoseMap);
	DEFINE_BIND_FUNCTIONNAME(OpenPoseLabelMap);
	DEFINE_BIND_FUNCTIONNAME(PosenetLabelMap);
	DEFINE_BIND_FUNCTIONNAME(SsdMobileNet);
	DEFINE_BIND_FUNCTIONNAME(MaskRcnn);
	DEFINE_BIND_FUNCTIONNAME(AppleVisionFaceDetect);
	DEFINE_BIND_FUNCTIONNAME(DeepLab);
	DEFINE_BIND_FUNCTIONNAME(WinSkillSkeleton);
}


void ApiCoreMl::Bind(Bind::TContext& Context)
{
	Context.BindObjectType<TCoreMlWrapper>( ApiPop::Namespace );
}


void TCoreMlWrapper::Construct(Bind::TCallback& Arguments)
{
	mCoreMl.reset( new CoreMl::TInstance );
}

void TCoreMlWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	using namespace ApiCoreMl;
	Template.BindFunction<BindFunction::Yolo>( &TCoreMlWrapper::Yolo );
	Template.BindFunction<BindFunction::Hourglass>( &TCoreMlWrapper::Hourglass );
	Template.BindFunction<BindFunction::HourglassLabelMap>( &TCoreMlWrapper::HourglassLabelMap );
	Template.BindFunction<BindFunction::Cpm>( &TCoreMlWrapper::Cpm );
	Template.BindFunction<BindFunction::CpmLabelMap>( &TCoreMlWrapper::CpmLabelMap );
	Template.BindFunction<BindFunction::OpenPose>( &TCoreMlWrapper::OpenPose );
	Template.BindFunction<BindFunction::OpenPoseMap>( &TCoreMlWrapper::OpenPoseMap );
	Template.BindFunction<BindFunction::OpenPoseLabelMap>( &TCoreMlWrapper::OpenPoseLabelMap );
	Template.BindFunction<BindFunction::PosenetLabelMap>( &TCoreMlWrapper::PosenetLabelMap );
	Template.BindFunction<BindFunction::SsdMobileNet>( &TCoreMlWrapper::SsdMobileNet );
	Template.BindFunction<BindFunction::MaskRcnn>( &TCoreMlWrapper::MaskRcnn );
	Template.BindFunction<BindFunction::AppleVisionFaceDetect>( &TCoreMlWrapper::AppleVisionFaceDetect );
	Template.BindFunction<BindFunction::DeepLab>(&TCoreMlWrapper::DeepLab);
	Template.BindFunction<BindFunction::WinSkillSkeleton>(&TCoreMlWrapper::WinSkillSkeleton);
}



CoreMl::TInstance::TInstance() :
	SoyWorkerJobThread	("CoreMl::TInstance")
{
	//	before the other stuff crashes, this should check for the
	//	PopCoreml.framework here and try and load the dylib
	Start();
}

template<typename TYPE>
CoreMl::TModel& CoreMl::TInstance::GetModel(std::shared_ptr<TYPE>& mModel)
{
#if defined(TARGET_WINDOWS)
	throw Soy::AssertException("Unsupported on windows");
#else
	if ( !mModel )
	{
		mModel.reset( new TYPE );
	}
	return *mModel;
#endif
}


#include "SoyLib/src/SoyRuntimeLibrary.h"
std::shared_ptr<Soy::TRuntimeLibrary> LoadSomeDll(const char* Filename)
{
	std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
	return Dll;
}

CoreMl::TModel& CoreMl::TInstance::GetWinSkillSkeleton()
{
	if (mWinSkillSkeleton)
		return *mWinSkillSkeleton;

#if defined(TARGET_WINDOWS)
	//LoadSomeDll("Microsoft.AI.Skills.SkillInterfacePreview.dll");
	auto Dll = LoadSomeDll("PopCoreml.dll");

	
	std::function<int32_t()> GetVersion;
	Dll->SetFunction(GetVersion, "PopCoreml_GetVersion");
	auto Version = GetVersion();

	auto Version2 = PopCoreml_GetVersion();
#endif
	//	todo: load dll
	mWinSkillSkeleton = PopCoreml_AllocModel("WinSkillSkeleton");
	if (!mWinSkillSkeleton)
		throw Soy::AssertException("Failed to allocated model WinSkillSkeleton");
	return *mWinSkillSkeleton;
}

void RunModelGetObjects(CoreMl::TModel& ModelRef,Bind::TCallback& Params,CoreMl::TInstance& CoreMl)
{
	auto* pImage = &Params.GetArgumentPointer<TImageWrapper>(0);
	size_t PromiseRef = 0;
	auto Promisex = CoreMl.mPromises.CreatePromise(Params.mLocalContext, __FUNCTION__, PromiseRef);
	auto* pContext = &Params.mContext;
	auto* pModel = &ModelRef;
	auto* pCoreMl = &CoreMl;
	Params.Return(Promisex);

	auto QueuePromise = [=](std::function<void(Bind::TLocalContext&, Bind::TPromise&)>&& ResolveFunc)
	{
		auto RunResolve = [=](Bind::TLocalContext& Context)
		{
			pCoreMl->mPromises.Flush(PromiseRef, ResolveFunc);
		};
		pContext->Queue(RunResolve);
	};


	auto RunModel = [=]
	{
		try
		{
			//	do all the work on the thread
			auto& Image = *pImage;
			auto& CurrentPixels = Image.GetPixels();
			SoyPixels TempPixels;
			SoyPixelsImpl* pPixels = &TempPixels;
			if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
			{
				pPixels = &CurrentPixels;
			}
			else
			{
				pImage->GetPixels(TempPixels);
				TempPixels.SetFormat( SoyPixelsFormat::RGBA );
				pPixels = &TempPixels;
			}
			auto& Pixels = *pPixels;
			
			Array<CoreMl::TObject> Objects;
						
			std::function<void(const CoreMl::TObject&)> PushObject = [&](const CoreMl::TObject& Object)
			{
				Objects.PushBack(Object);
			};
			auto& Model = *pModel;
			Model.GetObjects( Pixels, PushObject );
			
			std::function<void(Bind::TLocalContext&)> OnCompleted = [=](Bind::TLocalContext& Context)
			{
				Array<Bind::TObject> Elements;
				for (auto i = 0; i < Objects.GetSize(); i++)
				{
					auto& Object = Objects[i];
					auto ObjectJs = Context.mGlobalContext.CreateObjectInstance(Context);

					ObjectJs.SetString("Label", Object.mLabel);
					ObjectJs.SetFloat("Score", Object.mScore);
					ObjectJs.SetFloat("x", Object.mRect.x);
					ObjectJs.SetFloat("y", Object.mRect.y);
					ObjectJs.SetFloat("w", Object.mRect.w);
					ObjectJs.SetFloat("h", Object.mRect.h);
					ObjectJs.SetInt("GridX", Object.mGridPos.x);
					ObjectJs.SetInt("GridY", Object.mGridPos.y);

					Elements.PushBack(ObjectJs);
				};
				auto Resolve = [&](Bind::TLocalContext& Context, Bind::TPromise& Promise)
				{
					Promise.Resolve(Context, GetArrayBridge(Elements));
				};
				pCoreMl->mPromises.Flush(PromiseRef, Resolve);
			};

			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			
			auto Reject = [=](Bind::TLocalContext& Context, Bind::TPromise& Promise)
			{
				Promise.Reject(Context, ExceptionString);
			};
			QueuePromise(Reject);
		}
	};
	
	CoreMl.PushJob(RunModel);

}



void RunModelMap(CoreMl::TModel& ModelRef,Bind::TCallback& Params,CoreMl::TInstance& CoreMl)
{
	auto* pImage = &Params.GetArgumentPointer<TImageWrapper>(0);
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
	auto* pContext = &Params.mContext;
	auto* pModel = &ModelRef;

	std::string LabelFilter;
	if ( !Params.IsArgumentUndefined(1) )
		LabelFilter = Params.GetArgumentString(1);
	
	auto RunModel = [=]
	{
		try
		{
			//	do all the work on the thread
			auto& Image = *pImage;
			auto& CurrentPixels = Image.GetPixels();
			SoyPixels TempPixels;
			SoyPixelsImpl* pPixels = &TempPixels;
			if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
			{
				pPixels = &CurrentPixels;
			}
			else
			{
				pImage->GetPixels(TempPixels);
				TempPixels.SetFormat( SoyPixelsFormat::RGBA );
				pPixels = &TempPixels;
			}
			auto& Pixels = *pPixels;
			
			//	pixels we're writing into
			std::shared_ptr<SoyPixelsImpl> MapPixels;
			
			std::function<bool(const std::string&)> FilterLabel = [&](const std::string& Label)
			{
				if ( LabelFilter.length() == 0 )
					return true;
				if ( Label != LabelFilter )
					return false;
				return true;
			};
			
			auto& Model = *pModel;
			Model.GetLabelMap( Pixels, MapPixels, FilterLabel );
			
			auto OnCompleted = [=](Bind::TLocalContext& Context)
			{
				auto MapImageObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
				auto& MapImage = MapImageObject.This<TImageWrapper>();
				MapImage.SetPixels( MapPixels );
				Promise.Resolve( Context, MapImageObject );
			};
			
			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			
			auto OnError = [=](Bind::TLocalContext& Context)
			{
				Promise.Reject( Context, ExceptionString );
			};
			pContext->Queue( OnError );
		}
	};
	
	CoreMl.PushJob(RunModel);
	Params.Return( Promise );
}


void RunModelLabelMap(CoreMl::TModel& ModelRef,Bind::TCallback& Params,CoreMl::TInstance& CoreMl)
{
	auto* pImage = &Params.GetArgumentPointer<TImageWrapper>(0);
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
	auto* pContext = &Params.mContext;
	auto* pModel = &ModelRef;
	
	
	class TLabelMap
	{
	public:
		BufferArray<float,300*300>	mMapScores;
		vec2x<size_t>				mMapSize;
		std::string					mLabel;
	};
	
	std::string LabelFilter;
	if ( !Params.IsArgumentUndefined(1) )
		LabelFilter = Params.GetArgumentString(1);
	
	auto RunModel = [=]
	{
		try
		{
			//	do all the work on the thread
			auto& Image = *pImage;
			auto& CurrentPixels = Image.GetPixels();
			SoyPixels TempPixels;
			SoyPixelsImpl* pPixels = &TempPixels;
			if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
			{
				pPixels = &CurrentPixels;
			}
			else
			{
				pImage->GetPixels(TempPixels);
				TempPixels.SetFormat( SoyPixelsFormat::RGBA );
				pPixels = &TempPixels;
			}
			auto& Pixels = *pPixels;
			
			std::function<bool(const std::string&)> FilterLabel = [&](const std::string& Label)
			{
				if ( LabelFilter.length() == 0 )
					return true;
				if ( Label != LabelFilter )
					return false;
				return true;
			};
			
			//	save maps we output to push to the javascript thread
			Array<TLabelMap> LabelMaps;
			auto EnumMap = [&](vec2x<size_t> MetaSize,const std::string& Label,ArrayBridge<float>&& Map)
			{
				if ( !FilterLabel( Label ) )
					return;
				
				auto& LabelMap = LabelMaps.PushBack();
				LabelMap.mMapScores.Copy( Map );
				LabelMap.mLabel = Label;
				LabelMap.mMapSize = MetaSize;
			};
			
			//	run
			auto& Model = *pModel;
			Model.GetLabelMap( Pixels, EnumMap );
			
			auto OnCompleted = [=](Bind::TLocalContext& Context)
			{
				auto OutputObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
				
				//	set meta
				{
					auto MetaObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
					auto Size = LabelMaps.IsEmpty() ? vec2x<size_t>(0,0) : LabelMaps[0].mMapSize;
					MetaObject.SetInt("Width", Size.x );
					MetaObject.SetInt("Height", Size.y );
					OutputObject.SetObject("Meta",MetaObject);
				}
				
				for ( auto i=0;	i<LabelMaps.GetSize();	i++ )
				{
					auto& LabelMap = LabelMaps[i];
					auto LabelMapObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
					OutputObject.SetArray( LabelMap.mLabel, GetArrayBridge(LabelMap.mMapScores) );
				}

				Promise.Resolve( Context, OutputObject );
			};
			
			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			
			auto OnError = [=](Bind::TLocalContext& Context)
			{
				Promise.Reject( Context, ExceptionString );
			};
			pContext->Queue( OnError );
		}
	};
	
	CoreMl.PushJob(RunModel);
	Params.Return( Promise );
}


void TCoreMlWrapper::Yolo(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetYolo();

	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::Hourglass(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetHourglass();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::HourglassLabelMap(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetHourglass();
	
	RunModelLabelMap( Model, Params, CoreMl );
}

void TCoreMlWrapper::Cpm(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetCpm();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::CpmLabelMap(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetCpm();
	
	RunModelLabelMap( Model, Params, CoreMl );
}



void TCoreMlWrapper::OpenPose(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetOpenPose();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::OpenPoseMap(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetOpenPose();
	
	RunModelMap( Model, Params, CoreMl );
}

void TCoreMlWrapper::OpenPoseLabelMap(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetOpenPose();
	
	RunModelLabelMap( Model, Params, CoreMl );
}

void TCoreMlWrapper::PosenetLabelMap(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetPosenet();
	
	RunModelLabelMap( Model, Params, CoreMl );
}

void TCoreMlWrapper::SsdMobileNet(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetSsdMobileNet();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::MaskRcnn(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetMaskRcnn();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::DeepLab(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetDeepLab();
	
	RunModelGetObjects( Model, Params, CoreMl );
}


void TCoreMlWrapper::AppleVisionFaceDetect(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetAppleVisionFace();

	RunModelGetObjects(Model, Params, CoreMl);
}


void TCoreMlWrapper::WinSkillSkeleton(Bind::TCallback& Params)
{
	auto& CoreMl = *mCoreMl;
	auto& Model = CoreMl.GetWinSkillSkeleton();

	RunModelGetObjects(Model, Params, CoreMl);
}


