#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>

// Loading from registry on startup.
// If no any key - default values
void Settings_Load();

// Save current settings in registry
void Settings_Save();

#endif