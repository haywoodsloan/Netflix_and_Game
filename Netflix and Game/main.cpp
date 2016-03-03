#include <iostream>
#include <Windows.h>
#include <string>
#include "resource.h"

#define shellCallback 530
#define quitItemID 889
#define msgClassName "msgClass"

HMENU popupMenu;
HWND msgWindow;
NOTIFYICONDATA shellData;

LRESULT CALLBACK msgClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	if (uMsg == shellCallback && lParam == WM_RBUTTONUP) {
		POINT p;
		GetCursorPos(&p);

		BringWindowToTop(msgWindow);
		TrackPopupMenu(popupMenu, 0, p.x, p.y, 0, msgWindow, 0);
	}
	else if (uMsg == WM_COMMAND && LOWORD(wParam) == quitItemID) {
		Shell_NotifyIcon(NIM_DELETE, &shellData);
		exit(0);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


BOOL WINAPI EnumWindowProc(HWND hwnd, LPARAM lParam) {
	
	char titleBuff[128];
	GetWindowText(hwnd, titleBuff, 128);

	if (strstr(titleBuff, "YouTube") > 0) {
		SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
		SendMessage(hwnd, WM_KEYDOWN, 'K', 0);
		SendMessage(hwnd, WM_KEYUP, 'K', 0);
		SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
		
		return 0;
	}

	return 1;
}

LRESULT CALLBACK keyHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	
	if (wParam == WM_KEYUP) {
		
		KBDLLHOOKSTRUCT *kbHookStruct = (KBDLLHOOKSTRUCT*)lParam;
		if (kbHookStruct->vkCode == VK_MEDIA_PLAY_PAUSE) {
			EnumWindows(EnumWindowProc, 0);
		}

	}
	
	return CallNextHookEx(0, nCode, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASSEX msgClass = {};
	msgClass.cbSize = sizeof(WNDCLASSEX);
	msgClass.style = CS_NOCLOSE;
	msgClass.lpfnWndProc = msgClassProc;
	msgClass.lpszClassName = msgClassName;

	RegisterClassEx(&msgClass);
	msgWindow = CreateWindowEx(0, msgClassName, "Netflix and Game", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);

	MENUITEMINFO quitItem = {};
	quitItem.cbSize = sizeof(MENUITEMINFO);
	quitItem.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
	quitItem.fType = MFT_STRING;
	quitItem.wID = quitItemID;
	quitItem.dwTypeData = "Quit";
	quitItem.cch = 5;

	popupMenu = CreatePopupMenu();
	InsertMenuItem(popupMenu, 0, 1, &quitItem);

	shellData = {};
	shellData.cbSize = sizeof(NOTIFYICONDATA);
	shellData.hWnd = msgWindow;
	shellData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	shellData.uCallbackMessage = shellCallback;
	shellData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	strcpy_s(shellData.szTip, "Netflix and Game");

	Shell_NotifyIcon(NIM_ADD, &shellData);

	SetWindowsHookEx(WH_KEYBOARD_LL, keyHookProc, 0, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}