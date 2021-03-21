//
//  App.swift
//  PopEngine
//
//  Created by Graham Reeves on 20/03/2021.
//  Copyright © 2021 NewChromantics. All rights reserved.
//

import SwiftUI



@objc
class TestAppWindow : NSObject
{
	@objc var window: NSWindow!

	override init()
	{
	//func applicationDidFinishLaunching(_ aNotification: Notification) {
		// Create the SwiftUI view that provides the window contents.
		let contentView = TestView()

		// Create the window and set the content view.
		window = NSWindow(
		    contentRect: NSRect(x: 0, y: 0, width: 480, height: 300),
		    styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
		    backing: .buffered, defer: false)
		window.isReleasedWhenClosed = false
		window.center()
		window.setFrameAutosaveName("Main Window")
		window.contentView = NSHostingView(rootView: contentView)
		window.makeKeyAndOrderFront(nil)
	}
}





struct TestView: View {

	@State var renderView = PopEngineRenderView(name:"TestRenderView") 
   
   
    var body: some View {
        Text(/*@START_MENU_TOKEN@*/"Hello, World!"/*@END_MENU_TOKEN@*/)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color("SplashscreenBackground"))
        
        OpenglView(renderer:$renderView)
    }
    
}

struct App_Previews: PreviewProvider {
    static var previews: some View {
        TestView()
    }
}




/*
@objc
class PopEngineSwiftViewFactory : NSObject
{
	//	complicated! can't just return a View
	//	https://stackoverflow.com/a/65585090/355753
	//func CreateView(Name:String) -> View
	@ViewBuilder static func CreateView(Name:String) -> some View
	{
		if ( Name == "Test" )
		{
			TestView()
		}
		//throw "No view with this name"
		//return nil
	}
};
*/
/*
	//	gr: maybe should be using this to bind
    func makeCoordinator() -> Coordinator 
    {
        Coordinator(self)
    }

*/
/*
struct App: View {
    var body: some View {
        Text(/*@START_MENU_TOKEN@*/"Hello, World!"/*@END_MENU_TOKEN@*/)
    }
}

struct App_Previews: PreviewProvider {
    static var previews: some View {
        App()
    }
}
*/
