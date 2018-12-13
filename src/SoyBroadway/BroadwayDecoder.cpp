#include "BroadwayDecoder.h"
#include <sstream>
#include "SoyDebug.h"
#include "SoyPixels.h"


namespace Broadway
{
	void	IsOkay(H264SwDecRet Result,const char* Context);
}

Broadway::TDecoder::TDecoder()
{
	auto disableOutputReordering = false;
	auto Result = H264SwDecInit( &mDecoderInstance, disableOutputReordering );
	if ( Result != H264SWDEC_OK)
	{
		std::stringstream Error;
		Error << "H264SwDecInit failed: " << Result;
		throw Soy::AssertException(Error.str());
	}
	/*
	mDecoderInstance.pStream = decoder[i]->byteStrmStart;
	mDecoderInstance.dataLen = strmLen;
	mDecoderInstance.intraConcealmentMethod = 0;
	 */
}

Broadway::TDecoder::~TDecoder()
{
	H264SwDecRelease( &mDecoderInstance );
/*
	if (decoder[i]->foutput)
	fclose(decoder[i]->foutput);
	
	free(decoder[i]->byteStrmStart);
	
	free(decoder[i]);
 */
}

std::string GetDecodeResultString(H264SwDecRet Result)
{
	switch ( Result )
	{
		case H264SWDEC_OK:	return "H264SWDEC_OK";
		case H264SWDEC_STRM_PROCESSED:	return "H264SWDEC_STRM_PROCESSED";
		case H264SWDEC_PIC_RDY:	return "H264SWDEC_PIC_RDY";
		case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:	return "H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY";
		case H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY:	return "H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY";
		case H264SWDEC_PARAM_ERR:	return "H264SWDEC_PARAM_ERR";
		case H264SWDEC_STRM_ERR:	return "H264SWDEC_STRM_ERR";
		case H264SWDEC_NOT_INITIALIZED:	return "H264SWDEC_NOT_INITIALIZED";
		case H264SWDEC_MEMFAIL:	return "H264SWDEC_MEMFAIL";
		case H264SWDEC_INITFAIL:	return "H264SWDEC_INITFAIL";
		case H264SWDEC_HDRS_NOT_RDY:	return "H264SWDEC_HDRS_NOT_RDY";
		case H264SWDEC_EVALUATION_LIMIT_EXCEEDED:	return "H264SWDEC_EVALUATION_LIMIT_EXCEEDED";
		default:
		{
			std::stringstream Error;
			Error << "Unhandled H264SwDecRet: " << Result;
			return Error.str();
		}
	}
}


void Broadway::IsOkay(H264SwDecRet Result,const char* Context)
{
	switch ( Result )
	{
		case H264SWDEC_OK:
		case H264SWDEC_STRM_PROCESSED:
		case H264SWDEC_PIC_RDY:
		case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:
		case H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY:
			return;
		
		default:
			break;
	}
	
	std::stringstream Error;
	Error << "Broadway error: " << GetDecodeResultString(Result) << " in " << Context;
	throw Soy::AssertException(Error.str());
}

