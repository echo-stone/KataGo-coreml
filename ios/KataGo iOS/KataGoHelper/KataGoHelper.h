//
//  KataGoHelper.h
//  KataGoHelper
//
//  Created by Chin-Chang Yang on 2024/7/5.
//

#import <Foundation/Foundation.h>

//! Project version number for KataGoHelper.
FOUNDATION_EXPORT double KataGoHelperVersionNumber;

//! Project version string for KataGoHelper.
FOUNDATION_EXPORT const unsigned char KataGoHelperVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <KataGoHelper/PublicHeader.h>

@interface KataGoHelper : NSObject

+ (void)runGtp;

+ (NSString * _Nonnull)getMessageLine;

+ (void)sendCommand:(NSString * _Nonnull)command;

@end
