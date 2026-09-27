#include "clockicon.h"
#include <math.h>

//Color by hour
static COLORREF HourColor(int hour)
{
	if (hour < 6)		return RGB( 40, 40, 120); // night
	else if (hour < 12)	return RGB(255, 180, 40); // morning
	else if (hour < 18)	return RGB( 60, 160, 230); // day
	else				return RGB(160, 60, 160); // evening
}

HICON CreateClockIcon(int size)
{
	HDC hdcScreen	= GetDC(NULL);
	HDC hdcColor	= CreateCompatibleDC(hdcScreen);
	HDC hdcMask		= CreateCompatibleDC(hdcScreen);

	//color part
	HBITMAP hbmColor	= CreateCompatibleBitmap(hdcScreen, size, size);
	HBITMAP hbmMask		= CreateBitmap(size, size, 1, 1, NULL);

	HBITMAP oldColor	= (HBITMAP)SelectObject(hdcColor, hbmColor);
	HBITMAP oldMask		= (HBITMAP)SelectObject(hdcMask, hbmMask);

	//Mask background - white (transparent)
	PatBlt(hdcMask, 0, 0, size, size, WHITENESS);

	//Color part background - black
	RECT rc = { 0, 0, size, size };
	HBRUSH hbrBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);
	FillRect(hdcColor, &rc, hbrBlack);

	SYSTEMTIME st;
	GetLocalTime(&st);

	//Drawing clock face
	COLORREF faceColor = HourColor(st.wHour);
	HBRUSH hbrFace = CreateSolidBrush(faceColor);
	HPEN	hpenFace = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	HBRUSH oldBr = (HBRUSH)SelectObject(hdcColor, hbrFace);
	HPEN	oldPen = (HPEN)SelectObject(hdcColor, hpenFace);

	Ellipse(hdcColor, 0, 0, size, size);

	//Mask: black circle
	HBRUSH hbrMaskBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);
	HBRUSH oldMaskBr = (HBRUSH)SelectObject(hdcMask, hbrMaskBlack);
	HPEN oldMaskPen = (HPEN)SelectObject(hdcMask, GetStockObject(BLACK_PEN));
	Ellipse(hdcMask, 0, 0, size, size);

	// Arrows
	//Counting angles
	double hourAngle = ((st.wHour % 12) + st.wMinute / 60.0) * 30.0 - 90.0;
	double minAngle = (st.wMinute + st.wSecond / 60.0) * 6.0 - 90.0;

	double cx = size / 2.0;
	double cy = size / 2.0;
	double rHour = size * 0.28;
	double rMin = size * 0.40;
	double rad = 3.14159265 / 180.0;

	HPEN hpenHour = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
	HPEN hpenMin  = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

	//Hour
	SelectObject(hdcColor, hpenHour);
	MoveToEx(hdcColor, (int)cx, (int)cy, NULL);
	LineTo(hdcColor, (int)(cx + rHour * cos(hourAngle * rad)), (int)(cy + rHour * sin(hourAngle * rad)));

	//Mintute
	SelectObject(hdcColor, hpenMin);
	MoveToEx(hdcColor, (int)cx, (int)cy, NULL);
	LineTo(hdcColor, (int)(cx + rMin *  cos(minAngle * rad)), (int)(cy + rMin * sin(minAngle * rad)));

	//Restore DC
	SelectObject(hdcColor, oldBr);
	SelectObject(hdcColor, oldPen);
	SelectObject(hdcMask, oldMaskBr);
	SelectObject(hdcMask, oldMaskPen);
	SelectObject(hdcColor, oldColor);
	SelectObject(hdcMask, oldMask);

	DeleteObject(hbrFace);
	DeleteObject(hpenFace);
	DeleteObject(hpenHour);
	DeleteObject(hpenMin);

	//Icon
	ICONINFO ii = {0};
	ii.fIcon	= TRUE;
	ii.hbmColor	= hbmColor;
	ii.hbmMask	= hbmMask;
	HICON hIcon	= CreateIconIndirect(&ii);

	//Cleaning
	DeleteObject(hbmColor);
	DeleteObject(hbmMask);
	DeleteDC(hdcColor);
	DeleteDC(hdcMask);
	ReleaseDC(NULL, hdcScreen);

	return hIcon;
}


