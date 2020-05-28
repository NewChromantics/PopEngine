#include "TApiAudio.h"
#include "SoySound.h"


namespace ApiAudio
{
	const char Namespace[] = "Pop.Audio";
	DEFINE_BIND_TYPENAME(Sound);
	
	DEFINE_BIND_FUNCTIONNAME(Play);
}



void ApiAudio::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindObjectType<TSoundWrapper>( Namespace );
}


void ApiAudio::TSoundWrapper::Construct(Bind::TCallback& Params)
{
	Array<uint8_t> WaveData;
	Params.GetArgumentArray(0,GetArrayBridge(WaveData));
	mSound.reset( new Platform::TSound( GetArrayBridge(WaveData) ) );
}

void ApiAudio::TSoundWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::Play>( &TSoundWrapper::Play );
}

void ApiAudio::TSoundWrapper::Play(Bind::TCallback& Params)
{
	auto TimeMs = Params.GetArgumentInt(0);
	mSound->Play(TimeMs);
}
