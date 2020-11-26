#pragma once

#import <Foundation/Foundation.h>


@interface PopEngineControl : NSObject

@property (strong, nonatomic) NSString* name;

- (id)initWithName:(NSString*)name;

@end


@interface PopEngineLabel : PopEngineControl

@property (strong, nonatomic) NSString* label;


- (id)initWithName:(NSString*)name label:(NSString*)label;
- (id)initWithName:(NSString*)name;

@end


@interface PopEngineButton : PopEngineControl

@property (strong, nonatomic) NSString* label;

//- (id)initWithName:(NSString*)name;

- (void)onClicked;

@end


