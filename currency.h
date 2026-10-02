#ifndef CURRENCY_H
#define CURRENCY_H

#include <windows.h>

#define CURRENCY_SRC_CBR	0 // CBR RF - RUB, XML
#define CURRENCY_SRC_NBP	1 // NBP - PLN, JSON

// Get current source from registry
int Currency_GetSource();

// Save source in registry
void Currency_SetSource(int src);

// Show currencies in MessageBox (current source)
void Currency_ShowMessage(HWND hwnd);

BOOL Currency_Fetch(const char* currency, char* outBuffer, int outSize);

#endif

