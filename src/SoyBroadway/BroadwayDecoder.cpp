#include "BroadwayDecoder.h"
#include <sstream>
#include "SoyDebug.h"


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
	
	H264SwDecRelease(decoder[i]->decInst);
	
	if (decoder[i]->foutput)
		fclose(decoder[i]->foutput);
		
		free(decoder[i]->byteStrmStart);
		
		free(decoder[i]);
		}

free(decoder);

if (numErrors)
return 1;
*/
