#include "PopMain.h"
#import <Cocoa/Cocoa.h>

std::shared_ptr<TJobParams> gParams;

#if defined(TARGET_OSX_BUNDLE)
bool Soy::Platform::BundleInitialised = false;
std::shared_ptr<PopMainThread> Soy::Platform::gMainThread;
#endif

#if defined(TARGET_OSX_BUNDLE)
int Soy::Platform::BundleAppMain(int argc, const char * argv[])
{
	/*
	auto Params = ::Private::DecodeArgs( argc, argv );
	
	//	save params for pop main
	gParams.reset( new TJobParams(Params) );
	*/
	Soy::Platform::BundleInitialised = true;
	gMainThread.reset( new PopMainThread );
	
	//	create runloop and delegate
	return NSApplicationMain(argc, argv);
}
#endif



PopMainThread::PopMainThread()
{
	auto OnJobPushed = [this](std::shared_ptr<PopWorker::TJob>&)
	{
		TriggerIteration();
	};
	mOnJobPushed.AddListener( OnJobPushed );
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



#import <Cocoa/Cocoa.h>
#import <SpriteKit/SpriteKit.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

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

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	//	gr: make this a no and send the on-quit event
	return YES;
}

@end
