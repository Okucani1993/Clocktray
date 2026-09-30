#ifndef ALARM_H
#define ALARM_H

#include <windows.h>

//Loading and saving alarm from registry
void Alarm_Load();
void Alarm_Save();

//  Alarm check
// Return TRUE if alarm works
BOOL Alarm_Check();

BOOL Alarm_IsRinging();

// Work with status
BOOL Alarm_IsSet();
void Alarm_Clear();

//Settings dialog
void Alarm_ShowDialog(HWND hwnd);

// Beeping
void Alarm_StartBeeping(HWND hwnd);
void Alarm_StopBeeping(HWND hwnd);

void Alarm_PlaySound(BOOL firstTime);

#endif