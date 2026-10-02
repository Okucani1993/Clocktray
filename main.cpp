#include <windows.h>
#include <stdio.h>
#include "clockicon.h"
#include "resource.h"
#include "settings.h"
#include "alarm.h"
#include "worldclock.h"
#include "weather.h"
#include "currency.h"

#ifndef NIF_INFO
#define NIF_INFO 0x00000010
#endif

#ifndef NIIF_INFO
#define	NIIF_INFO 0x00000001
#endif

#ifndef NIM_SETVERSION
#define NIM_SETVERSION 0x00000004
#endif
#ifndef NOTIFYICON_VERSION
#define NOTIFYICON_VERSION 3
#endif

#define BASE_FLAGS (NIF_ICON | NIF_MESSAGE | NIF_TIP)
#define BALLOON_FLAGS (BASE_FLAGS | NIF_INFO)

typedef struct _NOTIFYICONDATA_V2 {
	DWORD cbSize;
	HWND hWnd;
	UINT uID;
	UINT uFlags;
	UINT uCallbackMessage;
	HICON hIcon;
	char szTip[128];
	DWORD dwState;
	DWORD dwStateMask;
	char szInfo[256];
	union {
		UINT uTimeout;
		UINT uVersion;
	};
	char szInfoTitle[64];
	DWORD dwInfoFlags;
	GUID guidItem;
	HICON hBalloonIcon;
} NOTIFYICONDATA_V2;

#define WM_TRAYICON				(WM_USER + 1)
#define IDT_TIMER				1
#define IDM_SHOWTIME			1001
#define IDM_ABOUT				1002
#define	IDM_EXIT				1003
#define IDT_BALLOON				2
#define IDM_INT_1MIN			2001
#define IDM_INT_5MIN			2002
#define IDM_INT_15MIN			2003
#define	IDM_INT_1HOUR			2004
#define IDM_INT_NEVER			2005
#define	BALLOON_1MIN			60000
#define BALLOON_5MIN			300000
#define BALLOON_15MIN			900000
#define BALLOON_1HOUR			3600000
#define BALLOON_NEVER			0xFFFFFFFF
#define IDM_AUTORUN				3001
#define IDM_SHOWSECONDS			3002
#define IDT_ALARM_BEEP			3
#define IDM_ALARM_SET			4001
#define IDM_ALARM_CLEAR			4002
#define IDM_WORLDCLOCK_BASE		5000
#define IDM_WEATHER_BASE		6000
#define IDM_CURRENCY_CBR		7001
#define IDM_CURRENCY_NBP		7002
#define IDM_WEATHER_CUSTOM		6999

#define AUTORUN_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTORUN_NAME "Clocktray"
#define SETTINGS_KEY "Software\\Clocktray"

UINT g_uBalloonInterval = BALLOON_1HOUR;

NOTIFYICONDATA_V2 g_nid;
HWND g_hwnd = NULL;
BOOL g_bBalloonEnabled = TRUE;
BOOL g_bShowSeconds = TRUE;

BOOL IsAutorunEnabled()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return FALSE;

	char buf[MAX_PATH] = {0};
	DWORD size = sizeof(buf);
	DWORD type = 0;
	LONG result = RegQueryValueEx(hKey, AUTORUN_NAME, NULL, &type, (LPBYTE)buf, &size);
	RegCloseKey(hKey);

	return (result == ERROR_SUCCESS);
}

BOOL IsSafePath(const char* path)
{
	char lower[MAX_PATH];
	lstrcpy(lower, path);
	CharLower(lower);

	if (strstr(lower, "\\temp\\") != NULL) return FALSE;
	if (strstr(lower, "\\tmp\\") != NULL) return FALSE;
	if (strstr(lower, "\\windows\\temporary") != NULL) return FALSE;

	return TRUE;
}