//	returns true if more data to proccess
bool Broadway::TDecoder::DecodeNextPacket(std::function<void(const SoyPixelsImpl&)> OnFrameDecoded)
{
	if ( mPendingData.IsEmpty() )
		return false;
	
	const unsigned IntraGrayConcealment = 0;
	const unsigned IntraReferenceConcealment = 1;
	
	H264SwDecInput Input;
	Input.pStream = mPendingData.GetArray();
	Input.dataLen = mPendingData.GetDataSize();
	Input.picId = 0;
	Input.intraConcealmentMethod = IntraGrayConcealment;
	
	H264SwDecOutput Output;
	Output.pStrmCurrPos = nullptr;
	
	auto Result = H264SwDecDecode( mDecoderInstance, &Input, &Output );
	IsOkay( Result, "H264SwDecDecode" );
	
	//	calc what data wasn't used
	auto BytesProcessed = static_cast<ssize_t>(Output.pStrmCurrPos - Input.pStream);
	std::Debug << "H264SwDecDecode result: " << GetDecodeResultString(Result) << ". Bytes processed: "  << BytesProcessed << "/" << Input.dataLen << std::endl;
	
	//	gr: can we delete data here? or do calls below use this data...
	mPendingData.RemoveBlock(0, BytesProcessed);

	
	auto GetMeta = [&]()
	{
		H264SwDecInfo Meta;
		auto GetInfoResult = H264SwDecGetInfo( mDecoderInstance, &Meta );
		IsOkay( GetInfoResult, "H264SwDecGetInfo" );
		return Meta;
	};
	
	switch( Result )
	{
		case H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY:
		{
			auto Meta = GetMeta();
			OnMeta( Meta );
			return true;
		}
		
		case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:
			//	ref code eats data, then falls through...
		//	android just does both https://android.googlesource.com/platform/frameworks/av/+/2b6f22dc64d456471a1dc6df09d515771d1427c8/media/libstagefright/codecs/on2/h264dec/source/EvaluationTestBench.c#158
		case H264SWDEC_PIC_RDY:
		{
			auto Meta = GetMeta();
			H264SwDecPicture Picture;
			u32 EndOfStream = false;
			while ( true )
			{
				//	decode pictures until we get a non "picture ready" (not sure what we'll get)
				auto DecodeResult = H264SwDecNextPicture( mDecoderInstance, &Picture, EndOfStream );
				IsOkay( Result, "H264SwDecNextPicture" );
				if ( DecodeResult != H264SWDEC_PIC_RDY )
				{
					//	OK just means it's finished
					if ( DecodeResult != H264SWDEC_OK )
						std::Debug << "H264SwDecNextPicture result: " << GetDecodeResultString(DecodeResult) << std::endl;
					break;
				}
				/*
				 picNumber++;
				printf("PIC %d, type %s, concealed %d\n", picNumber,
					   decPicture.isIdrPicture ? "IDR" : "NON-IDR",
					   decPicture.nbrOfErrMBs);
				*/
				//	YuvToRgb( decPicture.pOutputPicture, pRgbPicture );
				OnPicture( Picture, Meta, OnFrameDecoded );
			}
			return true;
		}
		
		default:
		{
			std::Debug << "Unhandled H264SwDecDecode result: " << GetDecodeResultString(Result) << ". Bytes processed: "  << BytesProcessed << "/" << Input.dataLen << std::endl;
			return true;
		}
	}
		
}



void Broadway::TDecoder::Decode(ArrayBridge<uint8_t>&& PacketData,std::function<void(const SoyPixelsImpl&)> OnFrameDecoded)
{
	mPendingData.PushBackArray(PacketData);
	
	while ( true )
	{
		//	keep decoding until no more data to process
		if ( !DecodeNextPacket( OnFrameDecoded ) )
			break;
	}
}

void Broadway::TDecoder::OnMeta(const H264SwDecInfo& Meta)
{
	
}

void Broadway::TDecoder::OnPicture(const H264SwDecPicture& Picture,const H264SwDecInfo& Meta,std::function<void(const SoyPixelsImpl&)> OnFrameDecoded)
{
	//		headers just say
	//	u32 *pOutputPicture;    /* Pointer to the picture, YUV format       */
	auto Format = SoyPixelsFormat::Yuv_8_88_Ntsc;
	SoyPixelsMeta PixelMeta( Meta.picWidth, Meta.picHeight, Format );
	std::Debug << "Decoded picture " << PixelMeta << std::endl;
		
	//	gr: wish we knew exactly how many bytes Picture.pOutputPicture pointed at!
	auto DataSize = PixelMeta.GetDataSize();
	
	auto* Pixels8 = reinterpret_cast<uint8_t*>(Picture.pOutputPicture);
	SoyPixelsRemote Pixels( Pixels8, DataSize, PixelMeta );
	OnFrameDecoded( Pixels );
}


	/*
		
	 
		
		case H264SWDEC_STRM_PROCESSED:
		case H264SWDEC_STRM_ERR:
		case H264SWDEC_PARAM_ERR:
		decoder[i]->decInput.dataLen = 0;
		break;
		
		default:
		DEBUG(("Decoder[%d] FATAL ERROR\n", i));
		exit(10);
		break;
*/



