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
	//var TestLabel3 = PopEngineLabel()
	var TestButton1 = PopEngineButton(name:"TestButton")
	//var TestButton2 = PopEngineButton()
	var TestTickBox1 = PopEngineTickBox(name:"TickBox1")
	var TestTickBox2 = PopEngineTickBox(name:"TickBox2", label:"TickBox1")
	var TestTickBox3 = PopEngineTickBox(name:"TickBox3", value:true)
	var TestTickBox4 = PopEngineTickBox(name:"TickBox4", value:false)
	var TestTickBox5 = PopEngineTickBox(name:"TickBox5", value:true, label:"Tickbox5")
	

	var body: some View {

		VStack {
	        Text(TestLabel1.label)
			Text(TestLabel2.label )
	        Button(action:TestButton1.onClicked)
	        {
	        	Text(TestButton1.label)
			}
			/*
			Toggle("label", isOn: $TestTickBox1.value) 
			{
				//Text(label:TestTickBox1?.label ?? "default TestTickBox1")
			}
			*/
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
