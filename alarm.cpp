#include <process.h>
#include "alarm.h"
#include "settings.h"
#include "resource.h"
#include <mmsystem.h>

#define ALARM_KEY "Software\\Clocktray"

//Alarm status
static int g_iAlarmHour		= -1; // -1 = not set
static int g_iAlarmMinute	= -1;
static BOOL g_bAlarmRinging	= FALSE;
static int g_iAlarmSound	= 0;
static char g_szAlarmWav[MAX_PATH] = {0};

// External var from main.cpp
extern HWND g_hwnd;

// Timer ID for beeps
#define IDT_ALARM_BEEP 3

void Alarm_Load()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, ALARM_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		g_iAlarmHour = -1;
		g_iAlarmMinute = -1;
		return;
	}

	DWORD value = 0;
	DWORD size= sizeof(DWORD);
	DWORD type = 0;

	if (RegQueryValueEx(hKey, "AlarmHour", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
		g_iAlarmHour = (int)value;
	else
		g_iAlarmHour = -1;

	size = sizeof(DWORD);
	type = 0;
	if (RegQueryValueEx(hKey, "AlarmMinute", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
		g_iAlarmMinute = (int)value;
	else
		g_iAlarmMinute = -1;

	size = sizeof(DWORD);
	type = 0;
	if (RegQueryValueEx(hKey, "AlarmSound", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
		g_iAlarmSound = (int)value;
	else
		g_iAlarmSound = 0;

	// WAV path
	size = sizeof(g_szAlarmWav);
	type = 0;
	g_szAlarmWav[0] = '\0';
	RegQueryValueEx(hKey, "AlarmWavPath", NULL, &type, (LPBYTE)g_szAlarmWav, &size);

	RegCloseKey(hKey);
}

void Alarm_Save()
{
	HKEY hKey;
	DWORD disp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, ALARM_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
		return;

	DWORD value;

	value = (DWORD)g_iAlarmHour;
	RegSetValueEx(hKey, "AlarmHour", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	value = (DWORD)g_iAlarmMinute;
	RegSetValueEx(hKey, "AlarmMinute", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	value = (DWORD)g_iAlarmSound;
	RegSetValueEx(hKey, "AlarmSound", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	if (g_szAlarmWav[0] != '\0')
	{
		RegSetValueEx(hKey, "AlarmWavPath", 0, REG_SZ, (const BYTE*)g_szAlarmWav, lstrlen(g_szAlarmWav) + 1);
	}

	RegCloseKey(hKey);
}

BOOL Alarm_Check()
{
	if (!Alarm_IsSet()) return FALSE;
	if (g_bAlarmRinging) return FALSE; // Already ringing

	SYSTEMTIME st;
	GetLocalTime(&st);

	if (st.wHour == g_iAlarmHour && st.wMinute == g_iAlarmMinute && st.wSecond == 0)
	{
		return TRUE;
	}
	return FALSE;
}

unsigned __stdcall AlarmThreadProc(void* pParam)
{
	HWND hwnd = (HWND)pParam;

	// Show window
	MessageBox(NULL,
				"Alarm!\n\n",
				"Clocktray alarm",
				MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST | MB_SETFOREGROUND);

	Alarm_StopBeeping(hwnd);

	return 0;
}

void Alarm_StartBeeping(HWND hwnd)
{
	g_bAlarmRinging = TRUE;
	SetTimer(hwnd, IDT_ALARM_BEEP, 1000, NULL);
	Beep(880, 300);

	Alarm_PlaySound(TRUE);

	_beginthreadex(NULL, 0, AlarmThreadProc, (void*)hwnd, 0, NULL);
}

void Alarm_StopBeeping(HWND hwnd)
{
	g_bAlarmRinging = FALSE;
	KillTimer(hwnd, IDT_ALARM_BEEP);

	PlaySound(NULL, NULL, 0);
}

static int g_iDlgHour = 0;
static int g_iDlgMinute = 0;

BOOL CALLBACK AlarmDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			char buf[8];
			wsprintf(buf, "%d", g_iDlgHour);
			SetDlgItemText(hDlg, IDC_EDIT_HOUR,  buf);

			wsprintf(buf, "%d", g_iDlgMinute);
			SetDlgItemText(hDlg, IDC_EDIT_MINUTE, buf);
			switch (g_iAlarmSound)
			{
			case 0: CheckRadioButton(hDlg, IDC_RADIO_PCSPEAKER, IDC_RADIO_WAV, IDC_RADIO_PCSPEAKER); break;
			case 1: CheckRadioButton(hDlg, IDC_RADIO_PCSPEAKER, IDC_RADIO_WAV, IDC_RADIO_SYSTEMBEEP); break;
			case 2: CheckRadioButton(hDlg, IDC_RADIO_PCSPEAKER, IDC_RADIO_WAV, IDC_RADIO_WAV); break;
			}

			// Show path
			SetDlgItemText(hDlg, IDC_EDIT_WAVPATH, g_szAlarmWav);

			// Turn on/off browse button
			EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), g_iAlarmSound == 2);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_WAVPATH), g_iAlarmSound == 2);
			SetFocus(GetDlgItem(hDlg, IDC_EDIT_HOUR));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
				char buf[8];

				// Read hour
				GetDlgItemText(hDlg, IDC_EDIT_HOUR, buf, sizeof(buf));
				int hour = atoi(buf);

				// Read minute
				GetDlgItemText(hDlg, IDC_EDIT_MINUTE, buf, sizeof(buf));
				int minute = atoi(buf);

				// Validation
				if (hour < 0 || hour > 23)
				{
					MessageBox(hDlg, "Hour must be 0-23", "Invalid value",
						MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hDlg, IDC_EDIT_HOUR));
					return TRUE;
				}
				if (minute < 0 || minute > 59)
				{
					MessageBox(hDlg, "Minute must be 0-59", "Invalid value",
						MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hDlg, IDC_EDIT_MINUTE));
					return TRUE;
				}

				// Save sound
				if (IsDlgButtonChecked(hDlg, IDC_RADIO_PCSPEAKER) == BST_CHECKED)
					g_iAlarmSound = 0;
				else if (IsDlgButtonChecked(hDlg, IDC_RADIO_SYSTEMBEEP) == BST_CHECKED)
					g_iAlarmSound = 1;
				else
					g_iAlarmSound = 2;

				//Check that WAV selected if radio = WAV
				if (g_iAlarmSound == 2 && g_szAlarmWav[0] == '\0')
				{
					MessageBox(hDlg, "Please select a WAV file", "Invalid",
						MB_OK | MB_ICONWARNING);
					return TRUE;
				}

				// Read path
				GetDlgItemText(hDlg, IDC_EDIT_WAVPATH, g_szAlarmWav, MAX_PATH);

				//Save
				g_iDlgHour = hour;
				g_iDlgMinute = minute;

				EndDialog(hDlg, IDOK);
				return TRUE;
			}

			case IDCANCEL:
				EndDialog(hDlg, IDCANCEL);
				return TRUE;

			case IDC_RADIO_PCSPEAKER:
			case IDC_RADIO_SYSTEMBEEP:
			case IDC_RADIO_WAV:
			{
				// Define what radio is selected
				BOOL isWav = (IsDlgButtonChecked(hDlg, IDC_RADIO_WAV) == BST_CHECKED);

				// Turn on or off Browse and Edit
				EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), isWav);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_WAVPATH), isWav);
				return TRUE;
			}


			case IDC_BUTTON_BROWSE:
			{
				OPENFILENAME ofn;
				char szFile[MAX_PATH] = {0};
				lstrcpy(szFile, g_szAlarmWav);

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn );
				ofn.hwndOwner	= hDlg;
				ofn.lpstrFilter	= "WAV Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
				ofn.lpstrFile	= szFile;
				ofn.nMaxFile	= MAX_PATH;
				ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

				if (GetOpenFileName(&ofn))
				{
					lstrcpy(g_szAlarmWav, szFile);
					SetDlgItemText(hDlg, IDC_EDIT_WAVPATH, g_szAlarmWav);
				}
				return TRUE;
			}
			}
			break;
	}

	return FALSE;
}

