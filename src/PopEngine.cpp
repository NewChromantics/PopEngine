#include "PopEngine.h"
#include "SoyDebug.h"
//#include "SoyApp.h"
#include "PopMain.h"
#include "SoyFilesystem.h"
#include "TBind.h"


namespace Platform
{
	void		Loop(bool Blocking,std::function<void()> OnQuit);
}

//	need a global to stop auto destruction
#if !defined(TARGET_WINDOWS)
std::shared_ptr<Bind::TInstance> pInstance;
#endif

TPopAppError::Type PopMain()
{
	//	need to resolve .. paths early in windows
	auto DataPath = Pop::ProjectPath;
	try
	{
		DataPath = Platform::GetFullPathFromFilename(DataPath);
	}
	catch (std::exception& e)
	{
		//std::Debug << "Argument path doesn't exist; " << e.what() << std::endl;
	}

	//	 try to predict full paths vs something embedded in the app
	if ( !Platform::IsFullPath(DataPath) )
	{
		//	todo: check CWD first, then resources
		std::Debug << "<" << DataPath << "> is not full path, prefixing with resources path." << std::endl;
		DataPath = Platform::GetAppResourcesDirectory() + DataPath;
	}
	
	//	in case the datapath is a filename, strip back to dir
	DataPath = Platform::GetDirectoryFromFilename(DataPath,true);
	
	
#if defined(TARGET_WINDOWS)
	//	on windows, we pump the win32 thread
	//	gr: do we NEED to do that on the main thread? I believe every window
	//		now has it's own message queue/thread so this one will never get a 
	//		message aside from PostQuitMessage()
	std::shared_ptr<Bind::TInstance> pInstance;
	bool Running = true;
#elif defined(TARGET_LINUX)
	//	on linux, the main thread has nothing to do
	std::shared_ptr<Bind::TInstance> pInstance;
	Soy::TSemaphore RunningLock;
#endif
	
	auto OnShutdown = [&](int32_t ExitCode)
	{		
	#if defined(TARGET_WINDOWS)
		Running = false;
		//	make sure WM_QUIT comes up by waking the message loop
		PostQuitMessage(ExitCode);
	#elif defined(TARGET_LINUX)
		//	todo: save exit code!
		RunningLock.OnCompleted();
	#endif
	};

	{
		//	run an instance
		std::string BootupFilename = "bootup.js";
		pInstance.reset(new Bind::TInstance(DataPath, BootupFilename, OnShutdown));

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
	#elif defined(TARGET_LINUX)
		//	wait for shutdown
		RunningLock.Wait();
	#endif
	}
	
	return TPopAppError::Success;
}

