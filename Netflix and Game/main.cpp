#include <Windows.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <time.h>
#include <ShlObj.h>
#include <stdio.h>
#include <psapi.h>
#include <Shlwapi.h>
#include "resource.h"

#include "media_ptr.h"

#define XBOX_BUTTON 7
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

bool prevTrackKeyDown = false;
bool nextTrackKeyDown = false;
bool xboxButtonDown = false;

enum class MatchType { title, exe };
struct MediaCommand
{
	MatchType matchType;
	const char* matchStr;
	UINT button;
};

struct PausePlayResult
{
	bool hasFoundMedia;
};

const MediaCommand mediaCommands[] = {
	{MatchType::title, "YouTube", VK_SPACE}, {MatchType::title, "Netflix", VK_SPACE},
	{MatchType::title, "Hulu", VK_SPACE},{MatchType::exe, "Spotify", NULL},
	{MatchType::title, "Skype", NULL}, {MatchType::title, "VLC media player", VK_SPACE},
	{MatchType::title, "Plex", VK_SPACE}, {MatchType::title, "CW iFrame", VK_SPACE},
	{MatchType::title, "Prime", VK_SPACE}, {MatchType::title, "Funimation", NULL},
	{MatchType::title, "Crunchyroll", VK_SPACE}, {MatchType::exe, "Firefox", NULL}
};

const MediaCommand* getMediaCommand(HWND hwnd)
{
	DWORD exeProcId = 0;
	char exeBuff[MAX_PATH];
	GetWindowThreadProcessId(hwnd, &exeProcId);
	HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, exeProcId);
	GetModuleFileNameEx((HMODULE)proc, 0, exeBuff, MAX_PATH);
	CloseHandle(proc);

	int length = GetWindowTextLength(hwnd) + 1;
	char* titleBuff = new char[length];
	GetWindowText(hwnd, titleBuff, length);

	UINT count = sizeof(mediaCommands) / sizeof(mediaCommands[0]);
	for (UINT i = 0; i < count; i++)
	{
		if ((mediaCommands[i].matchType == MatchType::title && StrStrI(titleBuff, mediaCommands[i].matchStr) != NULL) ||
			(mediaCommands[i].matchType == MatchType::exe && StrStrI(exeBuff, mediaCommands[i].matchStr) != NULL))
		{
			delete[] titleBuff;
			return &mediaCommands[i];
		}
	}

	delete[] titleBuff;
	return NULL;
}

media_ptr<IAudioSessionControl*> getMediaSession(HWND hwnd) {
	media_ptr<IMMDeviceEnumerator*> mmDeviceEnum;
	CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&mmDeviceEnum);

	media_ptr<IMMDevice*> mmDevice;
	mmDeviceEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &mmDevice);

	media_ptr<IAudioSessionManager2*> sessionManager;
	mmDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, 0, (void**)&sessionManager);

	media_ptr<IAudioSessionEnumerator*> sessionEnum;
	sessionManager->GetSessionEnumerator(&sessionEnum);

	DWORD targetPid;
	GetWindowThreadProcessId(hwnd, &targetPid);

	int sessionCount;
	sessionEnum->GetCount(&sessionCount);
	for (int i = 0; i < sessionCount; i++)
	{
		media_ptr<IAudioSessionControl*> sessionControl;
		sessionEnum->GetSession(i, &sessionControl);

		media_ptr<IAudioSessionControl2*> sessionControl2;
		sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);

		DWORD pid;
		sessionControl2->GetProcessId(&pid);
		if (targetPid == pid)
		{
			return sessionControl;
		}
	}

	return media_ptr<IAudioSessionControl*>(nullptr);
}