void Alarm_ShowDialog(HWND hwnd)
{

	// Prepare values
	if (Alarm_IsSet())
	{
		g_iDlgHour = g_iAlarmHour;
		g_iDlgMinute = g_iAlarmMinute;
	}
	else
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		g_iDlgHour = st.wHour;
		g_iDlgMinute = st.wMinute;
	}

	// Open dialog
	INT_PTR result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ALARM), hwnd, AlarmDlgProc);

	if (result == -1)
	{
		DWORD err = GetLastError();
		char buf[128];
		wsprintf(buf, "Dialogbox failed!\n\nError code: %d\n\n", err);
		MessageBox(NULL, buf, "Error", MB_OK | MB_ICONERROR);
		return;
	}

	if (result == IDOK)
	{
		// OK pressed - save
		g_iAlarmHour = g_iDlgHour;
		g_iAlarmMinute = g_iDlgMinute;
		Alarm_Save();
	}
}

BOOL Alarm_IsRinging()
{
	return g_bAlarmRinging;
}

BOOL Alarm_IsSet()
{
	return (g_iAlarmHour >= 0 && g_iAlarmMinute >= 0);
}

void Alarm_Clear()
{
	g_iAlarmHour = -1;
	g_iAlarmMinute = -1;
	Alarm_Save();
}

void Alarm_PlaySound(BOOL firstTime)
{
	switch (g_iAlarmSound)
	{
	case 0: Beep(880, 300); break;
	case 1: MessageBeep(MB_ICONEXCLAMATION); break;
	case 2:
		if (firstTime && g_szAlarmWav[0] != '\0')
			PlaySound(g_szAlarmWav, NULL, SND_ASYNC | SND_FILENAME | SND_LOOP);
		break;
	}
}