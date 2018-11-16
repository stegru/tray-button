/***

tray-button.c: Creates a button on the task tray that does nothing.

Build:

    cl tray-button.c

***/


#define UNICODE 1
#define _UNICODE 1

#include <Windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <WinBase.h>

#pragma comment (lib, "User32.lib")
#pragma comment (lib, "Kernel32.lib")
#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "uxtheme.lib")
#pragma comment (lib, "Msimg32.lib")


LRESULT CALLBACK buttonWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
void redraw();
HWND getTaskbarWindow();

HANDLE hIconOff, hIconOn;

#define ICON_SIZE 20
#define BUTTON_WIDTH 36

#define STATE_NORMAL 1
#define STATE_HOVER 2
#define STATE_PRESSED 4
#define STATE_CHECKED 8

int buttonState = STATE_NORMAL;

RECT taskRect = { 0 }, notifyRect = { 0 };
HWND buttonWindow = NULL;

BOOL highContrast = FALSE;

// Move the button in between the task list and notification icons by shrinking the task list.
BOOL positionTrayWindows(BOOL force)
{
	BOOL changed = FALSE;
	// Task bar
	HWND tray = getTaskbarWindow();
	// Container of the window list (and toolbars)
	HWND tasks = FindWindowEx(tray, NULL, L"ReBarWindow32", NULL);
	// The notification icons
	HWND notify = FindWindowEx(tray, NULL, L"TrayNotifyWnd", NULL);

	// Get the dimensions of both windows.
	RECT newTaskRect, newNotifyRect;
	RECT buttonRect = { 0 };
	GetWindowRect(tasks, &newTaskRect);
	GetWindowRect(notify, &newNotifyRect);

	changed = !EqualRect(&taskRect, &newTaskRect) || !EqualRect(&notifyRect, &newNotifyRect);
	if (force || changed) {
		CopyRect(&taskRect, &newTaskRect);
		CopyRect(&notifyRect, &newNotifyRect);

		// Shrink the tasks window
		taskRect.right = notifyRect.left - BUTTON_WIDTH;
		// Put the button between
		buttonRect.left = taskRect.right;
		buttonRect.right = notifyRect.left;
		buttonRect.top = highContrast ? 1 : 0;
		buttonRect.bottom = taskRect.bottom - taskRect.top;

		// Resize the task window
		SetWindowPos(tasks, 0, 0, 0, taskRect.right - taskRect.left, taskRect.bottom - taskRect.top,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREPOSITION);

		// Move the button between the tasks and notification area
		SetWindowPos(buttonWindow, 0,
			buttonRect.left, buttonRect.top, buttonRect.right - buttonRect.left, buttonRect.bottom - buttonRect.top,
			SWP_NOACTIVATE | SWP_NOREPOSITION);

		redraw();
	}
	return changed;
}

// Redraw the button
void redraw()
{
	RedrawWindow(buttonWindow, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ERASENOW | RDW_FRAME | RDW_ALLCHILDREN);
}

void updateState(int newState)
{
	buttonState = newState;
	redraw();
}
#define setState(F) updateState(buttonState | (F))
#define unsetState(F) updateState(buttonState & ~(F))
#define hasState(F) (buttonState & (F))


HWND getTaskbarWindow()
{
	return FindWindow(L"Shell_TrayWnd", NULL);
}

void checkHighContrast()
{
	HIGHCONTRAST hc = { 0 };
	hc.cbSize = sizeof(hc);
	SystemParametersInfo(SPI_GETHIGHCONTRAST, hc.cbSize, &hc, 0);
	highContrast = hc.dwFlags & HCF_HIGHCONTRASTON;
}

HICON loadIcon(WCHAR *iconFile)
{
	HICON hIcon = LoadImage(NULL, iconFile, IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_LOADFROMFILE);
	if (!hIcon) {
		printf("Bad icon, win32:%u\n", GetLastError());
	}
	return hIcon;
}

