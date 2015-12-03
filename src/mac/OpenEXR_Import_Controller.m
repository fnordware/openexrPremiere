
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
// OpenEXR_Import_Controller.m
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


#import "OpenEXR_Import_Controller.h"

@implementation OpenEXR_Import_Controller

- (id)init:(NSArray *)menu_items
			red:(NSString *)redChannel
			green:(NSString *)greenChannel
			blue:(NSString *)blueChannel
			alpha:(NSString *)alphaChannel
			bypass:(BOOL)bypassConversion
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"OpenEXR_Import_Dialog" owner:self]))
		return nil;
	
	[theWindow center];
	
	//[redMenu removeAllItems];
	//[greenMenu removeAllItems];
	//[blueMenu removeAllItems];
	//[alphaMenu removeAllItems];
	
	[redMenu addItemsWithTitles:menu_items];
	[greenMenu addItemsWithTitles:menu_items];
	[blueMenu addItemsWithTitles:menu_items];
	[alphaMenu addItemsWithTitles:menu_items];
	
	[redMenu selectItemWithTitle:redChannel];
	[greenMenu selectItemWithTitle:greenChannel];
	[blueMenu selectItemWithTitle:blueChannel];
	[alphaMenu selectItemWithTitle:alphaChannel];
	
	[bypassCheckbox setState:(bypassConversion ? NSOnState : NSOffState)];
	
	theResult = INDIALOG_RESULT_CONTINUE;
	
	return self;
}

- (IBAction)clickOK:(id)sender {
	theResult = INDIALOG_RESULT_OK;
}

- (IBAction)clickCancel:(id)sender {
	theResult = INDIALOG_RESULT_CANCEL;
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (InDialogResult)getResult {
	return theResult;
}

- (NSString *)getRed {
	return [[redMenu selectedItem] title];
}

- (NSString *)getGreen {
	return [[greenMenu selectedItem] title];
}

- (NSString *)getBlue {
	return [[blueMenu selectedItem] title];
}

- (NSString *)getAlpha {
	return [[alphaMenu selectedItem] title];
}

- (BOOL)getBypass {
	return ([bypassCheckbox state] == NSOnState);
}

@end
