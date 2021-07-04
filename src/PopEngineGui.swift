/*
	gr: until I work out how to make these classes accessible to projects including
		the frame work, copy & paste this file into your project
		
	gr: in popengine test, I can just include this in in the build, but via framework it
		fails to find my objective-c classes...
		
		
	//	your project's briding header needs to include to find the objective-c classes in the framework
	#import "XYZ/PopEngine.framework/Headers/SoyGuiSwift.h"
*/
import Foundation
import MetalKit
import SwiftUI




#if os(macOS)
typealias XViewRepresentable = NSViewRepresentable
#else
typealias XViewRepresentable = UIViewRepresentable
#endif



@objc class PopLabel : PopEngineLabel , ObservableObject
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



@objc class PopTickBox : PopEngineTickBox , ObservableObject
{
	//	called from objective-c's .label setter
	@objc override func updateUi() 
	{
		//	trigger published var change to redraw view
		labelCopy = self.label
		valueCopy = self.value
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
    
    var theValue: Bool 
	{
        get 
        {
            return super.value
        }
        set 
        {
        	valueCopy = newValue
            super.value = newValue	//	this probably calls updateUi
        }
    }
    
    //	gr: I think I only need one "dirty" variable to trigger swiftui update
	@Published private var labelCopy:String = "LabelCopy"
	@Published private var valueCopy:Bool = false
}


//	for some displays (and general swift optimisation) every item in a list should have a uid
//	todo: we should really move this engine side so a list is always a dictionary/keyed object
//		seeing as both sides kinda need that now
struct PopListItem : Identifiable 
{
	var id = UUID()
	var value: String
}


@objc class PopList : PopEngineList , ObservableObject
{
	//	called from objective-c's .label setter
	@objc override func updateUi()
	{
		//	trigger published var change to redraw view
		labelCopy = self.label

		//	regenerate the uuid'd values
		self.valueStrings = self.value as! [String]
		valueCopy = self.valueStrings.map( { PopListItem(value: $0) } )
		//valueCopy = self.value as! [String]
	}

	//	use this to read & write
	var theLabel: String
	{
		get
		{
			return super.label
		}
		set
		{
			labelCopy = newValue
			super.label = newValue
		}
	}

	var theValue: [PopListItem]
	{
		get
		{
			//return super.value as! [String]
			return valueCopy
		}

	}
	
	var theValueStrings: [String]
	{
		get
		{
			return self.valueStrings
		}

		set
		{
			//	the engine code is fixed now so we can set any NSArray, and makes a copy of the contents
			//	it was crashing as it seemed to assign to a temporary array that swift deleted?
			//	but swift can't see setValue(NSArray) so we still need to turn it into a mutable array 
			//let StringArray = newValue.map( { String($0.value) } )
			let StringArray = newValue
			let StringArrayNs = StringArray as NSArray
			let StringArrayMutable = StringArrayNs.mutableCopy() as! NSMutableArray
			super.value = StringArrayMutable
			//super.value.removeAllObjects()
			//super.value.addObjects(from:newValue )// as! NSArray ) 
			//valueCopy = newValue
			valueStrings = newValue
		}
	}			
	
	//	gr: I think I only need one "dirty" variable to trigger swiftui update
	@Published private var labelCopy:String = "LabelCopy"
	//	
	@Published private var valueCopy:[PopListItem] = []
	private var valueStrings:[String] = []
}

//	gr: make a NSView/UIView type in objective c? and remove user's decision between metal and opengl?
//NSViewRepresentable
#if !os(macOS)
struct MetalView: UIViewRepresentable 
{
	typealias UIViewType = MTKView

	//	pass in persistent PopEngine binding
	@Binding var renderer: PopEngineRenderView 
	
	init(renderer: Binding<PopEngineRenderView>) 
	{
    	self._renderer = renderer
	}
	
	//	gr: maybe should be using this to bind
    func makeCoordinator() -> Coordinator 
    {
        Coordinator(self)
    }

    func makeUIView(context: UIViewRepresentableContext<MetalView>) -> MTKView 
    {
    	if ( renderer.metalView == nil )
    	{
    		renderer.metalView = MTKView()
		}
    	/*
        let mtkView = MTKView()
        mtkView.delegate = context.coordinator
        mtkView.preferredFramesPerSecond = 60
        mtkView.enableSetNeedsDisplay = true
        if let metalDevice = MTLCreateSystemDefaultDevice() {
            mtkView.device = metalDevice
        }
        mtkView.framebufferOnly = false
        mtkView.clearColor = MTLClearColor(red: 0.05, green: 0.4, blue: 0.5, alpha: 1)
        mtkView.drawableSize = mtkView.frame.size
        mtkView.enableSetNeedsDisplay = true
        */
		return renderer.metalView!
    }
	
	func updateUIView(_ uiView: MTKView, context: UIViewRepresentableContext<MetalView>) 
	{
	}


    class Coordinator : NSObject, MTKViewDelegate 
    {
        var parent: MetalView
        var metalDevice: MTLDevice!
        var metalCommandQueue: MTLCommandQueue!
        
        init(_ parent: MetalView) 
        {
            self.parent = parent
            if let metalDevice = MTLCreateSystemDefaultDevice() {
                self.metalDevice = metalDevice
            }
            self.metalCommandQueue = metalDevice.makeCommandQueue()!
            super.init()
        }
        
        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) 
        {
        }
        
        func draw(in view: MTKView) 
        {
            guard let drawable = view.currentDrawable else {
                return
            }
            let commandBuffer = metalCommandQueue.makeCommandBuffer()
            let rpd = view.currentRenderPassDescriptor
            rpd?.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.10, 0.94, 1.0)
            rpd?.colorAttachments[0].loadAction = .clear
            rpd?.colorAttachments[0].storeAction = .store
            let re = commandBuffer?.makeRenderCommandEncoder(descriptor: rpd!)
            re?.endEncoding()
            commandBuffer?.present(drawable)
            commandBuffer?.commit()
        }
        
    }
}
#endif



/*
	usage: create a persistent PopEngineRenderView instance (ie @state)
	
	then create views as neccessary which link to the render view
	
	struct ContentView: View {
		@State var renderView = PopEngineRenderView(name:"RenderView")
		var body: some View {
		   OpenglView(renderer:$renderView)
		}
	}
*/
struct OpenglView: XViewRepresentable 
{
#if !os(macOS)
	typealias ViewType = GLView
	typealias ContextType = UIViewRepresentableContext<OpenglView>
#else//	uikit/ios
	typealias ViewType = GLView
	typealias ContextType = NSViewRepresentableContext<OpenglView>
#endif

	//	pass in persistent PopEngine binding
	@Binding var renderer: PopEngineRenderView 
	
	init(renderer: Binding<PopEngineRenderView>) 
	{
		self._renderer = renderer
	}

	func makeNSView(context: ContextType) -> ViewType
	{
		if ( renderer.openglView == nil )
		{
			renderer.openglView = ViewType()
		}
		return renderer.openglView!
	}
	
	func makeUIView(context: ContextType) -> ViewType
	{
		return makeNSView(context: context)
	}
	
	func updateUIView(_ uiView: ViewType, context: ContextType) 
	{
	}

	func updateNSView(_ uiView: ViewType, context: ContextType) 
	{
	}
	
}
