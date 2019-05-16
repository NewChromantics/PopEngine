#include "PopEngine.h"
#include "SoyDebug.h"
#include "SoyApp.h"
#include "PopMain.h"
#include "SoyFileSystem.h"
#include "TBind.h"


namespace Platform
{
	void		Loop(bool Blocking,std::function<void()> OnQuit);
}

#if !defined(TARGET_WINDOWS)
std::shared_ptr<Bind::TInstance> pInstance;
#endif

TPopAppError::Type PopMain(ArrayBridge<std::string>& Arguments)
{
	std::string DataPath;

	//	use arg0 if provided
	//	otherwise, we assume there's a bootup.js in the app's resources path
	if ( Arguments.GetSize() > 0 )
	{
		DataPath = Arguments[0];
	}
	else
	{
		//	gr: need a better default here
		DataPath = "";
	}

	//	 try to predict full paths vs something embedded in the app
	if ( !Platform::IsFullPath(DataPath) )
	{
		std::Debug << "<" << DataPath << "> is not full path, prefixing with resources path." << std::endl;
		DataPath = Platform::GetAppResourcesDirectory() + DataPath;
	}
	
	DataPath += "/";

	bool Running = true;
	
#if defined(TARGET_WINDOWS)
	std::shared_ptr<Bind::TInstance> pInstance;
#endif
	
	auto OnShutdown = [&](int32_t ExitCode)
	{
		Running = false;
		
	#if defined(TARGET_WINDOWS)
		//	make sure WM_QUIT comes up
		PostQuitMessage(ExitCode);
	#endif
	};

	{
		Array<std::string> ExeArguments;
		if ( Arguments.GetSize() > 1 )
		{
			auto RestOfArgs = Arguments.GetSubArray( 1, Arguments.GetSize()-1 );
			ExeArguments.Copy( RestOfArgs );
		}
		
		//	run an instance
		std::string BootupFilename = "bootup.js";
		pInstance.reset(new Bind::TInstance(DataPath, GetArrayBridge(ExeArguments), BootupFilename, OnShutdown));

	#if !defined(TARGET_OSX_BUNDLE)
		//	run
		//Soy::Platform::TConsoleApp app;
		//app.WaitForExit();
	#endif

	#if defined(TARGET_WINDOWS)
		//	gr: this thread should just spin, and not get win32 messages...
		while ( Running )
		{
			auto OnQuit = [&]()
			{
				OnShutdown(0);
			};
			auto Blocking = true;
			Platform::Loop( Blocking, OnQuit );

			//	don't free this immediately in OnShutdown, do it here off the thread that triggered
			if ( !Running )
				pInstance.reset();
		}
	#endif
	}
	
	return TPopAppError::Success;
}

