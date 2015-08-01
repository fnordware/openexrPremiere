
//
//	ProEXR AE
// 
//	by Brendan Bolles <brendan@fnordware.com>
//


#include "OpenEXR_Premiere_Dialogs.h"

#include <windows.h>

#include <string>

using namespace std;

HINSTANCE hDllInstance = NULL;

static WORD	g_item_clicked = 0;

static const ChannelsList	*g_channels;
static string				*g_red;
static string				*g_green;
static string				*g_blue;
static string				*g_alpha;
static bool					g_bypass;


// dialog item IDs
enum {
	IN_noUI = -1,
	IN_OK = IDOK,
	IN_Cancel = IDCANCEL,
	IN_Red_Menu = 3,
	IN_Green_Menu, 
	IN_Blue_Menu,
	IN_Alpha_Menu,
	IN_Bypass_Check
};


static BOOL CALLBACK InDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch (message) 
    { 
      case WM_INITDIALOG:
		  do{
			// add lists to combo boxes
			HWND menu[4] = {	GetDlgItem(hwndDlg, IN_Red_Menu),
								GetDlgItem(hwndDlg, IN_Green_Menu),
								GetDlgItem(hwndDlg, IN_Blue_Menu),
								GetDlgItem(hwndDlg, IN_Alpha_Menu) };

			string *value[4] = { g_red, g_green, g_blue, g_alpha };
			
			int current_index = 0;

			// add the (none) item to each menu
			for(int j=0; j < 4; j++)
			{
				SendMessage(menu[j], (UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)"(none)");
				SendMessage(menu[j], (UINT)CB_SETITEMDATA, (WPARAM)current_index, (LPARAM)(DWORD)current_index); // (none) is index 0

				if(*value[j] == "(none)")
					SendMessage(menu[j], CB_SETCURSEL, (WPARAM)current_index, (LPARAM)0);
			}

			current_index++;

			// add the channels
			for(ChannelsList::const_iterator i = g_channels->begin(); i != g_channels->end(); ++i)
			{
				for(int j=0; j < 4; j++)
				{
					SendMessage(menu[j], (UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)i->c_str());
					SendMessage(menu[j], (UINT)CB_SETITEMDATA, (WPARAM)current_index, (LPARAM)(DWORD)current_index); // this is the channel index number

					int item_width = i->size() * 7;
					int menu_width = SendMessage(menu[j], (UINT)CB_GETDROPPEDWIDTH, (WPARAM)0, (LPARAM)0);

					if(item_width > menu_width)
						SendMessage(menu[j], (UINT)CB_SETDROPPEDWIDTH, (WPARAM)item_width, (LPARAM)0);

					if(*value[j] == *i)
						SendMessage(menu[j], CB_SETCURSEL, (WPARAM)current_index, (LPARAM)0);
				}

				current_index++;
			}

			SendMessage(GetDlgItem(hwndDlg, IN_Bypass_Check), BM_SETCHECK, (WPARAM)g_bypass, (LPARAM)0);

		  }while(0);
		return FALSE;
 
        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch (LOWORD(wParam)) 
            { 
                case IN_OK: 
					do{
						HWND menu[4] = {	GetDlgItem(hwndDlg, IN_Red_Menu),
											GetDlgItem(hwndDlg, IN_Green_Menu),
											GetDlgItem(hwndDlg, IN_Blue_Menu),
											GetDlgItem(hwndDlg, IN_Alpha_Menu) };

						string *value[4] = { g_red, g_green, g_blue, g_alpha };

						// get the channel name
						TCHAR channel_name[256];

						for(int j=0; j < 4; j++)
						{
							LRESULT cur_sel = SendMessage(menu[j],(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

							int len = SendMessage(menu[j], (UINT)CB_GETLBTEXT, (WPARAM)cur_sel, (LPARAM)channel_name);

							*value[j] = channel_name;
						}

						g_bypass = SendMessage(GetDlgItem(hwndDlg, IN_Bypass_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);

					}while(0);

				case IN_Cancel:
					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 
                    return TRUE; 
            } 
    } 
    return FALSE; 
} 

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

	g_channels = &channels;
	g_red = &red;
	g_green = &green;
	g_blue = &blue;
	g_alpha = &alpha;
	g_bypass = bypassConversion;


	int status = DialogBox(hDllInstance, (LPSTR)"CHANDIALOG", (HWND)mwnd, (DLGPROC)InDialogProc);


	if(g_item_clicked == IN_OK)
	{
		red = *g_red;
		green = *g_green;
		blue = *g_blue;
		alpha = *g_alpha;
		bypassConversion = g_bypass;

		result = true;
	}

	return result;
}


BOOL WINAPI DllMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		hDllInstance = (HINSTANCE)hInstance;

	return TRUE;   // Indicate that the DLL was initialized successfully.
}
