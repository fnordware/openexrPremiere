//
//  OpenEXR_Import_Controller.h
//
//  Created by Brendan Bolles on 12/3/11.
//  Copyright 2011 fnord. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef enum {
	INDIALOG_RESULT_CONTINUE = 0,
	INDIALOG_RESULT_OK,
	INDIALOG_RESULT_CANCEL
} InDialogResult;

@interface OpenEXR_Import_Controller : NSObject {
	IBOutlet NSWindow *theWindow;
	IBOutlet NSPopUpButton *redMenu;
	IBOutlet NSPopUpButton *greenMenu;
	IBOutlet NSPopUpButton *blueMenu;
	IBOutlet NSPopUpButton *alphaMenu;
	IBOutlet NSButton *bypassCheckbox;
	InDialogResult theResult;
}

- (id)init:(NSArray *)menu_items
			red:(NSString *)redChannel
			green:(NSString *)greenChannel
			blue:(NSString *)blueChannel
			alpha:(NSString *)alphaChannel
			bypass:(BOOL)bypassConversion;

- (IBAction)clickOK:(id)sender;
- (IBAction)clickCancel:(id)sender;

- (NSWindow *)getWindow;
- (InDialogResult)getResult;

- (NSString *)getRed;
- (NSString *)getGreen;
- (NSString *)getBlue;
- (NSString *)getAlpha;
- (BOOL)getBypass;

@end
