//
//  TestGui.swift
//  PopEngineTestApp_Ios
//
//  Created by Graham Reeves on 22/11/2020.
//  Copyright © 2020 NewChromantics. All rights reserved.
//

import SwiftUI



@objc class PopEngineLabelWrapper : PopEngineLabel , ObservableObject
{
	//	called from objective-c's .label setter
	@objc override func updateUi() 
	{
		//	trigger published var change to redraw view
		labelCopy = self.label
	}
	
	//	use this to read & write
	var theLabel: String 
	{
        get 
        {
            return super.label // reaching ancestor prop
        }
        set 
        {
        	labelCopy = newValue
            super.label = newValue	//	this probably calls updateUi
        }
    }
    
	@Published private var labelCopy:String = "LabelCopy"
}




struct ContentView: View 
{
	@State var label1: String = "Uninitialised"
	//var labelWrapper = StringTest(name:"TestLabel1", label:"InitialLabel")
	@ObservedObject var Label1Wrapper = PopEngineLabelWrapper(name:"TestLabel1", label:"InitialLabel")
	@ObservedObject var FrameCounterLabel = PopEngineLabelWrapper(name:"FrameCounterLabel", label:"FrameCounter")
	@State var renderView = PopEngineRenderView(name:"TestRenderView") 
	
	var body: some View 
	{
		OpenglView(renderer:$renderView)
		Text(label1)
		Text(Label1Wrapper.theLabel)
		Text(FrameCounterLabel.theLabel)
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
