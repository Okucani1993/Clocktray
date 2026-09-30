#ifndef WORLDCLOCK_H
#define WORLDCLOCK_H

#include <windows.h>

// Show in selected city (by index)
void WorldClock_Show(int cityIndex);

// Cities
int WorldClock_GetCount();

// Name city by index
const char* WorldClock_GetName(int cityIndex);

// Add city in menu
void WorldClock_AddToMenu(HMENU hMenu, UINT baseId);

#endif