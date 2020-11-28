//
//  Metal.swift
//  PanopolySwiftGuiTest
//
//  Created by Graham Reeves on 23/11/2020.
//

import Foundation
import MetalKit
import SwiftUI




//NSViewRepresentable
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
    	return renderer.metalView
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




//NSViewRepresentable
struct OpenglView: UIViewRepresentable 
{
	typealias UIViewType = GLKView

	//	pass in persistent PopEngine binding
	@Binding var renderer: PopEngineRenderView 
	
	init(renderer: Binding<PopEngineRenderView>) 
	{
    	self._renderer = renderer
	}
	/*
	//	gr: maybe should be using this to bind
    func makeCoordinator() -> Coordinator 
    {
        Coordinator(self)
    }
*/
    func makeUIView(context: UIViewRepresentableContext<OpenglView>) -> UIViewType
    {
    	if ( renderer.openglView == nil )
    	{
    		renderer.openglView = UIViewType()
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
    	return renderer.openglView
    }
	
	func updateUIView(_ uiView: UIViewType, context: UIViewRepresentableContext<OpenglView>) 
	{
	}

/*
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
    */
}
