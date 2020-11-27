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

/*
@objc class StringTest: PopEngineLabel 
{
    @Binding var labelCopy: String = "x"

	//init(labelCopy: Binding<String>)
	override init(name: String,label: String) 
	{
     //   self.includeDecimal = round(self.amount)-self.amount > 0
     	super.init(name:name, label:label)
		self.labelCopy = super.label // beta 4

    }
}
*/
/*
@objc class PopEngineLabelWrapper : PopEngineLabel 
{
	@objc
	override func updateUi() 
	{
		labelCopy = self.label
		print("Ui updated in swift, LabelCopy now \(labelCopy) \(self.label)");
	}
	
	required override init() 
	{
    	//self.username = "Anonymous"
    	super.init()
	}

	@Binding var labelCopy:String	
}
*/
@objc class PopEngineLabelWrapper : PopEngineLabel , ObservableObject
{
	@objc
	override func updateUi() 
	{
		labelCopy = self.label
		print("Ui updated in swift, LabelCopy now \(labelCopy) \(self.label)")
	}
	/*
	required override init(name:String, label:String)
	{
    	//self.username = "Anonymous"
    	super.init(name:name, label:label)
	}
*/
	func getLabel() -> String
	{
		print("GetLabel() -> \(self.labelCopy)")
		return self.labelCopy
		//return self.label	//	exception
	}
	
	@Published var labelCopy:String = "LabelCopy"
}

struct ContentView: View 
{
	@State var label1: String = "Uninitialised"
	//var labelWrapper = StringTest(name:"TestLabel1", label:"InitialLabel")
	@ObservedObject var Label1Wrapper = PopEngineLabelWrapper(name:"TestLabel1", label:"InitialLabel")
	
	var body: some View 
	{
		Text(label1)
		Text(Label1Wrapper.labelCopy)
		//Text(TestLabel1Wrapper.labelCopy)
	}			
	/*
	@State var TestLabel1 = PopEngineLabelWrapper(name:"TestLabel1", label:"The Label")
	//var TestLabel1 = PopEngineLabel(name:"TestLabel1", label:"The Label")
	var TestLabel2 = PopEngineLabel(name:"TestLabel2")
	//var TestLabel3 = PopEngineLabel()
	@State var TestButton1 = PopEngineButton(name:"TestButton1x")
	@State var TestButton2 = PopEngineButton(name:"TestButton2")
	//var TestButton3 = PopEngineButton()
	@State var TestTickBox1 = PopEngineTickBox(name:"TestTickBox1")
	var TestTickBox2 = PopEngineTickBox(name:"TestTickBox2", label:"TickBox1")
	var TestTickBox3 = PopEngineTickBox(name:"TestTickBox3", value:true)
	var TestTickBox4 = PopEngineTickBox(name:"TestTickBox4", value:false)
	var TestTickBox5 = PopEngineTickBox(name:"TestTickBox5", value:true, label:"Tickbox5")

	func OnClick2()
	{
		TestButton2.label = "hello";
		TestButton2.onClicked()
	}	

	var body: some View {

		VStack {
			Text(TestLabel1Wrapper.label)
			Text(TestLabel1.label)
			Text(TestLabel2.label )
	        Button(action:TestButton1.onClicked)
	        {
	        	Text(TestButton1.label)
			}
			
			Button(action:OnClick2)
	        {
	        	Text(TestButton2.label)
			}
			
			Toggle("label", isOn: $TestTickBox1.value)
			
			
			Spacer()
		}
	}
	*/
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
