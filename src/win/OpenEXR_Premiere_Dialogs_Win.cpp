
//
//	ProEXR AE
// 
//	by Brendan Bolles <brendan@fnordware.com>
//


#include "ProEXR_Premiere_Dialogs.h"

#include <windows.h>

#include "serials.h"

#include <string>

using namespace std;

HINSTANCE hDllInstance = NULL;


#define PRO_EXR_DUMMY_PREFIX "Software\\AceTomato\\Produce"
#define PRO_EXR_PREFIX		 "Software\\fnord\\ProEXR"


#define DATE_KEY "Apple"
#define SERIAL_KEY "SerialNumber"

#define TRIAL_DAYS		15
#define MAJOR_VERSION	1


extern HINSTANCE hDllInstance;

static int DaysSince2000(int year, int month, int day)
{
	int total = 0;
	
	total += 365 * (year - 2000);
	
	total += (year - 2000) / 4; // leap days (I know, not totally precise)
	
	
	int month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	
	if(year % 4 == 0)
		month_days[1] = 29;

	for(int m = 1; m < month; m++)
		total += month_days[m - 1];
		
	
	total += day - 1;
	
	return total;
}

static int DaysPast(int cur_year, int cur_month, int cur_day,
					int past_year, int past_month, int past_day)
{
	return (DaysSince2000(cur_year, cur_month, cur_day) -
				DaysSince2000(past_year, past_month, past_day));
}


static int DaysRemaining(void)
{
	int days_remaining = 0;

	SYSTEMTIME	dateTime;
	GetSystemTime(&dateTime);

	char date_str[256];

	HKEY proexr_hkey;

	// get saved date
	LONG reg_error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PRO_EXR_DUMMY_PREFIX, 0, KEY_READ, &proexr_hkey);

	if(reg_error != ERROR_SUCCESS)
		reg_error = RegOpenKeyEx(HKEY_CURRENT_USER, PRO_EXR_DUMMY_PREFIX, 0, KEY_READ, &proexr_hkey);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD siz;

		reg_error = RegQueryValueEx(proexr_hkey, DATE_KEY, 0, NULL, NULL, &siz);

		if(reg_error == ERROR_SUCCESS && siz < 255)
		{
			// got date
			reg_error = RegQueryValueEx(proexr_hkey, DATE_KEY, 0, NULL, (LPBYTE)date_str, &siz);

			if(reg_error == ERROR_SUCCESS)
			{
				int sYear, sMonth, sDay;

				sscanf(date_str, "%d %d %d", &sYear, &sMonth, &sDay);

				days_remaining = (TRIAL_DAYS - DaysPast(dateTime.wYear, dateTime.wMonth, dateTime.wDay,
												sYear, sMonth, sDay) );
			}
		}
	}

	if(reg_error != ERROR_SUCCESS)
	{
		// save date
		reg_error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, PRO_EXR_DUMMY_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &proexr_hkey, NULL);

		if(reg_error != ERROR_SUCCESS)
			reg_error = RegCreateKeyEx(HKEY_CURRENT_USER, PRO_EXR_DUMMY_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &proexr_hkey, NULL);

		if(reg_error == ERROR_SUCCESS)
		{
			// save date
			sprintf(date_str, "%d %d %d", dateTime.wYear, dateTime.wMonth, dateTime.wDay);

			reg_error = RegSetValueEx(proexr_hkey, DATE_KEY, NULL, REG_SZ, (BYTE *)date_str, strlen(date_str));

			days_remaining = TRIAL_DAYS;
		}

		reg_error = RegCloseKey(proexr_hkey);
	}

	if(days_remaining < 0)
		days_remaining = 0;

	return days_remaining;
}


static void SaveSerial(char *serial)
{
	HKEY proexr_hkey;

	LONG reg_error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, PRO_EXR_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &proexr_hkey, NULL);

	if(reg_error != ERROR_SUCCESS)
		reg_error = RegCreateKeyEx(HKEY_CURRENT_USER, PRO_EXR_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &proexr_hkey, NULL);

	if(reg_error == ERROR_SUCCESS)
	{
		reg_error = RegSetValueEx(proexr_hkey, SERIAL_KEY, NULL, REG_SZ, (BYTE *)serial, strlen(serial));

		reg_error = RegCloseKey(proexr_hkey);
	}
}



bool ReadSerial(char *serial, int buf_len)
{
	bool copied_serial = false;

	HKEY proexr_hkey;

	LONG reg_error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PRO_EXR_PREFIX, 0, KEY_READ, &proexr_hkey);

	if(reg_error != ERROR_SUCCESS)
		reg_error = RegOpenKeyEx(HKEY_CURRENT_USER, PRO_EXR_PREFIX, 0, KEY_READ, &proexr_hkey);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD siz;

		reg_error = RegQueryValueEx(proexr_hkey, SERIAL_KEY, 0, NULL, NULL, &siz);

		if(reg_error == ERROR_SUCCESS && siz < buf_len)
		{
			reg_error = RegQueryValueEx(proexr_hkey, SERIAL_KEY, 0, NULL, (LPBYTE)serial, &siz);

			if(reg_error == ERROR_SUCCESS)
				copied_serial = true;
		}

		reg_error = RegCloseKey(proexr_hkey);
	}

	return copied_serial;
}



