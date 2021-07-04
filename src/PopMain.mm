#include "PopMain.h"
#include "SoyFilesystem.h"

#if defined(TARGET_OSX)
#import <Cocoa/Cocoa.h>
//#import <SpriteKit/SpriteKit.h>
#endif

#if defined(TARGET_IOS)
#import "UIKit/UIApplication.h"
#endif

void StartupInstance();

//	abstract class to signal when doing critical things, to prevent app nap on osx
//	but may have other-platform equivilents, so make it RAII
namespace Platform
{
	class TCriticalSection;
}

#if defined(TARGET_OSX)
class Platform::TCriticalSection
{
public:
	TCriticalSection(const std::string& Reason);
	~TCriticalSection();
	
	ObjcPtr<NSObject>		mActivity;
};
#endif

#if defined(TARGET_OSX)
Platform::TCriticalSection::TCriticalSection(const std::string& Reason)
{
	//	NSActivityBackground means has stuff to do whilst not relying on user input
	//	NSActivityUserInitiated	we're doing a section in response to user
	//	NSActivityUserInitiatedAllowingIdleSystemSleep
	//	NSActivityLatencyCritical	I/O and thread access is critical
	NSActivityOptions Options = NSActivityBackground;
	auto ReasonNs = Soy::StringToNSString(Reason);
	auto* Activity = [[NSProcessInfo processInfo] beginActivityWithOptions:Options reason:ReasonNs];
	mActivity.Retain(Activity);
}
#endif

#if defined(TARGET_OSX)
Platform::TCriticalSection::~TCriticalSection()
{
	[[NSProcessInfo processInfo] endActivity:mActivity];
}
#endif



#if defined(TARGET_OSX_BUNDLE)||defined(TARGET_IOS)
bool Soy::Platform::BundleInitialised = false;
std::shared_ptr<PopMainThread> Soy::Platform::gMainThread;
std::shared_ptr<Platform::TCriticalSection>	gCriticalSection;
#endif

#if defined(TARGET_OSX_BUNDLE)||defined(TARGET_IOS)
int Soy::Platform::BundleAppMain()
{
	Soy::Platform::BundleInitialised = true;
	gMainThread.reset( new PopMainThread );
	
	//	create runloop and delegate
	//	gr: these args are irrelevent it seems, we get proper args from the delegate
	int argc = 1;
	
#if defined(TARGET_IOS)
	// If nil is specified for principalClassName, the value for NSPrincipalClass from the Info.plist is used. If there is no
	// NSPrincipalClass key specified, the UIApplication class is used. The delegate class will be instantiated using init.
//	UIKIT_EXTERN int UIApplicationMain(int argc, char * _Nullable argv[_Nonnull], NSString * _Nullable principalClassName, NSString * _Nullable delegateClassName);
	//const char *[1]' to 'char * _Nullable *
	char* argv[] = {"FakeExe"};

	//	when using storyboards, we need to set an app delegate class;
	//	error:
	//	There is no app delegate set. An app delegate class must be specified to use a main storyboard file.
	//	https://stackoverflow.com/a/17278607/355753
	auto AppDelegateClass = @"AppDelegate";
	return UIApplicationMain(argc, argv, nullptr, AppDelegateClass );
#else
	//	https://developer.apple.com/documentation/appkit/1428499-nsapplicationmain
	//	NSApplicationMain itself ignores the argc and argv arguments. 
	//	Return Value
	//	This method never returns a result code. Instead, it calls the exit function to exit the application and terminate the process.
	const char* argv[] = {"FakeExe"};
	return NSApplicationMain(argc, argv);
#endif
}
#endif



PopMainThread::PopMainThread()
{
	auto OnJobPushed = [this](std::shared_ptr<PopWorker::TJob>&)
	{
		TriggerIteration();
	};
	mOnJobPushed = OnJobPushed;
}

void PopMainThread::TriggerIteration()
{
	//	to avoid deadlock when waiting for a job, flush if we've just queued a job on our own thread
	//	gr: should this only be triggered for a blocking job?
	if ( [[NSThread currentThread] isEqual:[NSThread mainThread]] )
	{
		Flush(*this);
		return;
	}
	
	dispatch_async( dispatch_get_main_queue(), ^(void){
		Flush(*this);
	});
}



