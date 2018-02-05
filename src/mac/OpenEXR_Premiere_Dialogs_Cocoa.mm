
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
// OpenEXR_Premiere_Dialogs_Cocoa.mm
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


#include "OpenEXR_Premiere_Dialogs.h"

#import <Cocoa/Cocoa.h>

#import "OpenEXR_Import_Controller.h"

#include <string>

using namespace std;


bool	
ProEXR_Channels(
	const ChannelsList	&channels,
	string				&red,
	string				&green,
	string				&blue,
	string				&alpha,
	DialogColorSpace	&colorSpace,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = false;
	

	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];
	
	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"OpenEXR_Import_Controller"];
									
									
	if(ui_controller_class)
	{
		NSMutableArray *menu_items = [[NSMutableArray alloc] init];
		
		for(ChannelsList::const_iterator i = channels.begin(); i != channels.end(); ++i)
		{
			[menu_items addObject:[NSString stringWithUTF8String:i->c_str()]];
		}
		
		const char *colorSpaceName = (colorSpace == DIALOG_COLORSPACE_LINEAR_ADOBE ? "Linear (Adobe)" :
										colorSpace == DIALOG_COLORSPACE_LINEAR_BYPASS ? "Linear" :
										colorSpace == DIALOG_COLORSPACE_SRGB ? "sRGB" :
										colorSpace == DIALOG_COLORSPACE_REC709 ? "Rec. 709" :
										colorSpace == DIALOG_COLORSPACE_CINEON ? "Cineon" :
										colorSpace == DIALOG_COLORSPACE_GAMMA22 ? "Gamma 2.2" :
										"Linear (Adobe)");
										
		OpenEXR_Import_Controller *ui_controller = [[ui_controller_class alloc] init:menu_items
													red:[NSString stringWithUTF8String:red.c_str()]
													green:[NSString stringWithUTF8String:green.c_str()]
													blue:[NSString stringWithUTF8String:blue.c_str()]
													alpha:[NSString stringWithUTF8String:alpha.c_str()]
													colorSpace:[NSString stringWithUTF8String:colorSpaceName]];
		if(ui_controller)
		{
			NSWindow *my_window = [ui_controller getWindow];
			
			if(my_window)
			{
				NSInteger modal_result;
				InDialogResult dialog_result;
			
				// dialog-on-dialog action
				NSModalSession modal_session = [NSApp beginModalSessionForWindow:my_window];
				
				do{
					modal_result = [NSApp runModalSession:modal_session];

					dialog_result = [ui_controller getResult];
				}
				while(dialog_result == INDIALOG_RESULT_CONTINUE && modal_result == NSRunContinuesResponse);
				
				[NSApp endModalSession:modal_session];
				
				
				if(dialog_result == INDIALOG_RESULT_OK || modal_result == NSRunStoppedResponse)
				{
					red   = [[ui_controller getRed]   UTF8String];
					green = [[ui_controller getGreen] UTF8String];
					blue  = [[ui_controller getBlue]  UTF8String];
					alpha = [[ui_controller getAlpha] UTF8String];
					
					NSString *colorSpaceString = [ui_controller getColorSpace];
					
					colorSpace = ([colorSpaceString isEqualToString:@"Linear (Adobe)"] ? DIALOG_COLORSPACE_LINEAR_ADOBE :
									[colorSpaceString isEqualToString:@"Linear"] ? DIALOG_COLORSPACE_LINEAR_BYPASS :
									[colorSpaceString isEqualToString:@"sRGB"] ? DIALOG_COLORSPACE_SRGB :
									[colorSpaceString isEqualToString:@"Rec. 709"] ? DIALOG_COLORSPACE_REC709 :
									[colorSpaceString isEqualToString:@"Cineon"] ? DIALOG_COLORSPACE_CINEON :
									[colorSpaceString isEqualToString:@"Gamma 2.2"] ? DIALOG_COLORSPACE_GAMMA22 :
									DIALOG_COLORSPACE_LINEAR_ADOBE);
					
					result = true;
				}

				[my_window close];
			}
			
			[ui_controller release];
		}
		
		[menu_items release];
	}
	
	return result;
}
