#include "TApiPanopoly.h"
#include "TApiCommon.h"

namespace ApiPanopoly
{
	const char Namespace[] = "Pop.Panopoly";

	DEFINE_BIND_FUNCTIONNAME(DepthToYuvAsync);

	void	DepthToYuvAsync(Bind::TCallback& Params);
}

namespace Panopoly
{
	class TContext;
	std::shared_ptr<TContext>	gContext;
	TContext& 					GetContext();
}


class Panopoly::TContext
{
public:
	TContext() :
		mJobQueueA("Panopoly::JobQueueA"),
		mJobQueueB("Panopoly::JobQueueB")
	{
		mJobQueueA.Start();
		mJobQueueB.Start();
	}
	
	void				QueueJob(std::function<void()> Job)
	{
		mQueueFlip++;
		auto& Queue = (mQueueFlip&1) ? mJobQueueA : mJobQueueB;
		Queue.PushJob(Job);
	}

	size_t				mQueueFlip = 0;
	SoyWorkerJobThread	mJobQueueA;
	SoyWorkerJobThread	mJobQueueB;

	Bind::TPromiseMap	mPromises;
};


Panopoly::TContext& Panopoly::GetContext()
{
	if ( gContext )
		return *gContext;
	
	gContext.reset( new TContext() );
	return *gContext;
}


void ApiPanopoly::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::DepthToYuvAsync>( DepthToYuvAsync, Namespace );
}


//#define TEST_OUTPUT
#include "PopDepthToYuv/Depth16ToYuv.c"


void DepthToYuv(SoyPixelsImpl& DepthPixels,SoyPixelsImpl& YuvPixels,EncodeParams_t EncodeParams,SoyPixelsFormat::Type OutputYuvFormat)
{
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,20);
	auto Width = DepthPixels.GetWidth();
	auto Height = DepthPixels.GetHeight();
	YuvPixels.Init(SoyPixelsMeta( Width, Height, OutputYuvFormat ));
	BufferArray<std::shared_ptr<SoyPixelsImpl>, 3> YuvPlanes;
	YuvPixels.SplitPlanes(GetArrayBridge(YuvPlanes));

	struct Meta_t
	{
		uint8_t* LumaPixels = nullptr;
		uint8_t* ChromaUPixels = nullptr;
		uint8_t* ChromaVPixels = nullptr;
		uint16_t* ChromaUVPixels = nullptr;
		size_t Width = 0;
	};
	Meta_t Meta;
	Meta.LumaPixels = YuvPlanes[0]->GetPixelsArray().GetArray();
	if (YuvPlanes.GetSize() == 3)
	{
		Meta.ChromaUPixels = YuvPlanes[1]->GetPixelsArray().GetArray();
		Meta.ChromaVPixels = YuvPlanes[2]->GetPixelsArray().GetArray();
	}
	else if (YuvPlanes.GetSize() == 2)
	{
		auto* uv8 = YuvPlanes[1]->GetPixelsArray().GetArray();
		Meta.ChromaUVPixels = reinterpret_cast<uint16_t*>(uv8);
	}
	Meta.Width = Width;

	
	auto WriteYuv_8_8_8 = [](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV, void* pMeta)
	{
		auto& Meta = *reinterpret_cast<Meta_t*>(pMeta);
		auto cx = x / 2;
		auto cy = y / 2;
		auto cw = Meta.Width/2;
		auto i = x + (y*Meta.Width);
		auto ci = cx + (cy*cw);
		Meta.LumaPixels[i] = Luma;
		Meta.ChromaUPixels[ci] = ChromaU;
		Meta.ChromaVPixels[ci] = ChromaV;
		//LumaPlane.SetPixel(x, y, 0, Luma);
		//ChromaUVPlane.SetPixel(cx, cy, 0, ChromaU);
		//ChromaUVPlane.SetPixel(cx, cy, 1, ChromaV);
	};

	auto WriteYuv_8_88 = [](uint32_t x, uint32_t y, uint8_t Luma, uint8_t ChromaU, uint8_t ChromaV, void* pMeta)
	{
		auto& Meta = *reinterpret_cast<Meta_t*>(pMeta);
		auto cx = x / 2;
		auto cy = y / 2;
		auto cw = Meta.Width / 2;
		auto i = x + (y*Meta.Width);
		auto ci = cx + (cy*cw);
		Meta.LumaPixels[i] = Luma;
		auto* ChromaUv8 = reinterpret_cast<uint8_t*>(&Meta.ChromaUVPixels[ci]);
		ChromaUv8[0] = ChromaU;
		ChromaUv8[1] = ChromaV;
	};
	
	auto OnErrorCAPI = [](const char* Error,void* This)
	{
		//	todo throw exception, but needs a more complicated This for the call
		std::Debug << "Depth16ToYuv error; " << Error << std::endl;
	};
	
	if (DepthPixels.GetFormat() == SoyPixelsFormat::Depth16mm && YuvPixels.GetFormat() == SoyPixelsFormat::Yuv_8_8_8)
	{
		auto* Depth16 = reinterpret_cast<uint16_t*>(DepthPixels.GetPixelsArray().GetArray());
		Depth16ToYuv(Depth16, Width, Height, EncodeParams, WriteYuv_8_8_8, OnErrorCAPI, &Meta);
		return;
	}
	else if (DepthPixels.GetFormat() == SoyPixelsFormat::DepthFloatMetres && YuvPixels.GetFormat() == SoyPixelsFormat::Yuv_8_8_8)
	{
		auto* Depthf = reinterpret_cast<float*>(DepthPixels.GetPixelsArray().GetArray());
		DepthfToYuv(Depthf, Width, Height, EncodeParams, WriteYuv_8_8_8, OnErrorCAPI, &Meta);
		return;
	}
	else if (DepthPixels.GetFormat() == SoyPixelsFormat::Depth16mm && YuvPixels.GetFormat() == SoyPixelsFormat::Yuv_8_88)
	{
		auto* Depth16 = reinterpret_cast<uint16_t*>(DepthPixels.GetPixelsArray().GetArray());
		Depth16ToYuv(Depth16, Width, Height, EncodeParams, WriteYuv_8_88, OnErrorCAPI, &Meta);
		return;
	}
	else if (DepthPixels.GetFormat() == SoyPixelsFormat::DepthFloatMetres && YuvPixels.GetFormat() == SoyPixelsFormat::Yuv_8_88)
	{
		auto* Depthf = reinterpret_cast<float*>(DepthPixels.GetPixelsArray().GetArray());
		DepthfToYuv(Depthf, Width, Height, EncodeParams, WriteYuv_8_88, OnErrorCAPI, &Meta);
		return;
	}

	
	std::stringstream Error;
	Error << "Unsupported pixel formats from:" << DepthPixels.GetFormat() << " to " << YuvPixels.GetFormat() << " for conversion to depth yuv";
	throw Soy::AssertException(Error);
}


