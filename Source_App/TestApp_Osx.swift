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
		window.setFrameAutosaveName("TestAppWindow")
		window.contentView = NSHostingView(rootView: contentView)
		window.makeKeyAndOrderFront(nil)
	}
}



struct TestView: View {

	@State var renderView = PopEngineRenderView(name:"TestRenderView")
	
	@ObservedObject var TestStringList = PopList(name:"TestStringList")

	var body: some View 
	{
		Text("Hello, World!")
			.frame(maxWidth: .infinity, maxHeight: .infinity)
			.background(Color("SplashscreenBackground"))

		List 
		{
			//ForEach(TestStringList.theValue.identified(by: \.self))
			ForEach(TestStringList.theValue) 
			{	
				Item in
				Text(Item.value)
			}
		}

		OpenglView(renderer:$renderView)
	}
}

struct App_Previews: PreviewProvider {
    static var previews: some View {
        TestView()
    }
}


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
