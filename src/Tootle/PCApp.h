/*------------------------------------------------------

	

-------------------------------------------------------*/
#pragma once
#include "PCGui.h"
#include "../TApp.h"




namespace TLGui
{
	namespace Platform
	{
		class App;
	}
}

class TLGui::Platform::App : public TLGui::TApp
{
public:
	App()		{}
	
	Bool			Init();
	SyncBool		Update();
	SyncBool		Shutdown();
};


