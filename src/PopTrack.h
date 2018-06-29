#pragma once
#include <SoyApp.h>

class TV8Instance;


class TPopTrack
{
public:
	TPopTrack(const std::string& BootupFilename);
	~TPopTrack();
	
private:
	std::shared_ptr<TV8Instance>	mV8Instance;
};


