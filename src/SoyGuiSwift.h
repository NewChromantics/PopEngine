#pragma once

#import <Foundation/Foundation.h>


@interface PopEngineControl : NSObject

@property (strong, nonatomic, nonnull) NSString* name;

- (nonnull id)initWithName:(nonnull NSString* )name;

@end


@interface PopEngineLabel : PopEngineControl

@property (strong, nonatomic, nonnull) NSString* label;

- (nonnull id)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull id)initWithName:(nonnull NSString*)name;
//- (nonnull id)init;

@end


@interface PopEngineButton : PopEngineControl

@property (strong, nonatomic, nonnull) NSString* label;

- (nonnull id)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull id)initWithName:(nonnull NSString*)name;
//- (nonnull id)init;

- (void)onClicked;

@end


@interface PopEngineTickBox : PopEngineControl

@property (strong, nonatomic, nonnull) NSString* label;
@property (nonatomic) Boolean value;

- (nonnull id)initWithName:(nonnull NSString*)name value:(Boolean)value label:(nonnull NSString*)label;
- (nonnull id)initWithName:(nonnull NSString*)name value:(Boolean)value;
- (nonnull id)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label;
- (nonnull id)initWithName:(nonnull NSString*)name;
//- (nonnull id)init;

- (void)onChanged;

@end

