#include "PopMain.h"
#include "SoyFilesystem.h"

#if defined(TARGET_OSX)
#import <Cocoa/Cocoa.h>
//#import <SpriteKit/SpriteKit.h>
#endif

#if defined(TARGET_IOS)
#import "UIKit/UIApplication.h"
#endif


#if defined(TARGET_OSX_BUNDLE)||defined(TARGET_IOS)
bool Soy::Platform::BundleInitialised = false;
std::shared_ptr<PopMainThread> Soy::Platform::gMainThread;
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

#if defined(TARGET_IOS)
@property (strong, nonatomic) UIWindow *window;
#endif

@end


/*
@implementation SKScene (Unarchive)

+ (instancetype)unarchiveFromFile:(NSString *)file {
	// Retrieve scene file path from the application bundle
	NSString *nodePath = [[NSBundle mainBundle] pathForResource:file ofType:@"sks"];
	// Unarchive the file to an SKScene object
	NSData *data = [NSData dataWithContentsOfFile:nodePath
										  options:NSDataReadingMappedIfSafe
											error:nil];
	NSKeyedUnarchiver *arch = [[NSKeyedUnarchiver alloc] initForReadingWithData:data];
	[arch setClass:self forClassName:@"SKScene"];
	SKScene *scene = [arch decodeObjectForKey:NSKeyedArchiveRootObjectKey];
	[arch finishDecoding];
	
	return scene;
}

@end
*/
@implementation AppDelegate

//@synthesize window = _window;


	
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	//GameScene *scene = [GameScene unarchiveFromFile:@"GameScene"];
 
	PopMain();

	// Set the scale mode to scale to fit the window
	//scene.scaleMode = SKSceneScaleModeAspectFit;
	
	//[self.skView presentScene:scene];
	
	// Sprite Kit applies additional optimizations to improve rendering performance
//	self.skView.ignoresSiblingOrder = YES;
	
//	self.skView.showsFPS = YES;
//	self.skView.showsNodeCount = YES;
}

#if defined(TARGET_OSX)
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	//	gr: make this a no and send the on-quit event
	return YES;
}
#endif


#if defined(TARGET_IOS)
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	// Override point for customization after application launch.
	PopMain();
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
