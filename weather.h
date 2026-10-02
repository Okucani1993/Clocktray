#ifndef WEATHER_H
#define WEATHER_H

#include <windows.h>

// Get weather for city. return TRUE if success.
BOOL Weather_Fetch(const char* city, char* outBuffer, int outSize);

// Show weather in MessageBox
void Weather_ShowMessage(HWND hwnd, const char* city);

// Own city
const char* Weather_GetCustomCity();
void Weather_SetCustomCity(const char* city);

// Own city dialog
BOOL Weather_ShowCityDialog(HWND hwnd);

BOOL Weather_FetchShort(const char* city, char* outBuffer, int outSize);

#endif