BOOL SetAutorun(BOOL enable)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return FALSE;

	LONG result;
	if (enable)
	{
		char path[MAX_PATH] = {0};
		GetModuleFileName(NULL, path, MAX_PATH);

		if (!IsSafePath(path))
		{
			MessageBox(NULL, "Unable to autorun: Clocktray run from temporary foler.",
				"Clocktray", MB_OK | MB_ICONWARNING);
			return FALSE;
		}
		result = RegSetValueEx(hKey, AUTORUN_NAME, 0, REG_SZ, (const BYTE*)path, lstrlen(path) + 1);
	}
	else
	{
		result = RegDeleteValue(hKey, AUTORUN_NAME);
		if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
	}

	RegCloseKey(hKey);
	return (result == ERROR_SUCCESS);
}

BOOL IsFirstRun()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return TRUE;

	RegCloseKey(hKey);
	return FALSE;
}

void MarkAsRun()
{
	HKEY hKey;
	DWORD disp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
	}
}


void ShowAboutMessage(HWND hwnd)
{
	MessageBox(hwnd,
		"Clocktray 1.2\n"
		"\n"
		"Clock in system tray\n"
		"\n"
		"Written by Immamalware",
		"About",
		MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

void ShowDateTimeMessage(HWND hwnd)
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	char dateBuf[128]	= {0};
	char timeBuf[64]	= {0};
	char msgBuf[256]	= {0};

	//Date
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, dateBuf, sizeof(dateBuf));

	//Time
	GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, &st, "HH:mm:ss", timeBuf, sizeof(timeBuf));

	wsprintf(msgBuf, "%s\n%s", dateBuf, timeBuf);

	MessageBox(hwnd, msgBuf, "Date and time", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

void ShowBalloonTip(HWND hwnd)
{
	if (!g_bBalloonEnabled) return;

	SYSTEMTIME st;
	GetLocalTime(&st);

	char timeBuf[64] = {0};
	char dateBuf[64] = {0};

	GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, &st, "HH:mm:ss", timeBuf, sizeof(timeBuf));
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, dateBuf, sizeof(dateBuf));

	lstrcpy(g_nid.szInfoTitle, "Clocktray");
	wsprintf(g_nid.szInfo, "%s\n%s", timeBuf, dateBuf);

	g_nid.dwInfoFlags = NIIF_INFO;
	g_nid.uTimeout = 10000;

	g_nid.uFlags = BALLOON_FLAGS;
	Shell_NotifyIcon(NIM_MODIFY, (NOTIFYICONDATA*)&g_nid);

	g_nid.uFlags = BASE_FLAGS;
	g_nid.szInfo[0] = '\0';
	g_nid.dwInfoFlags = 0;
}

void UpdateTrayTime(HWND hwnd)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char buf[64];
	wsprintf(buf, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

	//Date
	char tip[128];
	wsprintf(tip, "Clock: %s", buf);
	lstrcpy(g_nid.szTip, tip);

	//New icon
	HICON hNew = CreateClockIcon(16);// 16 = tray size
	if (hNew)
	{
		HICON hOld = g_nid.hIcon;
		g_nid.hIcon = hNew;
		g_nid.uFlags = BASE_FLAGS;
		Shell_NotifyIcon(NIM_MODIFY, (NOTIFYICONDATA*)&g_nid);
		if (hOld) DestroyIcon(hOld);
	}
	else
	{
		Shell_NotifyIcon(NIM_MODIFY, (NOTIFYICONDATA*)&g_nid);
	}
}

void ApplyBalloonInterval(HWND hwnd, UINT newInterval)
{
	KillTimer(hwnd, IDT_BALLOON);

	g_uBalloonInterval = newInterval;

	if (newInterval != BALLOON_NEVER)
	{
		SetTimer(hwnd, IDT_BALLOON, newInterval, NULL);
		g_bBalloonEnabled = TRUE;
		ShowBalloonTip(hwnd);
	}
	else
	{
		g_bBalloonEnabled = FALSE;
	}

	Settings_Save();

}

