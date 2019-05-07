#pragma once
#include "TBind.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace ApiEzsift
{
	void	Bind(Bind::TContext& Context);
}

namespace Ezsift
{
	class TInstance;
}


extern const char Ezsift_TypeName[];
class TEzsiftWrapper : public Bind::TObjectWrapper<Ezsift_TypeName,Ezsift::TInstance>
{
public:
	TEzsiftWrapper(Bind::TLocalContext& Context,Bind::TObject& This) :
		TObjectWrapper		( Context, This )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);

	virtual void 		Construct(Bind::TCallback& Arguments) override;

	static void			GetFeatures(Bind::TCallback& Arguments);

	
protected:
	std::shared_ptr<Ezsift::TInstance>&		mEzsift = mObject;
};


