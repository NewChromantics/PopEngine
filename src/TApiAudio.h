#pragma once
#include "TBind.h"

namespace Platform
{
	//	basic sound player
	class TSound;
}


namespace ApiAudio
{
	extern const char	Namespace[];
	void	Bind(Bind::TContext& Context);
	
	class TSoundWrapper;

	DECLARE_BIND_TYPENAME(Sound);
}



class ApiAudio::TSoundWrapper : public Bind::TObjectWrapper<BindType::Sound,Platform::TSound>
{
public:
	TSoundWrapper(Bind::TContext& Context) :
		TObjectWrapper			( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);

	virtual void 		Construct(Bind::TCallback& Params) override;
	void				Seek(Bind::TCallback& Params);
	void				Play(Bind::TCallback& Params);

protected:
	std::shared_ptr<Platform::TSound>&	mSound = mObject;
};