void ApiPanopoly::DepthToYuvAsync(Bind::TCallback& Params)
{
	//	depth image in, promise out
	auto& DepthImage = Params.GetArgumentPointer<TImageWrapper>(0);
	std::shared_ptr<SoyPixelsImpl> pDepthPixels( new SoyPixels );
	DepthImage.GetPixels(*pDepthPixels);
	
	//	get encoder params
	EncodeParams_t EncodeParams;
	EncodeParams.DepthMin = Params.GetArgumentInt(1);
	EncodeParams.DepthMax = Params.GetArgumentInt(2);
	EncodeParams.ChromaRangeCount = Params.GetArgumentInt(3);
	EncodeParams.PingPongLuma = Params.GetArgumentBool(4);

	//	gr: make a GetArgumentStringEnum() func for nice auto errors
	if (!Params.IsArgumentString(5))
		throw Soy::AssertException("Now expecting YUV output format param");
	auto EncodeYuvFormatString = Params.GetArgumentString(5);
	SoyPixelsFormat::Type EncodeYuvFormat = SoyPixelsFormat::ToType(EncodeYuvFormatString);
	
	//	create a promise in a promise map so we don't accidentally delete it in the wrong thread
	auto& PanopolyContext = Panopoly::GetContext();
	size_t PromiseRef = 0;
	{
		auto Promise = PanopolyContext.mPromises.CreatePromise(Params.mLocalContext, __PRETTY_FUNCTION__, PromiseRef);
		Params.Return(Promise);
	}

	//	do conversion in a job
	auto Convert = [=]()
	{
		try
		{
			pDepthPixels->Flip();
			std::shared_ptr<SoyPixelsImpl> pYuvPixels( new SoyPixels );
			DepthToYuv(*pDepthPixels,*pYuvPixels,EncodeParams, EncodeYuvFormat);
			
			auto ResolvePromise = [=](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
			{
				//	make an image and return
				BufferArray<JSValueRef, 1> ConstructorArguments;
				auto ImageObject = LocalContext.mGlobalContext.CreateObjectInstance(LocalContext, TImageWrapper::GetTypeName(), GetArrayBridge(ConstructorArguments));
				auto& Image = ImageObject.This<TImageWrapper>();
				Image.SetPixels(pYuvPixels);
				Promise.Resolve(LocalContext, ImageObject);
			};
			auto& PanopolyContext = Panopoly::GetContext();
			PanopolyContext.mPromises.Flush(PromiseRef, ResolvePromise);
		}
		catch(std::exception& e)
		{
			std::Debug << __PRETTY_FUNCTION__ << e.what() << std::endl;
			std::string Error = e.what();
			auto RejectPromise = [=](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
			{
				Promise.Reject(LocalContext,Error);
			};
			auto& PanopolyContext = Panopoly::GetContext();
			PanopolyContext.mPromises.Flush(PromiseRef, RejectPromise);
		}
	};
	PanopolyContext.QueueJob(Convert);
}



