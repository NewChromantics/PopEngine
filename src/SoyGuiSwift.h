#pragma once

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

#if !TARGET_OS_IOS
#import <AppKit/AppKit.h>
#endif

#include <string>
#include "SoyWindow.h"	//	could forward declare these

namespace Swift
{
	//	this finds a class with this name, and returns it's window from .window
	NSWindow* _Nonnull	CreateSwiftWindow(const std::string& Name);
	
	

	std::shared_ptr<SoyWindow>			GetWindow(const std::string& Name);
	std::shared_ptr<SoyLabel>			GetLabel(const std::string& Name);
	std::shared_ptr<SoyButton>			GetButton(const std::string& Name);
	std::shared_ptr<SoyTickBox>			GetTickBox(const std::string& Name);
	std::shared_ptr<Gui::TRenderView>	GetRenderView(const std::string& Name);
	std::shared_ptr<Gui::TList>			GetList(const std::string& Name);

}


@interface PopEngineControl : NSObject

@property (strong, atomic, nonnull) NSString* name;

- (nonnull instancetype)initWithName:(nonnull NSString* )name;
- (nonnull instancetype)init NS_UNAVAILABLE;

- (void)updateUi;

@end


@interface PopEngineLabel : PopEngineControl

@property (strong, atomic, nonnull) NSString* label;

- (nonnull instancetype)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end


@interface PopEngineButton : PopEngineControl


@property (strong, atomic, nonnull) NSString* label;

- (nonnull instancetype)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

- (void)onClicked;

@end


@interface PopEngineTickBox : PopEngineControl

@property (strong, atomic, nonnull) NSString* label;
@property (atomic) Boolean value;

- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(Boolean)value label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(Boolean)value;
- (nonnull instancetype)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end

@interface PopEngineList : PopEngineControl

@property (strong, atomic, nonnull) NSString* label;
@property (strong, atomic, nonnull) NSMutableArray<NSString*> *value;

- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(nonnull NSMutableArray<NSString*>*)value label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(nonnull NSMutableArray<NSString*>*)value;
- (nonnull instancetype)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end


#if TARGET_OS_IOS	//	gr: this SHOULD be always defined by xcode
@interface GLView : GLKView
#else
@interface GLView : NSOpenGLView
#endif
{
}
@end

@interface PopEngineRenderView : PopEngineControl

@property (strong, atomic) MTKView* _Nullable metalView;
@property (strong, atomic) GLView* _Nullable openglView;

@property (atomic) Boolean value;

- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end

