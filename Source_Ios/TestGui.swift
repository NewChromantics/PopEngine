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

@objc class SomeButton:NSObject
{
	@State public var Label = "The Label"

}


struct ContentView: View 
{
	var TestLabel1 = PopEngineLabel(name:"TestLabel1", label:"The Label")
	var TestLabel2 = PopEngineLabel(name:"TestLabel2")
	var TestButton = PopEngineButton(name:"TestButton")
	

	var body: some View {

		VStack {
	        Text(TestLabel1?.label ?? "default test label1")
	        Text(TestLabel2!.label ?? "default test label2")
	        Button(action:TestButton!.onClicked)
	        {
	        	Text(TestButton?.label ?? "default button label")
			}
			Spacer()
		}
	}
}


struct TestGui_Previews: PreviewProvider {
    static var previews: some View {
		Group {
			ContentView()
				.previewDevice("iPhone SE (2nd generation)")
				.preferredColorScheme(.dark)
		}
    }
}
