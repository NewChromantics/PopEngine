#pragma once

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

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

@interface PopEngineStringArray : PopEngineControl

@property (strong, atomic, nonnull) NSString* label;
@property (strong, atomic, nonnull) NSMutableArray<NSString*> *value;

- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(nonnull NSMutableArray<NSString*>*)value label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name value:(nonnull NSMutableArray<NSString*>*)value;
- (nonnull instancetype)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end

@class GLView;

@interface PopEngineRenderView : PopEngineControl

@property (strong, atomic) MTKView* metalView;

@property (strong, atomic) GLView* openglView;

@property (atomic) Boolean value;

- (nonnull instancetype)initWithName:(nonnull NSString*)name;
- (nonnull instancetype)init NS_UNAVAILABLE;

@end

