#pragma once

#include "SoyWindow.h"
#include "SoyGui.h"


namespace Platform
{
	class TWindow;
}


//	note: we want to re-use stuff that's in MacOpenglView too
@interface Platform_View: NSView
{
	@public std::function<NSDragOperation()>				mGetDragDropCursor;
	@public std::function<bool(ArrayBridge<std::string>&)>	mTryDragDrop;
	@public std::function<bool(ArrayBridge<std::string>&)>	mOnDragDrop;
}

-(void)RegisterForEvents;

//	overloaded funcs
- (BOOL) isFlipped;
/*
-(void)mouseMoved:(NSEvent *)event;
-(void)mouseDown:(NSEvent *)event;
-(void)mouseDragged:(NSEvent *)event;
-(void)mouseUp:(NSEvent *)event;
-(void)rightMouseDown:(NSEvent *)event;
-(void)rightMouseDragged:(NSEvent *)event;
-(void)rightMouseUp:(NSEvent *)event;
-(void)otherMouseDown:(NSEvent *)event;
-(void)otherMouseDragged:(NSEvent *)event;
-(void)otherMouseUp:(NSEvent *)event;

- (void)keyDown:(NSEvent *)event;
- (void)keyUp:(NSEvent *)event;
*/
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;


@end





class Platform::TWindow : public SoyWindow
{
public:
	TWindow(PopWorker::TJobQueue& Thread,const std::string& Name,const Soy::Rectx<int32_t>& Rect,bool Resizable,std::function<void()> OnAllocated=nullptr);
	TWindow(PopWorker::TJobQueue& Thread);
	~TWindow()
	{
		//	gr: this also isn't destroying the window
		[mWindow release];
	}

	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz,bool Vert) override;

	NSRect							GetChildRect(Soy::Rectx<int32_t> Rect);
	NSView*							GetContentView();
	void							OnChildAdded(const Soy::Rectx<int32_t>& ChildRect);
	
protected:
	//	run a safe job on the window (main) thread, queued! dont capture by reference
	void							QueueOnThread(std::function<void()> Exec);

public:
	PopWorker::TJobQueue&			mThread;	//	NS ui needs to be on the main thread
	NSWindow*						mWindow = nullptr;
	Platform_View*					mContentView = nullptr;	//	where controls go
	CVDisplayLinkRef				mDisplayLink = nullptr;
	
	//	Added from SoyGuiOsx
	NSWindow*						GetWindow();
	NSView*							GetChild(const std::string& Name);
	void							EnumChildren(std::function<bool(NSView*)> EnumChild);
};