#if defined(TARGET_OSX)
@interface AppDelegate : NSObject <NSApplicationDelegate>
#elif defined(TARGET_IOS)
//@interface AppDelegate : NSObject <UIApplicationDelegate>
@interface AppDelegate : UIResponder <UIApplicationDelegate>
#endif

//@property (assign) IBOutlet NSWindow *window;
//@property (assign) IBOutlet SKView *skView;
#if defined(TARGET_OSX)
@property (strong, nonatomic) id<NSObject>	mNoNapActivity;
#endif

#if defined(TARGET_IOS)
@property (strong, nonatomic) UIWindow *window;
#endif

@end

@implementation AppDelegate

//@synthesize window = _window;


//	gr: tidy this up!
namespace Bind
{
	class TInstance;
}
std::shared_ptr<Bind::TInstance> StartPopInstance(std::function<void(int32_t)> OnExitCode);
std::shared_ptr<Bind::TInstance> gPopInstance;
int32_t Platform_ExitCode = 666;


#if defined(TARGET_OSX)
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification 
#elif defined(TARGET_IOS)
- (void)applicationDidFinishLaunching:(UIApplication *)application 
#endif
{
	//	gr: from mavericks, osx naps our app, and recv() threads don't do anything until app is foreground
	//		maybe we can instance a critical section in recv/accept threads
	//		but for now, just do it here
	#if defined(TARGET_OSX)
	gCriticalSection.reset( new Platform::TCriticalSection("Stop sockets sleeping via app nap") );
	#endif

	StartupInstance();
}

#if defined(TARGET_OSX)
- (void)applicationWillTerminate:(NSNotification *)notification
{
	#if defined(TARGET_OSX)
	gCriticalSection.reset();
	#endif
	
	//	need to manualyl clean up these globals (specifically MainThread)
	//	not sure if it's being double deleted, or just still in use
	Soy::Platform::gMainThread.reset();
	//	this also crashes at exit() without being cleaned up
	//	todo: call soy/pop dll exit global func
	gPopInstance.reset();
	//	only function that has an exit code!
	exit(Platform_ExitCode);
}
#endif

#if defined(TARGET_OSX)
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	//	gr: make this a no and send the on-quit event
	return YES;
}
#endif




#if defined(TARGET_IOS)
- (void)applicationWillResignActive:(UIApplication *)application {
	// Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	// Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}
#endif


#if defined(TARGET_IOS)
- (void)applicationDidEnterBackground:(UIApplication *)application {
	// Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
	// If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}
#endif


#if defined(TARGET_IOS)
- (void)applicationWillEnterForeground:(UIApplication *)application {
	// Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
}
#endif


#if defined(TARGET_IOS)
- (void)applicationDidBecomeActive:(UIApplication *)application {
	// Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}
#endif


#if defined(TARGET_IOS)
- (void)applicationWillTerminate:(UIApplication *)application {
	// Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}
#endif

@end



void StartupInstance()
{
	if ( !Soy::Platform::gMainThread )
		Soy::Platform::gMainThread.reset( new PopMainThread );
	
	//	gr: we can't block here, but we need to capture exit code.
	//		NSApplications never return, they have to be aborted with an exit code.
	auto OnExitCode = [](int32_t ExitCode)
	{
		std::Debug << "OnExitCode(" << ExitCode << ")" << std::endl;
		Platform_ExitCode = ExitCode;
		
		//	can't cleanup here, from inside own thread.
		//	need to find a way to exit before c++ tear down
		//gPopInstance.reset();

		//	terminate is the only way to get a callback (applicationWillTerminate) func AND exit the main thread
#if defined(TARGET_OSX)
		[[NSApplication sharedApplication] terminate:nil];
#else
		[[UIApplication sharedApplication] terminateWithSuccess];
#endif
		//	gr: when not on main thread, this DOES happen
		//		on main thread, it stops at terminate
		//std::Debug << "terminate done" << std::endl;
	};
	
	gPopInstance = StartPopInstance(OnExitCode);
}

//	alternative async startup entry which doesn't do anything with the UI delegates, so it can
//	just be started with swift
int PopMain()
{
	StartupInstance();
	return 0;
}