void changeFGWindowVolume()
{
	HWND activeHWND = GetForegroundWindow();
	if (getMediaCommand(activeHWND)) return;

	media_ptr<IAudioSessionControl*> sessionControl = getMediaSession(activeHWND);
	if (sessionControl)
	{
		media_ptr<ISimpleAudioVolume*> audioVolume;
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
		else if (soundOption == dncItemID)
		{
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
	}
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

void sendPausePlayPress() {
	KEYBDINPUT kb = {};
	kb.wVk = VK_MEDIA_PLAY_PAUSE;
	kb.dwExtraInfo = simulatedInput;

	INPUT input = {};
	input.type = INPUT_KEYBOARD;
	input.ki = kb;

	SendInput(1, &input, sizeof(INPUT));
}

BOOL WINAPI pausePlayMediaEnumProc(HWND hwnd, LPARAM lParam)
{
	if (hwnd == msgWindow ||
		!IsWindowVisible(hwnd))
	{
		return true;
	}

	const MediaCommand* mediaCommand = getMediaCommand(hwnd);
	if (mediaCommand)
	{
		PausePlayResult* input = (PausePlayResult*)lParam;
		if (!input->hasFoundMedia && mediaCommand->button == NULL)
		{
			sendPausePlayPress();
		}
		else if (mediaCommand->button != NULL)
		{
			SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
			SendMessage(hwnd, WM_KEYDOWN, mediaCommand->button, 0);
			SendMessage(hwnd, WM_KEYUP, mediaCommand->button, 0);
			SendMessage(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
		}

		input->hasFoundMedia = true;
	}

	return true;
}

PausePlayResult pausePlayMedia()
{
	PausePlayResult result{};
	HWND activeHwnd = GetForegroundWindow();

	EnumWindows(pausePlayMediaEnumProc, (LPARAM)&result);
	SetForegroundWindow(activeHwnd);
	SetActiveWindow(activeHwnd);

	return result;
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
		WCHAR option[256] = { 0 }, value[256] = { 0 };
		while (fwscanf_s(optionsFile, L"%[^,],%[^\r\n] ", option, 256, value, 256) == 2)
		{
			if (wcsncmp(option, reqFSOptionName, 256) == 0)
			{
				unsigned short fullscreen;
				swscanf_s(value, L"%hu", &fullscreen);
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
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* keyHook = (KBDLLHOOKSTRUCT*)lParam;
		if (!(keyHook->flags & LLKHF_ALTDOWN))
		{
			if (wParam == WM_KEYDOWN)
			{
				switch (keyHook->vkCode)
				{
				case VK_MEDIA_PREV_TRACK:
					if ((!reqFullscreen || isActiveWindowFullscreen()) &&
						!prevTrackKeyDown)
					{
						prevTrackKeyDown = true;
						changeFGWindowVolume();
					}
					return TRUE;
				case VK_MEDIA_PLAY_PAUSE:
					if (keyHook->dwExtraInfo & simulatedInput)
					{
						break;
					}

					if (!reqFullscreen || isActiveWindowFullscreen())
					{
						PausePlayResult result = pausePlayMedia();
						if (result.hasFoundMedia) {
							changeFGWindowVolume();
						}
					}
					return TRUE;
				case VK_MEDIA_NEXT_TRACK:
					if (!nextTrackKeyDown)
					{
						nextTrackKeyDown = true;
						pausePlayMedia();
					}
					return TRUE;
				case XBOX_BUTTON:
					if (!xboxButtonDown)
					{
						xboxButtonDown = true;
						if (!reqFullscreen || isActiveWindowFullscreen())
						{
							PausePlayResult result = pausePlayMedia();
							if (result.hasFoundMedia)
							{
								changeFGWindowVolume();
							}
						}
					}
					return TRUE;
				}
			}
			else if (wParam == WM_KEYUP)
			{
				switch (keyHook->vkCode)
				{
				case VK_MEDIA_PREV_TRACK:
					prevTrackKeyDown = false;
					return TRUE;
				case VK_MEDIA_NEXT_TRACK:
					nextTrackKeyDown = false;
					return TRUE;
				case XBOX_BUTTON:
					xboxButtonDown = false;
					return TRUE;
				}
			}
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