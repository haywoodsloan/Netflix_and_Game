#include <Windows.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include "resource.h"

#define shellCallback 530
#define msgClassName "msgClass"
#define msgWindowName "Netflix and Game"

HMENU popupMenu;
HWND msgWindow;
HHOOK keyHook;
NOTIFYICONDATA shellData;

UINT taskbarCreateMsg;
UINT soundOption = s10ItemID;
bool reqFullscreen = true;

struct mediaCommand
{
	char* title;
	UINT button;
};

const mediaCommand mediaCommands[] = { {"YouTube",'K'}, {"Netflix", VK_SPACE}, {"Hulu", VK_SPACE},
									  {"Spotify", NULL}, {"Skype", NULL}, {"VLC media player", VK_SPACE},
									  {"Plex", VK_SPACE}, {"CW iFrame", VK_SPACE}, {"Amazon.com", VK_SPACE} };

void changeFGWindowVolume()
{
	if (soundOption == dncItemID) return;

	char titleBuff[128];
	HWND activeHWND = GetForegroundWindow();
	GetWindowText(activeHWND, titleBuff, 128);

	UINT count = sizeof(mediaCommands) / sizeof(mediaCommands[0]);
	for (UINT i = 0; i < count; i++)
	{
		if (strstr(titleBuff, mediaCommands[i].title) > 0) return;
	}

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
	GetWindowThreadProcessId(activeHWND, &activePid);

	int sessionCount;
	sessionEnum->GetCount(&sessionCount);
	for (int i = 0; i < sessionCount; i++)
	{
		sessionEnum->GetSession(i, &sessionControl);
		sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);

		DWORD pid;
		sessionControl2->GetProcessId(&pid);
		if (activePid == pid)
		{
			sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume);

			BOOL muted;
			audioVolume->GetMute(&muted);

			float volumeLevel;
			audioVolume->GetMasterVolume(&volumeLevel);

			if (soundOption == muteItemID)
			{
				audioVolume->SetMute(!muted, 0);

				if (volumeLevel != 1.0f)
				{
					audioVolume->SetMasterVolume(1.0f, 0);
				}
			}
			else
			{
				float newVolumeLevel = (soundOption - sBaseItemID) / 100.0f;
				audioVolume->SetMasterVolume(volumeLevel == 1.0f ? newVolumeLevel : 1.0f, 0);

				if (muted)
				{
					audioVolume->SetMute(false, 0);
				}
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

BOOL WINAPI pausePlayMediaEnumProc(HWND hwnd, LPARAM lParam)
{
	char titleBuff[128];
	GetWindowText(hwnd, titleBuff, 128);

	UINT count = sizeof(mediaCommands) / sizeof(mediaCommands[0]);
	for (UINT i = 0; i < count; i++)
	{
		if (strstr(titleBuff, mediaCommands[i].title) > 0)
		{
			bool* hasChangedVolume = (bool*)lParam;
			*hasChangedVolume = true;

			LONG windowStyles = GetWindowLong(hwnd, GWL_EXSTYLE);
			LONG windowStyleNoActive = windowStyles | WS_EX_NOACTIVATE;

			SetWindowLong(hwnd, GWL_EXSTYLE, windowStyleNoActive);
			SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
			SendMessage(hwnd, WM_KEYDOWN, mediaCommands[i].button, 0);
			SendMessage(hwnd, WM_KEYUP, mediaCommands[i].button, 0);
			SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
			SetWindowLong(hwnd, GWL_EXSTYLE, windowStyles);

			return 1;
		}
	}

	return 1;
}

bool pausePlayMedia()
{
	bool hasChangedVolume = false;
	EnumWindows(pausePlayMediaEnumProc, (LPARAM)&hasChangedVolume);
	return hasChangedVolume;
}

bool isActiveWindowFullscreen()
{
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

LRESULT CALLBACK msgClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_HOTKEY)
	{
		WORD key = HIWORD(lParam);
		WORD mods = LOWORD(lParam);

		if (key == VK_MEDIA_PLAY_PAUSE &&
			(!reqFullscreen || isActiveWindowFullscreen()) &&
			pausePlayMedia())
		{
			changeFGWindowVolume();
		}
		else if (key == VK_F6 && (mods & MOD_ALT) &&
			(!reqFullscreen || isActiveWindowFullscreen()))
		{
			changeFGWindowVolume();
		}
		else if (key == VK_F6 && (mods & MOD_CONTROL))
		{
			pausePlayMedia();
		}
	}
	else if (uMsg == shellCallback && lParam == WM_RBUTTONUP)
	{
		POINT p;
		GetCursorPos(&p);

		SetForegroundWindow(msgWindow);
		TrackPopupMenu(popupMenu, 0, p.x, p.y, 0, msgWindow, 0);
		PostMessage(msgWindow, WM_NULL, 0, 0);
	}
	else if (uMsg == WM_COMMAND)
	{
		if (LOWORD(wParam) == quitItemID)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
		else if (LOWORD(wParam) == reqFSItemID)
		{
			reqFullscreen = !reqFullscreen;

			MENUITEMINFO checkInfo = {};
			checkInfo.cbSize = sizeof(MENUITEMINFO);
			checkInfo.fMask = MIIM_STATE;

			GetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
			checkInfo.fState ^= MFS_CHECKED;
			checkInfo.fState ^= MFS_UNCHECKED;
			SetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
		}
		else
		{
			soundOption = LOWORD(wParam);

			MENUITEMINFO checkInfo = {};
			checkInfo.cbSize = sizeof(MENUITEMINFO);
			checkInfo.fMask = MIIM_STATE;

			HMENU soundOptions = GetSubMenu(popupMenu, 0);
			UINT count = GetMenuItemCount(soundOptions);

			for (UINT i = 0; i < count; i++)
			{
				if (GetMenuItemID(soundOptions, i) == soundOption)
				{
					checkInfo.fState = MFS_CHECKED;
					SetMenuItemInfo(popupMenu, GetMenuItemID(soundOptions, i), 0, &checkInfo);
				}
				else
				{
					checkInfo.fState = MFS_UNCHECKED;
					SetMenuItemInfo(popupMenu, GetMenuItemID(soundOptions, i), 0, &checkInfo);
				}
			}
		}
	}
	else if (uMsg == taskbarCreateMsg)
	{
		Shell_NotifyIcon(NIM_ADD, &shellData);
	}
	else if (uMsg == WM_CREATE)
	{
		taskbarCreateMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));
	}
	else if (uMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if (FindWindow(msgClassName, NULL)) return 1;

	CoInitialize(NULL);

	WNDCLASSEX msgClass = {};
	msgClass.cbSize = sizeof(WNDCLASSEX);
	msgClass.lpfnWndProc = msgClassProc;
	msgClass.hInstance = hInstance;
	msgClass.lpszClassName = msgClassName;

	RegisterClassEx(&msgClass);
	msgWindow = CreateWindowEx(0, msgClassName, msgWindowName, NULL, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	popupMenu = GetSubMenu(LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1)), 0);

	shellData = {};
	shellData.cbSize = sizeof(NOTIFYICONDATA);
	shellData.hWnd = msgWindow;
	shellData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	shellData.uCallbackMessage = shellCallback;
	shellData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	strcpy_s(shellData.szTip, "Netflix and Game");

	Shell_NotifyIcon(NIM_ADD, &shellData);
	RegisterHotKey(msgWindow, 0, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
	RegisterHotKey(msgWindow, 1, MOD_NOREPEAT | MOD_ALT, VK_F6);
	RegisterHotKey(msgWindow, 2, MOD_NOREPEAT | MOD_CONTROL, VK_F6);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &shellData);
	return 0;
}