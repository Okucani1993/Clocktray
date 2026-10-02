#include "currency.h"
#include <wininet.h>
#include <stdlib.h>

#define CURRENCY_TIMEOUT 5000
#define CURRENCY_KEY	"Software\\Clocktray"

int Currency_GetSource()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, CURRENCY_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return CURRENCY_SRC_CBR;

	DWORD value = CURRENCY_SRC_CBR;
	DWORD size = sizeof(DWORD);
	DWORD type = 0;
	RegQueryValueEx(hKey, "CurrencySource", NULL, &type, (LPBYTE)&value, &size);
	RegCloseKey(hKey);

	return (int)value;
}

void Currency_SetSource(int src)
{
	HKEY hKey;
	DWORD disp;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, CURRENCY_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
		return;


	DWORD value = (DWORD)src;
	RegSetValueEx (hKey, "CurrencySource", 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
	RegCloseKey(hKey);
}

static BOOL HttpGet(const char* url, char* outBuffer, int outSize)
{
	if (!url || !outBuffer || outSize <= 0) return FALSE;
	outBuffer[0] = '\0';

	HINTERNET hNet = InternetOpen("Clocktray/1.1", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

	if(!hNet) return FALSE;

	DWORD timeout = CURRENCY_TIMEOUT;
	InternetSetOption(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

	HINTERNET hUrl = InternetOpenUrl(hNet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
	if (!hUrl)
	{
		InternetCloseHandle(hNet);
		return FALSE;
	}

	DWORD bytesRead = 0;
	int total = 0;

	while (InternetReadFile(hUrl, outBuffer + total, outSize - 1 - total, &bytesRead) && bytesRead > 0)
	{
		total += bytesRead;
		if (total >= outSize - 1) break;
	}
	outBuffer[total] = '\0';

	InternetCloseHandle(hUrl);
	InternetCloseHandle(hNet);

	return (total > 0);
}

BOOL Currency_FetchCBR(const char* currency, char* outBuffer, int outSize)
{
	if (!currency || !outBuffer || outSize <= 0) return FALSE;
	outBuffer[0] = '\0';

	// Big buffer for XML
	char* xmlBuf = (char*)malloc(65536);
	if (!xmlBuf) return FALSE;

	if (!HttpGet("http://www.cbr.ru/scripts/XML_Daily.asp", xmlBuf, 65536))
	{
		free(xmlBuf);
		return FALSE;
	}

	// Find <charcode>USD</charcode>
	char pattern[64];
	wsprintf(pattern, "<CharCode>%s</CharCode>", currency);

	const char* p = strstr(xmlBuf, pattern);
	if (!p)
	{
		free(xmlBuf);
		return FALSE;
	}

	// Find <value> after
	p = strstr(p, "<Value>");
	if (!p)
	{
		free(xmlBuf);
		return FALSE;
	}

	p += 7; // Skip "<Value>"

	// Copy to </value>
	int i = 0;
	while (*p && *p != '<' && i < outSize - 1)
		outBuffer[i++] = *p++;
	outBuffer[i] = '\0';

	free(xmlBuf);
	return (outBuffer[0] != '\0');
}

// floatrates

//JSON parsing
static BOOL ParseFloatratesJson(const char* json, const char* currency, char* outBuffer, int outSize)
{
	if (!json || !currency || !outBuffer || outSize <= 0) return FALSE;
	outBuffer[0] = '\0';

	// Searching "code"
	char pattern[64];
	wsprintf(pattern, "\"code\": \"%s\"", currency);

	const char* p = strstr(json, pattern);
	if (!p) return FALSE;

	// Searching "rate"
	p = strstr(p, "\"rate\":");
	if (!p) return FALSE;

	p += 7; // Skip "rate":

	// Skip spaces
	while (*p == ' ' || *p == '\t') p++;

	// Copy number to , or }
	int i = 0;
	while (*p && *p != ',' && *p != '}' && *p != '\n' && *p != ' ' && i < outSize - 1)
		outBuffer[i++] = *p++;
	outBuffer[i] = '\0';

	return (outBuffer[0] != '\0');
}

static BOOL Currency_FetchNBP(const char* currency, char* outBuffer, int outSize)
{
	if (!currency || !outBuffer || outSize <= 0) return FALSE;
	outBuffer[0] = '\0';

	//Big buffer for JSON
	char* jsonBuf = (char*)malloc(65536);
	if (!jsonBuf) return FALSE;

	const char* url = "http://api.nbp.pl/api/exchangerates/tables/A/?format=json";

	if (!HttpGet(url, jsonBuf, 65536))
	{
		free(jsonBuf);
		return FALSE;
	}

	char pattern[64];
	wsprintf(pattern, "\"code\": \"%s\"", currency);

	const char* p = strstr(jsonBuf, pattern);
	if (!p)
	{
		wsprintf(pattern, "\"code\":\"%s\"", currency);
		p = strstr(jsonBuf, pattern);
	}
	if (!p)
	{
		free(jsonBuf);
		return FALSE;
	}

	p = strstr(p, "\"mid\":");
	if (!p)
	{
		free(jsonBuf);
		return FALSE;
	}
	p += 6;

	while (*p == ' ' || *p == '\t') p++;

	int i = 0;
	while (*p && *p != ',' && *p != '}' && *p != '\n' && *p != ' ' && i < outSize - 1)
		outBuffer[i++] = *p++;
	outBuffer[i] = '\0';

	free(jsonBuf);
	return (outBuffer[0] != '\0');
}

// Public function
BOOL Currency_Fetch(const char* currency, char* outBuffer, int outSize)
{
	int src = Currency_GetSource();
	if (src == CURRENCY_SRC_NBP)
		return Currency_FetchNBP(currency, outBuffer, outSize);
	return Currency_FetchCBR(currency, outBuffer, outSize);
}

void Currency_ShowMessage(HWND hwnd)
{
	int src = Currency_GetSource();
	const char* srcName = (src == CURRENCY_SRC_NBP) ? "NBP (PLN base)" : "CBR RF (RUB base)";
	const char* baseName = (src == CURRENCY_SRC_NBP) ? "PLN" : "RUB";

	// Currencies for displaying'
	const char* currencies[4];
	if (src == CURRENCY_SRC_NBP)
	{
		currencies[0] = "USD";
		currencies[1] = "EUR";
		currencies[2] = "GBP";
		currencies[3] = "CHF";
	}
	else
	{
		currencies[0] = "USD";
		currencies[1] = "EUR";
		currencies[2] = "CNY";
		currencies[3] = "GBP";
	}

	char msg[1024] = {0};
	int pos = 0;

	pos += wsprintf(msg + pos, "Exchange rates \nSource: %s\n\n", srcName);

	for (int i = 0; i < 4; i++)
{
    char value[32] = {0};
    if (Currency_Fetch(currencies[i], value, sizeof(value)))
    {
        if (src == CURRENCY_SRC_NBP)
        {
            pos += wsprintf(msg + pos, "1 %s = %s PLN\n",
                            currencies[i], value);
        }
        else
        {
            pos += wsprintf(msg + pos, "1 %s = %s RUB\n",
                            currencies[i], value);
        }
    }
    else
    {
        pos += wsprintf(msg + pos, "%s: n/a\n", currencies[i]);
    }
}

	MessageBox(hwnd, msg, "Currency", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}
