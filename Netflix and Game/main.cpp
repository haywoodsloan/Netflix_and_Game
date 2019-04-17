#include <Windows.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <time.h>
#include <ShlObj.h>
#include <stdio.h>
#include "resource.h"

#define shellCallback 530
#define msgClassName "msgClass"
#define msgWindowName "Netflix and Game"
#define reqFSOptionName L"ReqFS"
#define soundOptionName L"SoundOption"

HMENU popupMenu;
HWND msgWindow;
HHOOK keyHook;
NOTIFYICONDATA shellData;

UINT taskbarCreateMsg;
UINT soundOption = s25ItemID;
bool reqFullscreen = true;

struct mediaCommand
{
	char* title;
	UINT button;
};

struct mediaEnumInput
{
	bool hasChangedVolume;
	bool simulatePause;
};

const mediaCommand mediaCommands[] = { {"YouTube", NULL}, {"Netflix", NULL}, {"Hulu", VK_SPACE},
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

BOOL WINAPI pausePlayMediaEnumProc(HWND hwnd, LPARAM lParam)
{	
	if (hwnd == msgWindow ||
		!IsWindowVisible(hwnd))
	{
		return true;
	}

	int length = GetWindowTextLength(hwnd) + 1;
	char* titleBuff = new char[length];
	GetWindowText(hwnd, titleBuff, length);

	UINT count = sizeof(mediaCommands) / sizeof(mediaCommands[0]);
	for (UINT i = 0; i < count; i++)
	{
		if (strstr(titleBuff, mediaCommands[i].title) > 0)
		{
			mediaEnumInput* input = (mediaEnumInput*)lParam;
			input->hasChangedVolume = true;

			if (input->simulatePause && mediaCommands[i].button == NULL)
			{
				KEYBDINPUT kb = {};
				kb.wVk = VK_MEDIA_PLAY_PAUSE;
				kb.dwExtraInfo = GetMessageExtraInfo();

				INPUT input = {};
				input.type = INPUT_KEYBOARD;
				input.ki = kb;

				SendInput(1, &input, sizeof(INPUT));
				
				kb.dwFlags = KEYEVENTF_KEYUP;
				SendInput(1, &input, sizeof(INPUT));
			}
			else
			{
				LONG windowStyles = GetWindowLong(hwnd, GWL_EXSTYLE);
				LONG windowStyleNoActive = windowStyles | WS_EX_NOACTIVATE;

				SetWindowLong(hwnd, GWL_EXSTYLE, windowStyleNoActive);
				SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
				SendMessage(hwnd, WM_KEYDOWN, mediaCommands[i].button, 0);
				SendMessage(hwnd, WM_KEYUP, mediaCommands[i].button, 0);
				SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
				SetWindowLong(hwnd, GWL_EXSTYLE, windowStyles);
			}

			break;
		}
	}

	delete[] titleBuff;
	return true;
}

bool pausePlayMedia(bool simulatePause)
{
	mediaEnumInput input;
	input.hasChangedVolume = false;
	input.simulatePause = simulatePause;

	HWND activeHwnd = GetForegroundWindow();

	EnumWindows(pausePlayMediaEnumProc, (LPARAM)&input);
	SetForegroundWindow(activeHwnd);

	return input.hasChangedVolume;
}

void updateSoundOption()
{
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

void updateRequireFullScreen()
{
	MENUITEMINFO checkInfo = {};
	checkInfo.cbSize = sizeof(MENUITEMINFO);
	checkInfo.fMask = MIIM_STATE;

	GetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
	checkInfo.fState = reqFullscreen ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(popupMenu, reqFSItemID, 0, &checkInfo);
}

FILE* getOptionsFile(const wchar_t* mode)
{
	PWSTR folderPath;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &folderPath);

	WCHAR optionsFilePath[1024];
	swprintf_s(optionsFilePath, L"%s\\%s", folderPath, L"Netflix and Game");
	CoTaskMemFree(folderPath);

	CreateDirectoryW(optionsFilePath, NULL);
	swprintf_s(optionsFilePath, L"%s\\%s", optionsFilePath, L"options.csv");

	FILE* optionsFile;
	_wfopen_s(&optionsFile, optionsFilePath, mode);

	return optionsFile;
}

void saveOptions()
{
	FILE* optionsFile = getOptionsFile(L"w");

	if (optionsFile)
	{
		fwprintf_s(optionsFile, L"%s,%hhu\n", reqFSOptionName, reqFullscreen);
		fwprintf_s(optionsFile, L"%s,%i\n", soundOptionName, soundOption);
		fclose(optionsFile);
	}
}

LRESULT CALLBACK msgClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == shellCallback && lParam == WM_RBUTTONUP)
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
			updateRequireFullScreen();
		}
		else
		{
			soundOption = LOWORD(wParam);
			updateSoundOption();
		}

		saveOptions();
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

void loadOptions()
{
	FILE* optionsFile = getOptionsFile(L"r");

	if (optionsFile)
	{
		WCHAR option[256], value[256];
		while (fwscanf_s(optionsFile, L"%[^,],%[^\r\n] ", option, 256, value, 256) == 2)
		{
			if (wcsncmp(option, reqFSOptionName, 256) == 0)
			{
				unsigned char fullscreen;
				swscanf_s(value, L"%hhu", &fullscreen);
				reqFullscreen = fullscreen;
			}
			else if (wcsncmp(option, soundOptionName, 256) == 0)
			{
				swscanf_s(value, L"%i", &soundOption);
			}
		}

		fclose(optionsFile);
	}

	updateRequireFullScreen();
	updateSoundOption();
}

LRESULT CALLBACK keyHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && wParam == WM_KEYUP)
	{
		KBDLLHOOKSTRUCT* keyHook = (KBDLLHOOKSTRUCT*)lParam;
		switch (keyHook->vkCode)
		{
			case VK_MEDIA_PLAY_PAUSE:
			case VK_MEDIA_PREV_TRACK:
				if ((!reqFullscreen || isActiveWindowFullscreen()) &&
					(keyHook->vkCode == VK_MEDIA_PREV_TRACK || pausePlayMedia(false)))
				{
					changeFGWindowVolume();
				}
				break;
			case VK_MEDIA_NEXT_TRACK:
				pausePlayMedia(true);
				break;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
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
	loadOptions();

	shellData = {};
	shellData.cbSize = sizeof(NOTIFYICONDATA);
	shellData.hWnd = msgWindow;
	shellData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	shellData.uCallbackMessage = shellCallback;
	shellData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	strcpy_s(shellData.szTip, "Netflix and Game");

	Shell_NotifyIcon(NIM_ADD, &shellData);
	keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyHookProc, hInstance, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &shellData);
	UnhookWindowsHookEx(keyHook);
	return 0;
}