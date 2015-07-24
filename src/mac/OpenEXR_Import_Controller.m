//
//  OpenEXR_InUI_Controller.m
//
//  Created by Brendan Bolles on 12/3/11.
//  Copyright 2011 fnord. All rights reserved.
//

#import "OpenEXR_Import_Controller.h"

@implementation OpenEXR_Import_Controller

- (id)init:(NSArray *)menu_items
			red:(NSString *)redChannel
			green:(NSString *)greenChannel
			blue:(NSString *)blueChannel
			alpha:(NSString *)alphaChannel
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


@end