/*
typedef struct
{
	H264SwDecInst decInst;
	H264SwDecInput decInput;
	H264SwDecOutput decOutput;
	H264SwDecPicture decPicture;
	H264SwDecInfo decInfo;
	FILE *foutput;
	char outFileName[256];
	u8 *byteStrmStart;
	u32 picNumber;
} Decoder;



ret = H264SwDecInit(&(decoder[i]->decInst), disableOutputReordering);

if (ret != H264SWDEC_OK)
{
	DEBUG(("Init failed %d\n", ret));
	exit(100);
}

decoder[i]->decInput.pStream = decoder[i]->byteStrmStart;
decoder[i]->decInput.dataLen = strmLen;
decoder[i]->decInput.intraConcealmentMethod = 0;

}

// main decoding loop
do
{
	// decode once using each instance
	for (i = 0; i < instCount; i++)
	{
		ret = H264SwDecDecode(decoder[i]->decInst,
							  &(decoder[i]->decInput),
							  &(decoder[i]->decOutput));
		
		switch(ret)
		{
				
			case H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY:
				
				ret = H264SwDecGetInfo(decoder[i]->decInst,
									   &(decoder[i]->decInfo));
				if (ret != H264SWDEC_OK)
					exit(1);
					
					if (cropDisplay && decoder[i]->decInfo.croppingFlag)
					{
						DEBUG(("Decoder[%d] Cropping params: (%d, %d) %dx%d\n",
							   i,
							   decoder[i]->decInfo.cropParams.cropLeftOffset,
							   decoder[i]->decInfo.cropParams.cropTopOffset,
							   decoder[i]->decInfo.cropParams.cropOutWidth,
							   decoder[i]->decInfo.cropParams.cropOutHeight));
					}
				
				DEBUG(("Decoder[%d] Width %d Height %d\n", i,
					   decoder[i]->decInfo.picWidth,
					   decoder[i]->decInfo.picHeight));
				
				DEBUG(("Decoder[%d] videoRange %d, matricCoefficients %d\n",
					   i, decoder[i]->decInfo.videoRange,
					   decoder[i]->decInfo.matrixCoefficients));
				decoder[i]->decInput.dataLen -=
				(u32)(decoder[i]->decOutput.pStrmCurrPos -
					  decoder[i]->decInput.pStream);
				decoder[i]->decInput.pStream =
				decoder[i]->decOutput.pStrmCurrPos;
				break;
				
			case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:
				decoder[i]->decInput.dataLen -=
				(u32)(decoder[i]->decOutput.pStrmCurrPos -
					  decoder[i]->decInput.pStream);
				decoder[i]->decInput.pStream =
				decoder[i]->decOutput.pStrmCurrPos;
				///fall through
			case H264SWDEC_PIC_RDY:
				if (ret == H264SWDEC_PIC_RDY)
					decoder[i]->decInput.dataLen = 0;
					
					ret = H264SwDecGetInfo(decoder[i]->decInst,
										   &(decoder[i]->decInfo));
					if (ret != H264SWDEC_OK)
						exit(1);
						
						while (H264SwDecNextPicture(decoder[i]->decInst,
													&(decoder[i]->decPicture), 0) == H264SWDEC_PIC_RDY)
						{
							decoder[i]->picNumber++;
							
							numErrors += decoder[i]->decPicture.nbrOfErrMBs;
							
							DEBUG(("Decoder[%d] PIC %d, type %s, concealed %d\n",
								   i, decoder[i]->picNumber,
								   decoder[i]->decPicture.isIdrPicture
								   ? "IDR" : "NON-IDR",
								   decoder[i]->decPicture.nbrOfErrMBs));
							fflush(stdout);
							
							CropWriteOutput(decoder[i]->foutput,
											(u8*)decoder[i]->decPicture.pOutputPicture,
											cropDisplay, &(decoder[i]->decInfo));
						}
				
				if (maxNumPics && decoder[i]->picNumber == maxNumPics)
					decoder[i]->decInput.dataLen = 0;
					break;
				
			case H264SWDEC_STRM_PROCESSED:
			case H264SWDEC_STRM_ERR:
			case H264SWDEC_PARAM_ERR:
				decoder[i]->decInput.dataLen = 0;
				break;
				
			default:
				DEBUG(("Decoder[%d] FATAL ERROR\n", i));
				exit(10);
				break;
				
		}
	}
	
	// check if any of the instances is still running (=has more data) 	instRunning = instCount;
	for (i = 0; i < instCount; i++)
	{
		if (decoder[i]->decInput.dataLen == 0)
			instRunning--;
	}
	
} while (instRunning);


/// get last frames and close each instance
for (i = 0; i < instCount; i++)
{
	while (H264SwDecNextPicture(decoder[i]->decInst,
								&(decoder[i]->decPicture), 1) == H264SWDEC_PIC_RDY)
	{
		decoder[i]->picNumber++;
		
		DEBUG(("Decoder[%d] PIC %d, type %s, concealed %d\n",
			   i, decoder[i]->picNumber,
			   decoder[i]->decPicture.isIdrPicture
			   ? "IDR" : "NON-IDR",
			   decoder[i]->decPicture.nbrOfErrMBs));
		fflush(stdout);
		
		CropWriteOutput(decoder[i]->foutput,
						(u8*)decoder[i]->decPicture.pOutputPicture,
						cropDisplay, &(decoder[i]->decInfo));
	}
	*/
