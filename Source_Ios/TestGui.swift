//
//  TestGui.swift
//  PopEngineTestApp_Ios
//
//  Created by Graham Reeves on 22/11/2020.
//  Copyright © 2020 NewChromantics. All rights reserved.
//

import SwiftUI
/*
Child: UIWindow
Child: UITransitionView
Child: UIDropShadowView
Child: _TtGC7SwiftUI14_UIHostingViewV20PopEngineTestApp_Ios11ContentView_
Child: _TtGC7SwiftUI16PlatformViewHostGVS_P10$1a0ff781832PlatformViewRepresentableAdaptorV20PopEngineTestApp_Ios9MetalView__
Child: MTKView
Child: _TtCOCV7SwiftUI11DisplayList11ViewUpdater8Platform13CGDrawingView
Child: _TtCOCV7SwiftUI11DisplayList11ViewUpdater8Platform13CGDrawingView
Child: _TtCOCV7SwiftUI11DisplayList11ViewUpdater8Platform13CGDrawingView
*/
struct ContentView: View 
{
	func OnButtonClick()
	{
		print("Clicked")
	}

	var body: some View {

		VStack {
			MetalView()
	        Text("Panopoly")
	        Text("Panopoly")
	        Button(action: OnButtonClick)
	        {
	        	Text("Button")
			}
		}
	}
}


struct TestGui_Previews: PreviewProvider {
    static var previews: some View {
		Group {
			ContentView()
				.preferredColorScheme(.dark)
		}
    }
}
