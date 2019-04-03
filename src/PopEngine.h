#pragma once
#include "SoyApp.h"

class TV8Instance;
namespace Bind
{
	class TInstance;
}

class TPopTrack
{
public:
	TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename);
	~TPopTrack();
	
private:
	std::shared_ptr<Bind::TInstance>	mApiInstance;
};


