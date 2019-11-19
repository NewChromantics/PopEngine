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
	return UIApplicationMain(argc, argv, nullptr, nullptr );
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
@interface AppDelegate : NSObject <UIApplicationDelegate>
#endif

//@property (assign) IBOutlet NSWindow *window;
//@property (assign) IBOutlet SKView *skView;

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


void NSArray_String_ForEach(NSArray<NSString*>* Array,std::function<void(std::string&)> Enum)
{
	auto Size = [Array count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto Element = [Array objectAtIndex:i];
		//auto StringNs = [Element stringValue];
		auto String = Soy::NSStringToString(Element);
		Enum( String );
	}
}
	
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	//GameScene *scene = [GameScene unarchiveFromFile:@"GameScene"];
 
	Array<std::string> Arguments;
	auto PushArgument = [&](std::string& Argument)
	{
		Arguments.PushBack( Argument );
	};
	NSArray<NSString*>* ArgumentsNs = [[NSProcessInfo processInfo] arguments];
	NSArray_String_ForEach( ArgumentsNs, PushArgument );

	//	remove exe
	::Platform::SetExePath( Arguments[0] );
	Arguments.RemoveBlock(0,1);

	auto ArgumentsBridge = GetArrayBridge(Arguments);
	PopMain(ArgumentsBridge);

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

@end
