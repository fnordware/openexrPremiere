
//////////////////////////////////////////////////////////////////////////////
// 
// Copyright (c) 2015, Brendan Bolles
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
//////////////////////////////////////////////////////////////////////////////

//------------------------------------------
//
// OpenEXR_Import_Controller.h
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


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
