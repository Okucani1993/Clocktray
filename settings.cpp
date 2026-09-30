#include "settings.h"

// Registry path for settings
#define SETTINGS_KEY "Software\\Clocktray"

// Default values
#define DEFAULT_INTERVAL 3600000 // 1 hour
#define DEFAULT_ENABLED	1
#define DEFAULT_SHOW_SECONDS 1

extern UINT g_uBalloonInterval;
extern BOOL g_bBalloonEnabled;
extern BOOL g_bShowSeconds;

void Settings_Load()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		// If no key - first run. Make defaults.
		g_uBalloonInterval = DEFAULT_INTERVAL;
		g_bBalloonEnabled = DEFAULT_ENABLED;
		g_bShowSeconds = DEFAULT_SHOW_SECONDS;
		return;
	}

	DWORD value = 0;
	DWORD size = sizeof(DWORD);
	DWORD type = 0;

	// BalloonInterval
	size = sizeof(DWORD);
	type = 0;
	if (RegQueryValueEx(hKey, "BalloonInterval", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		g_uBalloonInterval = value;
	}
	else
	{
		g_uBalloonInterval = DEFAULT_INTERVAL;
	}

	// BalloonEnabled
	size = sizeof(DWORD);
	type = 0;
	if (RegQueryValueEx(hKey, "BalloonEnabled", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		g_bBalloonEnabled = (value != 0) ? TRUE : FALSE;
	}
	else
	{
		g_bBalloonEnabled = DEFAULT_ENABLED;
	}

	size = sizeof(DWORD);
	type = 0;
	if (RegQueryValueEx(hKey, "ShowSeconds", NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		g_bShowSeconds = (value != 0) ? TRUE : FALSE;
	}
	else
	{
		g_bShowSeconds = (value != 0) ? TRUE : FALSE;
	}

	RegCloseKey(hKey);
}

void Settings_Save()
{
	HKEY hKey;
	DWORD disp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
	{
		return; // Couldn't open - abort
	}

	DWORD value;

	// BalloonInterval
	value = (DWORD)g_uBalloonInterval;
	RegSetValueEx(hKey, "BalloonInterval", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	// BalloonEnabled
	value = g_bBalloonEnabled ? 1 : 0;
	RegSetValueEx(hKey, "BalloonEnabled", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	//ShowSeconds
	value = g_bShowSeconds ? 1 : 0;
	RegSetValueEx(hKey, "ShowSeconds", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));

	RegCloseKey(hKey);
}
