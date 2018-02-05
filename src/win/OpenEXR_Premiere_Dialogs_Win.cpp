
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
// OpenEXR_Premiere_Dialogs_Win.cpp
// 
// OpenEXR plug-in for Adobe Premiere
//
//------------------------------------------


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
//static bool					g_bypass;
static DialogColorSpace		g_colorSpace;


// dialog item IDs
enum {
	IN_noUI = -1,
	IN_OK = IDOK,
	IN_Cancel = IDCANCEL,
	IN_Red_Menu = 3,
	IN_Green_Menu, 
	IN_Blue_Menu,
	IN_Alpha_Menu,
	//IN_Bypass_Check
	IN_Color_Space_Menu
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

			//SendMessage(GetDlgItem(hwndDlg, IN_Bypass_Check), BM_SETCHECK, (WPARAM)g_bypass, (LPARAM)0);

			const char *colorSpaces[] = {	"Linear (Adobe)",
											"Linear",
											"sRGB",
											"Rec. 709",
											"Cineon",
											"Gamma 2.2" };

			for(int i=DIALOG_COLORSPACE_LINEAR_ADOBE; i <= DIALOG_COLORSPACE_GAMMA22; i++)
			{
				SendMessage(GetDlgItem(hwndDlg, IN_Color_Space_Menu), (UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)colorSpaces[i]);
				SendMessage(GetDlgItem(hwndDlg, IN_Color_Space_Menu), (UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the channel index number
			}

			SendMessage(GetDlgItem(hwndDlg, IN_Color_Space_Menu), CB_SETCURSEL, (WPARAM)g_colorSpace, (LPARAM)0);

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

						//g_bypass = SendMessage(GetDlgItem(hwndDlg, IN_Bypass_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);

						g_colorSpace = (DialogColorSpace)SendMessage(GetDlgItem(hwndDlg, IN_Color_Space_Menu),(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

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
	DialogColorSpace	&colorSpace,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = false;

	g_channels = &channels;
	g_red = &red;
	g_green = &green;
	g_blue = &blue;
	g_alpha = &alpha;
	//g_bypass = bypassConversion;
	g_colorSpace = colorSpace;


	int status = DialogBox(hDllInstance, (LPSTR)"CHANDIALOG", (HWND)mwnd, (DLGPROC)InDialogProc);


	if(g_item_clicked == IN_OK)
	{
		red = *g_red;
		green = *g_green;
		blue = *g_blue;
		alpha = *g_alpha;
		//bypassConversion = g_bypass;
		colorSpace = g_colorSpace;

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