int main(int argc, char **argv)
{
	WCHAR *iconFileOff = argc > 1 ? argv[1] : TEXT("Morphic-tray-icon-white.ico");
	WCHAR *iconFileOn = argc > 2 ? argv[2] : TEXT("Morphic-tray-icon-green.ico");

	hIconOff = loadIcon(iconFileOff);
	hIconOn = loadIcon(iconFileOn);

	WNDCLASS cls = { 0 };
	cls.cbWndExtra = sizeof(cls);
	cls.lpfnWndProc = buttonWndProc;
	cls.lpszClassName = L"MorphicTrayButton";
	RegisterClass(&cls);

	BufferedPaintInit();

	checkHighContrast();

	// Create the button window
	buttonWindow = CreateWindowEx(
		WS_EX_TRANSPARENT,
		L"MorphicTrayButton",
		L"Hello",
		WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
		0, 0, BUTTON_WIDTH, 40,
		getTaskbarWindow(),
		NULL,
		0,
		NULL);

	positionTrayWindows(TRUE);
	MSG msg = { 0 };

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

// Draws a translucent rectangle
void AlphaRect(HDC dc, RECT rc, COLORREF color, BYTE alpha)
{
	// Create a bitmap containing a single translucent pixel, and stretch it over the rect.

	// The bitmap
	int pixel;
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = bmi.bmiHeader.biHeight = 1;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;

	// The pixel
	pixel = (
		((alpha * GetRValue(color) / 0xff) << 16) |
		((alpha * GetGValue(color) / 0xff) << 8) |
		(alpha * GetBValue(color) / 0xff)
			);
	pixel = pixel | (alpha << 24);

	// Make the bitmap DC.
	int *bits;
	HDC dcPixel = CreateCompatibleDC(dc);
	HBITMAP bmpPixel = CreateDIBSection(dcPixel, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
	HBITMAP bmpOrig = SelectObject(dcPixel, bmpPixel);
	*bits = pixel;

	// Draw the "rectangle"
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	AlphaBlend(dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, dcPixel, 0, 0, 1, 1, bf);

	SelectObject(dcPixel, bmpOrig);
	DeleteObject(bmpPixel);
	DeleteDC(dcPixel);
}

// Draw the window, from WM_PAINT
void paint()
{
	RECT rc;
	HDC dc, dcPaint;
	PAINTSTRUCT ps;

	GetWindowRect(buttonWindow, &rc);
	MapWindowPoints(0, buttonWindow, (POINT*)&rc, 2);

	dcPaint = BeginPaint(buttonWindow, &ps);

	HPAINTBUFFER paintBuffer = BeginBufferedPaint(dcPaint, &rc, BPBF_TOPDOWNDIB, NULL, &dc);
	BufferedPaintClear(paintBuffer, NULL);

	int x = (rc.right - ICON_SIZE) / 2;
	int y = (rc.bottom - ICON_SIZE) / 2;

	if (highContrast) {
		int backcolor = COLOR_WINDOW;
		int forecolor = COLOR_WINDOWTEXT;

		if (hasState(STATE_HOVER)) {
			backcolor = COLOR_HOTLIGHT;
			forecolor = COLOR_HIGHLIGHTTEXT;
		}

		if (hasState(STATE_CHECKED)) {
			forecolor = COLOR_HIGHLIGHT;
		}

		backcolor = GetSysColor(backcolor);
		forecolor = GetSysColor(forecolor);


		HICON hIcon = hIconOff;
		// Create an off-screen copy of the icon
		HDC dcBuf = CreateCompatibleDC(dc);

		// Make the colours of the icon what they need to be by manually changing each pixel
		// TODO: Pre-generate

		// Create a bitmap
		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = rc.right;
		bmi.bmiHeader.biHeight = rc.bottom;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;

		UINT *pixels;
		HBITMAP bmpBuf = CreateDIBSection(dcBuf, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
		HBITMAP origBmp = SelectObject(dcBuf, bmpBuf);

		AlphaRect(dcBuf, rc, 0, 255);
		DrawIconEx(dcBuf, x, y, hIcon, ICON_SIZE, ICON_SIZE, 0, NULL, DI_NORMAL);
		int len = rc.right * rc.bottom;
		UINT *p = pixels;
#define TO_RGB(C) ((C << 16) & 0xff0000) | (C & 0xff00) | ((C >> 16) & 0xff)
#define BLACK 0xff000000
#define WHITE 0xffffffff
		int bg = TO_RGB(backcolor), fg = TO_RGB(forecolor);
		BYTE r1 = GetRValue(fg), g1 = GetGValue(fg), b1 = GetBValue(fg);
		BYTE r2 = GetRValue(bg), g2 = GetGValue(bg), b2 = GetBValue(bg);

		for (int n = 0; n < len; n++, p++) {
			if (*p == BLACK) {
				*p = bg;
			} else if (*p == WHITE) {
				*p = fg;
			} else {
				double a = (*p & 0xff) / 255.0;
				*p = RGB(
					r1 * a + r2 * (1.0 - a),
					g1 * a + g2 * (1.0 - a),
					b1 * a + b2 * (1.0 - a)
				);
			}
		}

		AlphaRect(dc, rc, 0, 255);
		BitBlt(dc, 0, 0, rc.right, rc.bottom, dcBuf, 0, 0, SRCINVERT);

		SelectObject(dcBuf, origBmp);
		DeleteObject(bmpBuf);
		DeleteObject(dcBuf);

	} else {
		HICON hIcon = hasState(STATE_CHECKED) ? hIconOn : hIconOff;
		BYTE alpha = 0;
		if (hasState(STATE_PRESSED)) {
			alpha = 10;
		} else if (hasState(STATE_HOVER)) {
			alpha = 25;
		}

		if (alpha) {
			AlphaRect(dc, rc, RGB(255, 255, 255), alpha);
		}
		DrawIconEx(dc, x, y, hIcon, ICON_SIZE, ICON_SIZE, 0, 0, DI_NORMAL);
	}

	EndBufferedPaint(paintBuffer, TRUE);
	EndPaint(buttonWindow, &ps);
}

LRESULT CALLBACK buttonWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_MOUSEMOVE:
		if (!hasState(STATE_HOVER)) {
			TRACKMOUSEEVENT tme = { 0 };
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hwnd;
			TrackMouseEvent(&tme);

			setState(STATE_HOVER);
		}
		break;

	case WM_MOUSELEAVE:
		unsetState(STATE_HOVER | STATE_PRESSED);
		break;

	case WM_LBUTTONDOWN:
		setState(STATE_PRESSED);
		break;

	case WM_LBUTTONUP:
		puts("left-click\n");
		unsetState(STATE_PRESSED);
		if (hasState(STATE_CHECKED)) {
			unsetState(STATE_CHECKED);
		} else {
			setState(STATE_CHECKED);
		}
		break;

	case WM_RBUTTONUP:
		puts("right-click\n");
		return 0;

	case WM_ERASEBKGND:
		positionTrayWindows(FALSE);
		puts("WM_ERASEBKGND\n");
		break;

	case WM_PAINT:
		puts("WM_PAINT\n");
		paint();
		break;

	case WM_SETTINGCHANGE:
		checkHighContrast();
		positionTrayWindows(TRUE);
		break;

	case WM_DESTROY:
		BufferedPaintUnInit();
		break;

	default:
		printf("%u %x (%u, %u)\n", hwnd, msg, wp, lp);
		break;
	}

	return DefWindowProc(hwnd, msg, wp, lp);
}
