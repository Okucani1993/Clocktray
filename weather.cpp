#include "weather.h"
#include <wininet.h>
#include <stdlib.h>
#include "resource.h"

#define WEATHER_TIMEOUT 5000
#define WEATHER_KEY "Software\\Clocktray"

// Own city

const char* Weather_GetCustomCity()
{
	static char city[128] = {0};
	city[0] = '\0';

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, WEATHER_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return city;

	DWORD size = sizeof(city);
	DWORD type = 0;
	RegQueryValueEx(hKey, "WeatherCity", NULL, &type, (LPBYTE)city, &size);
	RegCloseKey(hKey);

	city[sizeof(city) - 1] = '\0';
	return city;
}

void Weather_SetCustomCity(const char* city)
{
	HKEY hKey;
	DWORD disp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, WEATHER_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
		return;

	if (city && city[0])
	{
		RegSetValueEx(hKey, "WeatherCity", 0, REG_SZ, (const BYTE*)city, lstrlen(city) + 1);
	}
	else
	{
		RegDeleteValue(hKey, "WeatherCity");
	}
	RegCloseKey(hKey);
}

// City enter dialog

static BOOL CALLBACK CityDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
	{
		const char* current= (const char*)lParam;
		if (current && current[0])
			SetDlgItemText(hDlg, IDC_EDIT_CITY, current);

		SetFocus(GetDlgItem(hDlg, IDC_EDIT_CITY));
		return FALSE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				char buf[128] = {0};
				GetDlgItemText(hDlg, IDC_EDIT_CITY, buf, sizeof(buf));

				if (buf[0] == '\0')
				{
					MessageBox(hDlg, "Please enter a city name", "Invalid", MB_OK | MB_ICONWARNING);
					return TRUE;
				}

				while (buf[0] == ' ') lstrcpy(buf, buf + 1);

				Weather_SetCustomCity(buf);
				EndDialog(hDlg, IDOK);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

BOOL Weather_ShowCityDialog(HWND hwnd)
{
	const char* current = Weather_GetCustomCity();
	INT_PTR result = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CITY), hwnd, CityDlgProc, (LPARAM)current);

	return (result == IDOK);
}

void Utf8ToAnsi(const char* src, char* dst, int dstSize)
{
    if (!src || !dst || dstSize <= 0) return;
    dst[0] = '\0';

    int wlen = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (wlen <= 0)
    {
        lstrcpyn(dst, src, dstSize);
        return;
    }

    WCHAR* wbuf = (WCHAR*)malloc(wlen * sizeof(WCHAR));
    if (!wbuf)
    {
        lstrcpyn(dst, src, dstSize);
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, src, -1, wbuf, wlen);

    int alen = WideCharToMultiByte(CP_ACP, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (alen > 0 && alen <= dstSize)
        WideCharToMultiByte(CP_ACP, 0, wbuf, -1, dst, dstSize, NULL, NULL);
    else
    {
        WideCharToMultiByte(CP_ACP, 0, wbuf, -1, dst, dstSize - 1, NULL, NULL);
        dst[dstSize - 1] = '\0';
    }

    free(wbuf);
}

BOOL Weather_Fetch(const char* city, char* outBuffer, int outSize)
{
    if (!city || !outBuffer || outSize <= 0) return FALSE;
    outBuffer[0] = '\0';

    HINTERNET hNet = InternetOpen("Clocktray/1.1",
                                   INTERNET_OPEN_TYPE_PRECONFIG,
                                   NULL, NULL, 0);
    if (!hNet) return FALSE;

    DWORD timeout = WEATHER_TIMEOUT;
    InternetSetOption(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOption(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    char url[512];
    wsprintf(url,
        "http://wttr.in/%s?m&format="
        "City:%%20%%l%%0A"
        "Condition:%%20%%C%%0A"
        "Temperature:%%20%%t%%0A"
        "Feels+like:%%20%%f%%0A"
        "Humidity:%%20%%h%%0A"
        "Pressure:%%20%%P%%0A"
        "Sunrise:%%20%%S%%0A"
        "Sunset:%%20%%s%%0A"
        "Day+length:%%20%%D",
        city);

    HINTERNET hUrl = InternetOpenUrl(hNet, url, NULL, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl)
    {
        InternetCloseHandle(hNet);
        return FALSE;
    }

    char utf8buf[1024];
    DWORD bytesRead = 0;
    int total = 0;

    while (InternetReadFile(hUrl, utf8buf + total,
                            sizeof(utf8buf) - 1 - total, &bytesRead)
           && bytesRead > 0)
    {
        total += bytesRead;
        if (total >= (int)sizeof(utf8buf) - 1) break;
    }
    utf8buf[total] = '\0';

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);

    // ?????? trailing whitespace
    while (total > 0 && (utf8buf[total-1] == '\n' ||
                         utf8buf[total-1] == '\r' ||
                         utf8buf[total-1] == ' '))
    {
        utf8buf[--total] = '\0';
    }

    Utf8ToAnsi(utf8buf, outBuffer, outSize);

    return (outBuffer[0] != '\0');
}

void Weather_ShowMessage(HWND hwnd, const char* city)
{
    char buffer[512];
    if (!Weather_Fetch(city, buffer, sizeof(buffer)))
    {
        MessageBox(hwnd,
                    "Failed to fetch weather.\n\n"
                    "Check your internet connection.",
                    "Weather",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }

    char msg[768];
    wsprintf(msg, "Weather for %s:\n\n%s", city, buffer);

    MessageBox(hwnd, msg, "Weather", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

BOOL Weather_FetchShort(const char* city, char* outBuffer, int outSize)
{
	if (!city || !outBuffer || outSize <= 0) return FALSE;
	outBuffer[0] = '\0';

	HINTERNET hNet = InternetOpen("Clocktray/1.2", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hNet) return FALSE;

	DWORD timeout = WEATHER_TIMEOUT;
	InternetSetOption(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

	char url[256];
	wsprintf(url, "http://wttr.in/%s?m&format=%%t+%%C", city);

	HINTERNET hUrl = InternetOpenUrl(hNet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
	if (!hUrl)
	{
		InternetCloseHandle(hNet);
		return FALSE;
	}

	char utf8buf[256];
	DWORD bytesRead = 0;
	int total = 0;

	while (InternetReadFile(hUrl, utf8buf + total, sizeof(utf8buf) - 1 - total, &bytesRead) && bytesRead > 0)
	{
		total += bytesRead;
		if (total >= (int)sizeof(utf8buf) - 1) break;
	}
	utf8buf[total] = '\0';

	InternetCloseHandle(hUrl);
	InternetCloseHandle(hNet);

	while (total > 0 && (utf8buf[total-1] == '\n' || utf8buf[total-1] == '\r' || utf8buf[total-1] == ' '))
		utf8buf[--total] = '\0';

	Utf8ToAnsi(utf8buf, outBuffer, outSize);

	return (outBuffer[0] != '\0');
}