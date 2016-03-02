#include <iostream>
#include <Windows.h>
#include <string>
#include "resource.h"

#define shellCallback 530
#define quitItemID 889
#define msgClassName "msgClass"

HMENU popupMenu;
HWND msgWindow;

LRESULT CALLBACK msgClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	if (uMsg == shellCallback && lParam == WM_RBUTTONUP) {
		POINT p;
		GetCursorPos(&p);

		BringWindowToTop(msgWindow);
		TrackPopupMenu(popupMenu, 0, p.x, p.y, 0, msgWindow, 0);
	}
	else if (uMsg == WM_COMMAND && LOWORD(wParam) == quitItemID) {
		exit(0);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASSEX msgClass = {};

	msgClass.cbSize = sizeof(WNDCLASSEX);
	msgClass.style = CS_NOCLOSE;
	msgClass.lpfnWndProc = msgClassProc;
	msgClass.lpszClassName = msgClassName;

	RegisterClassEx(&msgClass);
	msgWindow = CreateWindowEx(0, msgClassName, "Netflix and Game", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
	
	NOTIFYICONDATA shellData = {};
	shellData.cbSize = sizeof(NOTIFYICONDATA);
	shellData.hWnd = msgWindow;
	shellData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	shellData.uCallbackMessage = shellCallback;
	shellData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	strcpy_s(shellData.szTip, "Netflix and Game");

	MENUITEMINFO quitItem = {};
	quitItem.cbSize = sizeof(MENUITEMINFO);
	quitItem.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
	quitItem.fType = MFT_STRING;
	quitItem.wID = quitItemID;
	quitItem.dwTypeData = "Quit";
	quitItem.cch = strlen("Quit");

	popupMenu = CreatePopupMenu();
	InsertMenuItem(popupMenu, 0, true, &quitItem);

	Shell_NotifyIcon(NIM_ADD, &shellData);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}