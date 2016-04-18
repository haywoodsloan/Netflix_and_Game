#include <Windows.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include "resource.h"

#define shellCallback 530
#define msgClassName "msgClass"

HMENU popupMenu;
HWND msgWindow;
HHOOK keyHook;
NOTIFYICONDATA shellData;

UINT soundOption = s25ItemID;
BOOL reqFullscreen = 1;

struct mediaCommand {
	char *title;
	UINT button;
};
const mediaCommand mediaCommands[] = { {"YouTube",'K'}, {"Netflix", VK_SPACE}, {"Hulu", VK_SPACE} , {"Spotify", 0}, {"Skype", 0} };

LRESULT CALLBACK msgClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (uMsg == shellCallback && lParam == WM_RBUTTONUP) {

		POINT p;
		GetCursorPos(&p);

		SetForegroundWindow(msgWindow);
		TrackPopupMenu(popupMenu, 0, p.x, p.y, 0, msgWindow, 0);
		PostMessage(msgWindow, WM_NULL, 0, 0);
	}
	else if (uMsg == WM_COMMAND) {

		if (LOWORD(wParam) == quitItemID) {
			Shell_NotifyIcon(NIM_DELETE, &shellData);
			UnhookWindowsHookEx(keyHook);
			exit(0);
		}
		else if (LOWORD(wParam) == reqFSItemID) {
			reqFullscreen = !reqFullscreen;

			MENUITEMINFO checkInfo = {};
			checkInfo.cbSize = sizeof(MENUITEMINFO);
			checkInfo.fMask = MIIM_STATE;

			GetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
			checkInfo.fState ^= MFS_CHECKED;
			checkInfo.fState ^= MFS_UNCHECKED;
			SetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
		}
		else {
			soundOption = LOWORD(wParam);

			MENUITEMINFO checkInfo = {};
			checkInfo.cbSize = sizeof(MENUITEMINFO);
			checkInfo.fMask = MIIM_STATE;

			HMENU soundOptions = GetSubMenu(popupMenu, 0);
			UINT count = GetMenuItemCount(soundOptions);

			for (UINT i = 0; i < count; i++) {

				if (GetMenuItemID(soundOptions, i) == soundOption) {
					checkInfo.fState = MFS_CHECKED;
					SetMenuItemInfo(popupMenu, GetMenuItemID(soundOptions, i), 0, &checkInfo);
				}
				else {
					checkInfo.fState = MFS_UNCHECKED;
					SetMenuItemInfo(popupMenu, GetMenuItemID(soundOptions, i), 0, &checkInfo);
				}
			}
		}
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

	DWORD activePid;
	GetWindowThreadProcessId(GetForegroundWindow(), &activePid);

	int sessionCount;
	sessionEnum->GetCount(&sessionCount);
	for (int i = 0; i < sessionCount; i++) {

		sessionEnum->GetSession(i, &sessionControl);
		sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);

		DWORD pid;
		sessionControl2->GetProcessId(&pid);
		if (activePid == pid) {

			sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume);

			if (soundOption == muteItemID) {
				BOOL muted;

				audioVolume->GetMute(&muted);
				audioVolume->SetMute(!muted, 0);
			}
			else {
				float volumeLevel;
				float newVolumeLevel = (soundOption - 40000) / 100.0f;

				audioVolume->GetMasterVolume(&volumeLevel);
				audioVolume->SetMasterVolume(volumeLevel == 1.0f ? newVolumeLevel : 1.0f, 0);
			}

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

	UINT count = sizeof(mediaCommands) / sizeof(mediaCommands[0]);
	for (UINT i = 0; i < count; i++) {
		if (strstr(titleBuff, mediaCommands[i].title) > 0) {
			
			HWND foregroundHWND = GetForegroundWindow();
			muteForegroundWindow();

			SendMessage(hwnd, WM_ACTIVATE, WA_CLICKACTIVE, 0);
			SendMessage(hwnd, WM_KEYDOWN, mediaCommands[i].button, 0);
			SendMessage(hwnd, WM_CHAR, mediaCommands[i].button, 0);
			SendMessage(hwnd, WM_KEYUP, mediaCommands[i].button, 0);
			SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);

			SetActiveWindow(foregroundHWND);
			SetFocus(foregroundHWND);

			return 0;
		}
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
			if (!reqFullscreen || isActiveWindowFullscreen()) {
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
	msgClass.lpfnWndProc = msgClassProc;
	msgClass.hInstance = hInstance;
	msgClass.lpszClassName = msgClassName;

	RegisterClassEx(&msgClass);
	msgWindow = CreateWindowEx(0, msgClassName, "Netflix and Game", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0);
	popupMenu = GetSubMenu(LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1)), 0);

	shellData = {};
	shellData.cbSize = sizeof(NOTIFYICONDATA);
	shellData.hWnd = msgWindow;
	shellData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	shellData.uCallbackMessage = shellCallback;
	shellData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	strcpy_s(shellData.szTip, "Netflix and Game");

	Shell_NotifyIcon(NIM_ADD, &shellData);
	keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyHookProc, 0, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &shellData);
	UnhookWindowsHookEx(keyHook);
}