static void DeleteSerial(void)
{
	HKEY proexr_hkey;

	LONG reg_error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PRO_EXR_PREFIX, 0, KEY_SET_VALUE, &proexr_hkey);

	if(reg_error != ERROR_SUCCESS)
		reg_error = RegOpenKeyEx(HKEY_CURRENT_USER, PRO_EXR_PREFIX, 0, KEY_SET_VALUE, &proexr_hkey);

	if(reg_error == ERROR_SUCCESS)
	{
#if _MSC_VER >= 1400

#ifdef _WIN64
#define WHICH_REGISTRY KEY_WOW64_64KEY
#else
#define WHICH_REGISTRY KEY_WOW64_32KEY
#endif
		reg_error = RegDeleteKeyEx(proexr_hkey, SERIAL_KEY, WHICH_REGISTRY, NULL);
#else
		reg_error = RegDeleteKey(proexr_hkey, SERIAL_KEY);
#endif

		reg_error = RegCloseKey(proexr_hkey);
	}
}

bool CheckRegistration(void)
{
	char str[256];
	
	if( ReadSerial(str, 255) )
	{
		unsigned long	val;
		unsigned short	num;

		if( !parseSN(&val, &num, (char *)str) )
		{		
			if( !verifySN(val, num, PRODUCT_PROEXR, MAJOR_VERSION) )
			{
				return true;
			}
		}
	}
	
	return false;
}

enum {
	DLOG_noUI = -1,
	DLOG_Register = IDOK,
	DLOG_Demo = IDCANCEL,
	DLOG_Buy = 3,
	DLOG_Line1,
	DLOG_Line2,
	DLOG_Field
};

bool g_registered = false;
int g_days_remaining = 0;


static BOOL CALLBACK RegisterProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
	if(message == WM_INITDIALOG)
	{
		char first_line[256], second_line[256];

		if(g_days_remaining)
		{
			sprintf((char *)first_line, "%d days left in the trial period,", g_days_remaining );
			sprintf((char *)second_line, "or enter a serial number below.");
		}
		else
		{
			sprintf((char *)first_line, "The trial period is over,");
			sprintf((char *)second_line, "please enter a serial number.");
			SetDlgItemText(hwndDlg, DLOG_Demo, "Abort");
		}

		SetDlgItemText(hwndDlg, DLOG_Line1, first_line);
		SetDlgItemText(hwndDlg, DLOG_Line2, second_line);

		EnableWindow(GetDlgItem(hwndDlg,DLOG_Register),FALSE);

		SetFocus(GetDlgItem(hwndDlg, DLOG_Field));

		return TRUE; //not sure about this, may want to return FALSE here
	}
	else if(message == WM_COMMAND)
	{
		switch (LOWORD(wParam)) 
		{ 
			case DLOG_Register:
				do{
					char serialStr[256];

					GetDlgItemText(hwndDlg, DLOG_Field, serialStr, 255);

					SaveSerial(serialStr);

					g_registered = true;
				}while(0);

				EndDialog(hwndDlg, 0);

				return TRUE;

			case DLOG_Demo:
				EndDialog(hwndDlg, 0);

				return TRUE;

			case DLOG_Buy:
				ShellExecute(NULL, NULL, "http://www.fnordware.com/ProEXR/buy_proexr.html", NULL, "C:\\temp",SW_SHOWNORMAL);

				return TRUE;

			case DLOG_Field:
					do {
						char serialStr[256];

						GetDlgItemText(hwndDlg, DLOG_Field, serialStr, 255);

						unsigned long	val;
						unsigned short	num;

						if( !parseSN(&val, &num, serialStr) )
						{
							if( !verifySN(val, num, PRODUCT_PROEXR, MAJOR_VERSION) )
							{
								EnableWindow(GetDlgItem(hwndDlg,DLOG_Register),TRUE);
							}
							else
								EnableWindow(GetDlgItem(hwndDlg,DLOG_Register),FALSE);
						}
						else
							EnableWindow(GetDlgItem(hwndDlg,DLOG_Register),FALSE);

					}while (0);

				return TRUE;
		}
	}

	return FALSE; 
} 

RegistrationState
ProEXR_Registration(
	bool			show_dialog,
	const void		*plugHndl,
	const void		*mwnd)
{
	RegistrationState reg_state = REG_TRIAL;
	
	if( CheckRegistration() )
		return REG_REGISTERED;
	
	int days_remaining = DaysRemaining();
	g_days_remaining = days_remaining;
	
	if(days_remaining <= 0)
		reg_state = REG_ABORT;

	g_registered = false;

	if(show_dialog)
		DialogBox((HINSTANCE)hDllInstance, (LPSTR)"REGDIALOG", (HWND)mwnd, (DLGPROC)RegisterProc);

	if(g_registered)
		reg_state = REG_REGISTERED;

	return reg_state;
}


static WORD	g_item_clicked = 0;

static const ChannelsList	*g_channels;
static string				*g_red;
static string				*g_green;
static string				*g_blue;
static string				*g_alpha;


// dialog item IDs
enum {
	IN_noUI = -1,
	IN_OK = IDOK,
	IN_Cancel = IDCANCEL,
	IN_Red_Menu = 3,
	IN_Green_Menu, 
	IN_Blue_Menu,
	IN_Alpha_Menu
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
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = false;

	g_channels = &channels;
	g_red = &red;
	g_green = &green;
	g_blue = &blue;
	g_alpha = &alpha;


	int status = DialogBox(hDllInstance, (LPSTR)"CHANDIALOG", (HWND)mwnd, (DLGPROC)InDialogProc);


	if(g_item_clicked == IN_OK)
	{
		red = *g_red;
		green = *g_green;
		blue = *g_blue;
		alpha = *g_alpha;

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
