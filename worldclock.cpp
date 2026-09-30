#include "worldclock.h"

#define DST_NONE	0
#define DST_EU		1
#define DST_US		2
#define DST_AU		3

// Cities and their UTC offset (in hours)
struct WorldCity
{
	const char* name;
	int			standardOffset;
	int			dstOffset;
	int			dstRegion;
};

static WorldCity g_cities[] =
{
	{ "Moscow",			3,	3,	DST_NONE},
	{ "London",			0,	1,	DST_EU	},
	{ "Paris",			1,	2,	DST_EU	},
	{ "New York",		-5,	-4,	DST_US	},
	{ "Los Angeles",	-8,	-7,	DST_US	},
	{ "Tokyo",			9,	9,	DST_NONE},
	{ "Sydney",			10,	11,	DST_AU	},
	{ "Beijing",		8,	8,	DST_NONE},
	{ "Dubai",			4,	4,	DST_NONE},
};

static const int g_cityCount = sizeof(g_cities)  / sizeof(g_cities[0]);

// Day of week
static int DayOfWeek(int y, int m, int d)
{
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	if (m < 3) y -= 1;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

//Last sunday of month
static int LastSunday(int year, int month)
{
	// Last day of month
	static int daysInMonth[]  = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int lastDay = daysInMonth[month];
	// Leap year
	if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
		lastDay = 29;

	//Going back from last day
	while (DayOfWeek(year, month, lastDay) != 0)
		lastDay--;

	return lastDay;
}

// First sunday of month
static int FirstSunday(int year, int month)
{
	int day = 1;
	while (DayOfWeek(year, month, day) != 0)
		day++;
	return day;
}

// Second sunday of month
static int SecondSunday(int year, int month)
{
	return FirstSunday(year, month) + 7;
}

// Does DST active in this region for current date?
static BOOL IsDSTActive(int region)
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	int year = st.wYear;
	int month = st.wMonth;
	int day	= st.wDay;

	if (region == DST_EU)
	{
		int dstStart = LastSunday(year, 3); // March
		int dstEnd = LastSunday(year, 10);	// October
		if (month > 3 && month < 10) return TRUE;
		if (month == 3 && day >= dstStart) return TRUE;
		if (month == 10 && day < dstEnd) return TRUE;
		return FALSE;
	}
	else if (region == DST_US)
	{
		int dstStart = LastSunday(year, 3); // March
		int dstEnd	= LastSunday(year, 11); // November
		if (month > 3 && month < 11) return TRUE;
		if (month == 3 && day >= dstStart) return TRUE;
		if (month == 10 && day < dstEnd) return TRUE;
		return FALSE;
	}
	else if (region == DST_AU)
	{
		int dstStart = FirstSunday(year, 10); // October
		int dstEnd	= FirstSunday(year, 4); // April

		if (month > 10) return TRUE;
		if (month == 10 && day >= dstStart) return TRUE;

		if (month < 4) return TRUE;
		if (month == 4 && day < dstEnd) return TRUE;

		return FALSE;
	}

	return FALSE;
}

// Actual city offset considering DST
static int GetCityOffset(int cityIndex)
{
	WorldCity& c = g_cities[cityIndex];
	if (c.dstRegion == DST_NONE)
		return c.standardOffset;
	if (IsDSTActive(c.dstRegion))
		return c.dstOffset;
	return c.standardOffset;
}

int WorldClock_GetCount()
{
	return g_cityCount;
}

const char*  WorldClock_GetName(int cityIndex)
{
	if (cityIndex < 0 || cityIndex >= g_cityCount) return "";
		return g_cities[cityIndex].name;
}

void WorldClock_Show(int cityIndex)
{
	if (cityIndex < 0 || cityIndex >= g_cityCount) return;

	SYSTEMTIME stLocal;
	GetLocalTime(&stLocal);

	// Get local zone offset (in minutes by UTC)
	TIME_ZONE_INFORMATION tzi;
	DWORD tzResult = GetTimeZoneInformation(&tzi);

	// Bias - local time offset by UTC in minutes
	// Positive - on west by UTC
	int localBiasMinutes = tzi.Bias;

	// Consider Daylight if active
	if (tzResult == TIME_ZONE_ID_DAYLIGHT)
		localBiasMinutes += tzi.DaylightBias;
	else if (tzResult == TIME_ZONE_ID_STANDARD)
		localBiasMinutes += tzi.StandardBias;

	// Local offset from UTC in minutes (positive - on east)
	int localUtcOffsetMinutes = -localBiasMinutes;
	int cityOffset = GetCityOffset(cityIndex);

	// City offset in minutes
	int  cityUtcOffsetMinutes = cityOffset * 60;

	// Difference between city and local time
	int diffMinutes = cityUtcOffsetMinutes - localUtcOffsetMinutes;

	// Add to local time
	int totalMinutes = stLocal.wHour * 60 + stLocal.wMinute + diffMinutes;

	// Normalize in 0..1439
	while (totalMinutes < 0)		totalMinutes += 1440;
	while (totalMinutes >= 1440)	totalMinutes -= 1440;

	int cityHour = totalMinutes / 60;
	int cityMinute = totalMinutes % 60;

	char offsetStr[16];
	if (cityOffset >= 0)
		wsprintf(offsetStr, "+%d", cityOffset);
	else
		wsprintf(offsetStr, "%d", cityOffset);

	// Point DST if active
	const char* dstNote = "";
	if (g_cities[cityIndex].dstRegion != DST_NONE && IsDSTActive(g_cities[cityIndex].dstRegion))
		dstNote = " (DST)";

	// Form message
	char msg[256];
	wsprintf(msg,
		"%s\n\n"
		"Local time: %02d:%02d\n"
		"UTC offset: %s\n\n"
		"%s time: %02d:%02d",
		g_cities[cityIndex].name,
		stLocal.wHour, stLocal.wMinute,
		offsetStr,
		g_cities[cityIndex].name,
		cityHour, cityMinute);

	MessageBox(NULL, msg, "World Clock", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

void WorldClock_AddToMenu(HMENU hMenu, UINT baseId)
{
	for (int i = 0; i < g_cityCount; i++)
	{
		AppendMenu(hMenu, MF_STRING, baseId + i, g_cities[i].name);
	}
}

