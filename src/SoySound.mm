#include "SoySound.h"
#include "SoyTypes.h"
#include <AppKit/AppKit.h>


namespace Platform
{
	void	NSDataToArray(NSData* Data,ArrayBridge<uint8>&& Array);
	NSData*	ArrayToNSData(const ArrayBridge<uint8>& Array);
}


class Platform::TSoundImpl
{
public:
	ObjcPtr<NSSound>	mSound;
};


Platform::TSound::TSound(const ArrayBridge<uint8_t>&& Data)
{
	mSound.reset( new TSoundImpl );
	
	auto NsData = Platform::ArrayToNSData(Data);
	mSound->mSound.mObject = [[NSSound alloc] initWithData:NsData];
}


Platform::TSound::~TSound()
{
	mSound.reset();
}

void Platform::TSound::Play(uint64_t TimeMs)
{
	auto* Sound = mSound->mSound.mObject;

	NSTimeInterval TimeSecs = TimeMs / 1000.0;
	[Sound setCurrentTime:TimeSecs];

	auto Success = [Sound play];
	
	//	returns false if already playing or error
	if ( !Success )
	{
		if ( ![Sound isPlaying] )
			throw Soy::AssertException("Error playing sound");
	}
}

