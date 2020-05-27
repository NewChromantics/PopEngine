#pragma once

#include <string>
#include "Array.hpp"


namespace Platform
{
	//	basic sound player
	class TSound;
	
	//	platform specific implemention/data
	class TSoundImpl;
}


class Platform::TSound
{
public:
	TSound(const ArrayBridge<uint8_t>&& Data);
	~TSound();
	
	void		Play(uint64_t TimeMs);
	
private:
	std::shared_ptr<TSoundImpl>	mSound;
};


