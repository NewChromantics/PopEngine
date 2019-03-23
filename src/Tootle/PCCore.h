/*------------------------------------------------------
	PC Core include header

-------------------------------------------------------*/
#pragma once

#include "../TLTypes.h"




//	forward declarations
class TString;
class TBinaryTree;


namespace TLTime
{
	class TTimestamp;

	namespace Platform
	{
		SyncBool			Init();				//	time init
	}
}

namespace TLCore
{
	namespace Platform
	{
		SyncBool			Init();				//	platform init
		SyncBool			Update();			//	platform update
		SyncBool			Shutdown();			//	platform shutdown

		void				DoQuit();			// Notification of app quit
		
		void				QueryHardwareInformation(TBinaryTree& Data);
		void				QueryLanguageInformation(TBinaryTree& Data);

	}
};


