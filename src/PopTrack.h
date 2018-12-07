#pragma once
#include "SoyApp.h"

class TV8Instance;
namespace JsCore
{
	class TInstance;
}

class TPopTrack
{
public:
	TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename);
	~TPopTrack();
	
private:
	std::shared_ptr<JsCore::TInstance>	mJsCoreInstance;
	std::shared_ptr<TV8Instance>		mV8Instance;
};


