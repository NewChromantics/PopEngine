#include "PopEngine.h"
#include "SoyDebug.h"
//#include "SoyApp.h"
#include "PopMain.h"
#include "SoyFilesystem.h"
#include "TBind.h"


namespace Platform
{
	void		Loop(bool Blocking,std::function<void(int32_t)> OnQuit);
}

//	gr: need to move win32 stuff away from opengl (in SoyOpenglWindow.cpp) atm
#if defined(TARGET_UWP)
void Platform::Loop(bool Blocking, std::function<void(int32_t)> OnQuit)
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::minutes(1));
	}
}

#endif


//	start instance, non blocking
std::shared_ptr<Bind::TInstance> StartPopInstance(std::function<void(int32_t)> OnExitCode)
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
	
	//	run an instance
	std::string BootupFilename = "bootup.js";
	std::Debug << "PopMain() Creating instance, DataPath=" << DataPath << " BootupFilename=" << BootupFilename << std::endl;
	std::shared_ptr<Bind::TInstance> pInstance;
	pInstance.reset(new Bind::TInstance(DataPath, BootupFilename, OnExitCode));

	return pInstance;
}



#if defined(TARGET_WINDOWS)
//	blocking on windows with win32 message queue
int32_t PopMain()
{
	//	on windows, we pump the win32 thread
	//	gr: do we NEED to do that on the main thread? I believe every window
	//		now has it's own message queue/thread so this one will never get a 
	//		message aside from PostQuitMessage()
	bool Running = true;
	DWORD ThisWin32ThreadId = GetCurrentThreadId();
	int32_t PopExitCode = 999;
	auto OnShutdown = [&](int32_t ExitCode)
	{		
		std::Debug << "PopMain() OnShutdown exitcode=" << ExitCode << std::endl;
		//	make sure WM_QUIT comes up by waking the message loop
		//	this exit code will get set again from WM_QUIT in case WM_QUIT comes from elsewhere
		PopExitCode = ExitCode;
		PostQuitMessage(ExitCode);	//	this posts to THIS thread, if being called from the JSC thread, the win32 loop below wont trigger
		if (!PostThreadMessageA(ThisWin32ThreadId, WM_QUIT, ExitCode, 0))
		{
			std::Debug << "error PostThreadMessageA(thread=" << ThisWin32ThreadId << ", WM_QUIT) " << Platform::GetLastErrorString() << std::endl;
		}
	};
	try
	{
		auto pInstance = StartPopInstance(OnShutdown);

		//	gr: this thread should just spin, and not get win32 messages...
		std::Debug << "PopMain() run loop..." << std::endl;
		while (Running)
		{
			auto OnQuit = [&](int32_t ExitCode)
			{
				PopExitCode = ExitCode;
				Running = false;
			};
			auto Blocking = true;
			Platform::Loop(Blocking, OnQuit);

			//	don't free this immediately in OnShutdown, do it here off the thread that triggered
			if (!Running)
				pInstance.reset();
		}
		std::Debug << "PopMain() run loop exited." << std::endl;
	}
	catch (std::exception & e)
	{
		std::Debug << "Instance startup exception: " << e.what() << std::endl;
		OnShutdown(999);
	}
	
	return PopExitCode;
}
#endif
	
#if defined(TARGET_LINUX)
//	blocking on linux with lock
int32_t PopMain()
{
	//	on linux, the main thread has nothing to do so we lock
	Soy::TSemaphore RunningLock;

	int32_t PopExitCode = 707;
	auto OnShutdown = [&](int32_t ExitCode)
	{		
		PopExitCode = ExitCode;
		RunningLock.OnCompleted();
	};
	auto pInstance = StartPopInstance(OnShutdown);
	
	std::Debug << "PopMain() waiting for RunningLock..." << std::endl;
	RunningLock.Wait();
	std::Debug << "PopMain() RunningLock flagged. Exiting." << std::endl;

	try
	{
		std::Debug << "PopMain() resetting instance...." << std::endl;
		pInstance.reset();
		std::Debug << "PopMain() resetting instance done." << std::endl;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception ending instance: " << e.what() << std::endl;
	}
	return PopExitCode;
}
#endif


/*
int32_t PopMain()
{
#elif defined(TARGET_LINUX)
	//	on linux, the main thread has nothing to do
	std::shared_ptr<Bind::TInstance> pInstance;
	Soy::TSemaphore RunningLock;
#endif
	
	int32_t PopExitCode = 707;
	auto OnShutdown = [&](int32_t ExitCode)
	{		
		std::Debug << "PopMain() OnShutdown exitcode=" << ExitCode << std::endl;
#if defined(TARGET_UWP)
		Running = false;
#elif defined(TARGET_WINDOWS)
		//	make sure WM_QUIT comes up by waking the message loop
		//	this exit code will get set again from WM_QUIT in case WM_QUIT comes from elsewhere
		PopExitCode = ExitCode;
		PostQuitMessage(ExitCode);	//	this posts to THIS thread, if being called from the JSC thread, the win32 loop below wont trigger
		if (!PostThreadMessageA(ThisWin32ThreadId, WM_QUIT, ExitCode, 0))
		{
			std::Debug << "error PostThreadMessageA(thread=" << ThisWin32ThreadId << ", WM_QUIT) " << Platform::GetLastErrorString() << std::endl;
		}
#elif defined(TARGET_LINUX)
		//	todo: save exit code!
		RunningLock.OnCompleted();
#endif
	};

	{
		//	run an instance
		std::string BootupFilename = "bootup.js";
		std::Debug << "PopMain() Creating instance, DataPath=" << DataPath << " BootupFilename=" << BootupFilename << std::endl;
		pInstance.reset(new Bind::TInstance(DataPath, BootupFilename, OnShutdown));

	#if !defined(TARGET_OSX_BUNDLE)
		//	run
		//Soy::Platform::TConsoleApp app;
		//app.WaitForExit();
	#endif

	#if defined(TARGET_WINDOWS)
		//	gr: this thread should just spin, and not get win32 messages...
		std::Debug << "PopMain() run loop..." << std::endl;
		while ( Running )
		{
			auto OnQuit = [&](int32_t ExitCode)
			{
				PopExitCode = ExitCode;
				Running = false;
			};
			auto Blocking = true;
			Platform::Loop( Blocking, OnQuit );

			//	don't free this immediately in OnShutdown, do it here off the thread that triggered
			if ( !Running )
				pInstance.reset();
		}
		std::Debug << "PopMain() run loop exited." << std::endl;
	#elif defined(TARGET_LINUX)
		//	wait for shutdown
		std::Debug << "PopMain() waiting for RunningLock..." << std::endl;
		RunningLock.Wait();
		std::Debug << "PopMain() RunningLock flagged. Exiting." << std::endl;
	#endif
	}
	
	return PopExitCode;
}
*/

namespace Java
{
	void	FlushThreadLocals()	{}
}


