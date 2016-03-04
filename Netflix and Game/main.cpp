#include <Windows.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
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

void muteForegroundWindow() {

	IMMDevice *mmDevice;
	IMMDeviceEnumerator *mmDeviceEnum;
	IAudioSessionManager2 *sessionManager;
	IAudioSessionEnumerator *sessionEnum;
	IAudioSessionControl *sessionControl;
	IAudioSessionControl2 *sessionControl2;
	ISimpleAudioVolume *audioVolume;

	CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&mmDeviceEnum);
	mmDeviceEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &mmDevice);
	mmDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, 0, (void**)&sessionManager);
	sessionManager->GetSessionEnumerator(&sessionEnum);

	DWORD fpid;
	GetWindowThreadProcessId(GetForegroundWindow(), &fpid);

	int sessionCount;
	sessionEnum->GetCount(&sessionCount);
	for (int i = 0; i < sessionCount; i++) {
		
		sessionEnum->GetSession(i, &sessionControl);
		sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);

		DWORD pid;
		sessionControl2->GetProcessId(&pid);
		
		if (fpid == pid) {
			
			sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume);

			BOOL muted;
			audioVolume->GetMute(&muted);
			audioVolume->SetMute(!muted, 0);

			audioVolume->Release();
		}

		sessionControl->Release();
		sessionControl2->Release();
	}

	sessionEnum->Release();
	sessionManager->Release();
	mmDevice->Release();
	mmDeviceEnum->Release();
}

BOOL WINAPI EnumWindowProc(HWND hwnd, LPARAM lParam) {

	char titleBuff[128];
	GetWindowText(hwnd, titleBuff, 128);

	if (strstr(titleBuff, "YouTube") > 0) {
		muteForegroundWindow();
		SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
		SendMessage(hwnd, WM_KEYDOWN, 'K', 0);
		SendMessage(hwnd, WM_KEYUP, 'K', 0);
		SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);

		return 0;
	}

	return 1;
}

bool isActiveWindowFullscreen() {

	HWND activeHwnd = GetForegroundWindow();
	if (activeHwnd == GetShellWindow()) return 0;

	HMONITOR activeMonitor = MonitorFromWindow(activeHwnd, MONITOR_DEFAULTTONEAREST);

	MONITORINFO activeMonitorInfo;
	activeMonitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(activeMonitor, &activeMonitorInfo);

	RECT activeHwndRect;
	GetWindowRect(activeHwnd, &activeHwndRect);

	return (activeHwndRect.top <= activeMonitorInfo.rcMonitor.top + 1 &&
		activeHwndRect.bottom >= activeMonitorInfo.rcMonitor.bottom - 1 &&
		activeHwndRect.left <= activeMonitorInfo.rcMonitor.left + 1 &&
		activeHwndRect.right >= activeMonitorInfo.rcMonitor.right - 1);
}

LRESULT CALLBACK keyHookProc(int nCode, WPARAM wParam, LPARAM lParam) {

	if (wParam == WM_KEYUP) {

		KBDLLHOOKSTRUCT *kbHookStruct = (KBDLLHOOKSTRUCT*)lParam;
		if (kbHookStruct->vkCode == VK_MEDIA_PLAY_PAUSE) {
			if (isActiveWindowFullscreen()) {
				EnumWindows(EnumWindowProc, 0);
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	CoInitialize(0);

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