static const char* g_weatherCities[] =
{
	"Moscow",
	"London",
	"New York",
	"Tokyo",
	"Warsaw",
};
static const int g_weatherCityCount = sizeof(g_weatherCities) / sizeof(g_weatherCities[0]);

static const char* GetGreeting(int hour)
{
	if (hour >= 5 && hour < 12) return "Good morning";
	if (hour >= 12 && hour < 18) return "Good afternoon";
	if (hour >= 18 && hour < 23) return "Good evening";
	return "Good night";
}

static DWORD WINAPI WelcomeBalloonThread(LPVOID lpParam)
{
	HWND hwnd = (HWND)lpParam;

	Sleep(2000);

	SYSTEMTIME st;
	GetLocalTime(&st);

	const char* greeting = GetGreeting(st.wHour);

	const char* city = Weather_GetCustomCity();
	if (!city[0]) city = "New York";

	char weather[128] = {0};
	if (!Weather_FetchShort(city, weather, sizeof(weather)))
		lstrcpy(weather, "weather n/a");

	char usd[32] = {0}, eur[32] = {0};
	Currency_Fetch("USD", usd, sizeof(usd));
	Currency_Fetch("EUR", eur, sizeof(eur));

	char title[64];
	wsprintf(title, "Clocktray - %s", greeting);

	char info[256];
	wsprintf(info,
		"%s %s\n"
		"USD %s | EUR %s\n"
		"%02d:%02d, %02d.%02d.%04d",
		city, weather,
		usd[0] ? usd : "n/a",
		eur[0] ? eur : "n/a",
		st.wHour, st.wMinute,
		st.wDay, st.wMonth, st.wYear);

	lstrcpy(g_nid.szInfoTitle, title);
	lstrcpy(g_nid.szInfo, info);
	g_nid.dwInfoFlags = NIIF_INFO;
	g_nid.uTimeout = 15000;
	g_nid.uFlags = BALLOON_FLAGS;
	Shell_NotifyIcon(NIM_MODIFY, (NOTIFYICONDATA*)&g_nid);

	g_nid.uFlags = BASE_FLAGS;
	g_nid.szInfo[0] = '\0';
	g_nid.dwInfoFlags = 0;

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_TRAYICON:
		if (lParam == WM_LBUTTONUP)
		{
			if (Alarm_IsRinging())
			{
				Alarm_StopBeeping(hwnd);
			}
			else
			{
				ShowDateTimeMessage(hwnd);
			}
		}
		else if (lParam == WM_RBUTTONUP)
		{
			// Menu
			POINT pt;
			GetCursorPos(&pt);
			HMENU hMenu = CreatePopupMenu();
			HMENU hSubMenu = CreatePopupMenu();

			// Intervals submenu
			AppendMenu(hSubMenu, MF_STRING | (g_uBalloonInterval == BALLOON_1MIN ? MF_CHECKED : 0),
				IDM_INT_1MIN, "Every minute");
			AppendMenu(hSubMenu, MF_STRING | (g_uBalloonInterval == BALLOON_5MIN ? MF_CHECKED : 0),
				IDM_INT_5MIN, "Every 5 minutes");
			AppendMenu(hSubMenu, MF_STRING | (g_uBalloonInterval == BALLOON_15MIN ? MF_CHECKED : 0),
				IDM_INT_15MIN, "Every 15 minutes");

			AppendMenu(hSubMenu, MF_STRING | (g_uBalloonInterval == BALLOON_1HOUR ? MF_CHECKED : 0),
				IDM_INT_1HOUR, "Every hour");
			AppendMenu(hSubMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hSubMenu, MF_STRING | (g_uBalloonInterval == BALLOON_NEVER ? MF_CHECKED : 0),
				IDM_INT_NEVER, "Never");

			// Main Menu
			AppendMenu(hMenu, MF_STRING, IDM_SHOWTIME, "Show date and time");

			UINT secondsFlag = MF_STRING;
			if (g_bShowSeconds) secondsFlag |= MF_CHECKED;
			AppendMenu(hMenu, secondsFlag, IDM_SHOWSECONDS, "Show seconds");

			HMENU hWorldMenu = CreatePopupMenu();
			WorldClock_AddToMenu(hWorldMenu, IDM_WORLDCLOCK_BASE);
			AppendMenu(hMenu, MF_POPUP, (UINT)hWorldMenu, "World clock");

			HMENU hWeatherMenu = CreatePopupMenu();
			for (int i = 0; i < g_weatherCityCount; i++)
			{
				AppendMenu(hWeatherMenu, MF_STRING, IDM_WEATHER_BASE + i, g_weatherCities[i]);
			}

			const char* customCity = Weather_GetCustomCity();
			if (customCity[0])
			{
				AppendMenu(hWeatherMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hWeatherMenu, MF_STRING, IDM_WEATHER_CUSTOM + 1, customCity);
			}

			AppendMenu(hWeatherMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hWeatherMenu, MF_STRING, IDM_WEATHER_CUSTOM, "Set custom city...");

			AppendMenu(hMenu, MF_POPUP, (UINT)hWeatherMenu, "Weather");

			HMENU hCurrencyMenu = CreatePopupMenu();
			AppendMenu(hCurrencyMenu, MF_STRING | (Currency_GetSource() == CURRENCY_SRC_CBR ? MF_CHECKED : 0),
				IDM_CURRENCY_CBR, "Russian ruble (CBR)");
			AppendMenu(hCurrencyMenu, MF_STRING | (Currency_GetSource() == CURRENCY_SRC_NBP ? MF_CHECKED : 0),
				IDM_CURRENCY_NBP, "Polish zloty (NBP)");
			AppendMenu(hMenu, MF_POPUP, (UINT)hCurrencyMenu, "Currency rates");

			AppendMenu(hMenu, MF_STRING, IDM_ALARM_SET, "Set alarm..");

			//only if alarm is set
			UINT clearFlag = MF_STRING;
			if (!Alarm_IsSet()) clearFlag |= MF_GRAYED;
			AppendMenu(hMenu, clearFlag, IDM_ALARM_CLEAR, "Clear alarm");

			UINT balloonFlag = MF_POPUP;
			if (g_bBalloonEnabled) balloonFlag |= MF_CHECKED;
			AppendMenu(hMenu, balloonFlag, (UINT)hSubMenu, "Show balloon tip");

			UINT autorunFlag = MF_STRING;
			if (IsAutorunEnabled()) autorunFlag |= MF_CHECKED;
			AppendMenu(hMenu, autorunFlag, IDM_AUTORUN, "Run on startup");

			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MF_STRING, IDM_ABOUT, "About");
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MF_STRING, IDM_EXIT, "Exit");

			SetForegroundWindow(hwnd);
			TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL); 
			DestroyMenu(hMenu);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_SHOWTIME:
			ShowDateTimeMessage(hwnd);
			break;

		case IDM_ABOUT:
			ShowAboutMessage(hwnd);
			break;

		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;

		case IDM_AUTORUN:
			if (IsAutorunEnabled())
				SetAutorun(FALSE);
			else
				SetAutorun(TRUE);

		case IDM_SHOWSECONDS:
			g_bShowSeconds = !g_bShowSeconds;
			Settings_Save();
			UpdateTrayTime(hwnd);
			break;

		case IDM_ALARM_SET:
			Alarm_ShowDialog(hwnd);
			break;

		case IDM_ALARM_CLEAR:
			Alarm_Clear();
			Alarm_StopBeeping(hwnd);
			break;

		case IDM_CURRENCY_CBR:
			Currency_SetSource(CURRENCY_SRC_CBR);
			Currency_ShowMessage(hwnd);
			break;

		case IDM_CURRENCY_NBP:
			Currency_SetSource(CURRENCY_SRC_NBP);
			Currency_ShowMessage(hwnd);
			break;

		default:
			// World clock
			if (LOWORD(wParam) >= IDM_WORLDCLOCK_BASE &&
				LOWORD (wParam) < IDM_WORLDCLOCK_BASE + WorldClock_GetCount())
			{
				int cityIndex = LOWORD(wParam) - IDM_WORLDCLOCK_BASE;
				WorldClock_Show(cityIndex);
			}
			// Weather
			else if (LOWORD(wParam) >= IDM_WEATHER_BASE && LOWORD(wParam) < IDM_WEATHER_BASE + g_weatherCityCount)
			{
				int cityIndex = LOWORD(wParam) - IDM_WEATHER_BASE;
				Weather_ShowMessage(hwnd, g_weatherCities[cityIndex]);
			}
			else if (LOWORD(wParam) == IDM_WEATHER_CUSTOM + 1)
			{
				const char* city = Weather_GetCustomCity();
				if (city[0])
					Weather_ShowMessage(hwnd, city);
			}
			else if (LOWORD(wParam) == IDM_WEATHER_CUSTOM)
			{
				Weather_ShowCityDialog(hwnd);
			}
			break;

		case IDM_INT_1MIN: ApplyBalloonInterval(hwnd, BALLOON_1MIN); break;
		case IDM_INT_5MIN: ApplyBalloonInterval(hwnd, BALLOON_5MIN); break;
		case IDM_INT_15MIN: ApplyBalloonInterval(hwnd, BALLOON_15MIN); break;
		case IDM_INT_1HOUR: ApplyBalloonInterval(hwnd, BALLOON_1HOUR); break;
		case IDM_INT_NEVER: ApplyBalloonInterval(hwnd, BALLOON_NEVER); break;
		}
		break;

	case WM_TIMER:
		if (wParam == IDT_TIMER)
		{
			UpdateTrayTime(hwnd);
			if (Alarm_Check())
			{
				Alarm_StartBeeping(hwnd);
				ShowBalloonTip(hwnd); // optional - show balloon
			}
		}
		else if (wParam == IDT_ALARM_BEEP)
		{
			Alarm_PlaySound(FALSE);
		}

		else if (wParam == IDT_BALLOON)
		{
			ShowBalloonTip(hwnd);
		}
		break;

	case WM_DESTROY:
		KillTimer(hwnd, IDT_TIMER);
		KillTimer(hwnd, IDT_BALLOON);
		Shell_NotifyIcon(NIM_DELETE, (NOTIFYICONDATA*)&g_nid);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	WNDCLASS wc = {0};
	wc.lpfnWndProc		= WndProc;
	wc.hInstance		= hInst;
	wc.lpszClassName	= "TrayClockClass";
	wc.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_CLOCKICON));
	RegisterClass(&wc);

	g_hwnd = CreateWindow("TrayClockClass", "TrayClock", 0, 0, 0, 0, 0, NULL, NULL, hInst, NULL);

	Settings_Load();
	Alarm_Load();

	ZeroMemory(&g_nid, sizeof(g_nid));
	g_nid.cbSize			= 504;
	g_nid.hWnd				= g_hwnd;
	g_nid.uID				= 1;
	g_nid.uFlags			= BASE_FLAGS;
	g_nid.uCallbackMessage	= WM_TRAYICON;
	g_nid.hIcon				= CreateClockIcon(16); //if NULL - fallback
	if (!g_nid.hIcon)
		g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	lstrcpy(g_nid.szTip, "Clock");
	Shell_NotifyIcon(NIM_ADD, (NOTIFYICONDATA*)&g_nid);

	if (IsFirstRun())
	{
		SetAutorun(TRUE);
		MarkAsRun();
	}

	SetTimer(g_hwnd, IDT_TIMER, 1000, NULL);
	SetTimer(g_hwnd, IDT_BALLOON, g_uBalloonInterval, NULL);
	UpdateTrayTime(g_hwnd);
	CreateThread(NULL, 0, WelcomeBalloonThread, (LPVOID)g_hwnd, 0, NULL);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
