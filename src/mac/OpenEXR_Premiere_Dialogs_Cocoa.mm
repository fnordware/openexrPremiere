
//
//	ProEXR AE
// 
//	by Brendan Bolles <brendan@fnordware.com>
//


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
	bool				&bypassConversion,
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
	
		OpenEXR_Import_Controller *ui_controller = [[ui_controller_class alloc] init:menu_items
													red:[NSString stringWithUTF8String:red.c_str()]
													green:[NSString stringWithUTF8String:green.c_str()]
													blue:[NSString stringWithUTF8String:blue.c_str()]
													alpha:[NSString stringWithUTF8String:alpha.c_str()]
													bypass:bypassConversion];
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
					red   = [[ui_controller getRed]   cStringUsingEncoding:NSUTF8StringEncoding];
					green = [[ui_controller getGreen] cStringUsingEncoding:NSUTF8StringEncoding];
					blue  = [[ui_controller getBlue]  cStringUsingEncoding:NSUTF8StringEncoding];
					alpha = [[ui_controller getAlpha] cStringUsingEncoding:NSUTF8StringEncoding];
					
					bypassConversion = [ui_controller getBypass];
					
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
