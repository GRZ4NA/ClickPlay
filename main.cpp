//INCLUDES
#include "framework.h"
#include "resource.h"
#include <ShlObj.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cstdlib>
#include <time.h>

//IMPORTS
using std::wstring;
using std::vector;

//CONTROL IDS
#define IDC_DIRLABEL 201
#define IDC_BROWSEBTN 202
#define IDC_LIST 203
#define IDC_SEARCH 204
#define IDC_SEARCHBTN 205
#define IDC_NPLABEL 206
#define IDC_PLAYBTN 207
#define IDC_STOPBTN 208

//STATIC DEFINES
#define EXT_LENGTH 2
#define STATE_CHANGE 333

//STRING TABLE
#define PROGRAM_NAME L"ClickPlay"
#define NOT_PLAYING_LABEL L"-"
#define PLAY_LABEL L"Play"
#define PAUSE_LABEL L"Pause"
#define NO_DIR_LOADED_LABEL L"No directory"

//GLOBAL VARS
HFONT hFont = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
HINSTANCE globalInstance;
MSG msg;
wstring ext[EXT_LENGTH] = { L"mp3", L"wav" };

//PATH VARS
vector<WIN32_FIND_DATA> files;
wstring path;
wstring currentSearchPhrase;
__int64 selected = -1;

//STATE VARS
/*
* -1 - not ready
* 0 - ready
* 1 - playback active
* 2 - playback paused
*/
int currentState = -1;
BOOL isOnClickPlayEnabled = TRUE;
BOOL isAutoPlayEnabled = TRUE;
BOOL isLoopEnabled = FALSE;
BOOL isQueued = FALSE;
BOOL showExtensions = FALSE;

//CONTROLS
HWND mainWindow;
HWND dirLabel;
HWND browseButton;
HWND filesListCtrl;
HWND searchBox;
HWND searchBtn;
HWND playButton;
HWND stopButton;
HWND npLabel;
HMENU menuBar;

//HOOKS
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK JumpToProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
bool alphabeticSort(WIN32_FIND_DATA a, WIN32_FIND_DATA b);

//ENTRY POINT
int WINAPI WinMain(HINSTANCE currentInst, HINSTANCE previousInst, LPSTR lpCmdLine, int nCmdShow) {
	globalInstance = currentInst;

	//CREATE & REGISTER WINDOW CLASS
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = currentInst;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	windowClass.lpszClassName = PROGRAM_NAME;
	windowClass.hIcon = (HICON)LoadImage(currentInst, L"ClickPlay.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
	windowClass.hIconSm = (HICON)LoadImage(currentInst, L"small.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
	if (!RegisterClassEx(&windowClass)) {
		return 1;
	}

	//LOAD KEYBOARD SHORTCUT TABLE
	HACCEL shortcuts = LoadAccelerators(currentInst, MAKEINTRESOURCE(IDC_CLICKPLAY));

	//LOAD MENU BAR
	menuBar = LoadMenu(currentInst, (LPCWSTR)IDC_MENU);

	//CREATE MAIN WINDOW
	mainWindow = CreateWindowEx(WS_EX_CLIENTEDGE, windowClass.lpszClassName, PROGRAM_NAME, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 400, 620, NULL, menuBar, currentInst, NULL);
	SendMessage(mainWindow, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(mainWindow, STATE_CHANGE, -1, NULL);
	ShowWindow(mainWindow, nCmdShow);
	UpdateWindow(mainWindow);

	//INITIALIZE MESSAGE LOOP
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(mainWindow, shortcuts, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

//CREATE CONTROLS
void CreateControls(HWND window) {
	dirLabel = CreateWindowEx(0, L"STATIC", NO_DIR_LOADED_LABEL, WS_CHILD | WS_VISIBLE, 10, 10, 250, 40, window, (HMENU)IDC_DIRLABEL, globalInstance, NULL);
	SendMessage(dirLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
	browseButton = CreateWindowEx(0, L"BUTTON", L"Browse", WS_CHILD | WS_VISIBLE, 275, 10, 100, 30, window, (HMENU)IDC_BROWSEBTN, globalInstance, NULL);
	SendMessage(browseButton, WM_SETFONT, (WPARAM)hFont, TRUE);
	filesListCtrl = CreateWindowEx(0, L"LISTBOX", NULL, LBS_NOTIFY | LBS_DISABLENOSCROLL | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 50, 380, 425, window, (HMENU)IDC_LIST, globalInstance, NULL);
	SendMessage(filesListCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
	searchBox = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE, 0, 460, 300, 25, window, (HMENU)IDC_SEARCH, globalInstance, NULL);
	SendMessage(searchBox, EM_SETCUEBANNER, FALSE, (LPARAM)L"Search for files...");
	SendMessage(searchBox, WM_SETFONT, (WPARAM)hFont, TRUE);
	searchBtn = CreateWindowEx(0, L"BUTTON", L"Search", WS_CHILD | WS_VISIBLE, 300, 460, 80, 25, window, (HMENU)IDC_SEARCHBTN, globalInstance, NULL);
	SendMessage(searchBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
	npLabel = CreateWindowEx(0, L"STATIC", NOT_PLAYING_LABEL, SS_CENTER | WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, -5, 485, 400, 25, window, (HMENU)IDC_NPLABEL, globalInstance, NULL);
	SendMessage(npLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
	playButton = CreateWindowEx(0, L"BUTTON", PLAY_LABEL, WS_CHILD | WS_VISIBLE, 75, 515, 100, 30, window, (HMENU)IDC_PLAYBTN, globalInstance, NULL);
	SendMessage(playButton, WM_SETFONT, (WPARAM)hFont, TRUE);
	stopButton = CreateWindowEx(0, L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE, 195, 515, 100, 30, window, (HMENU)IDC_STOPBTN, globalInstance, NULL);
	SendMessage(stopButton, WM_SETFONT, (WPARAM)hFont, TRUE);
}

//GET SELECTED INDEX
int GetIndex() {
	LRESULT listItemsCount = SendMessage(filesListCtrl, LB_GETCOUNT, NULL, NULL);
	for (int i = 0; i < listItemsCount; i++) {
		if (SendMessage(filesListCtrl, LB_GETSEL, (WPARAM)i, NULL) > 0) {
			return i;
		}
	}
	return -1;
}

//REMOVE SELECTION
void UnloadSelection() {
	mciSendString(L"close mp3", NULL, 0, mainWindow);
	selected = -1;
}

//LOAD SELECTION
/*
* ERROR CODES
* -1 - invalid index
* -2 - file not found
* -3 - path not loaded
* -4 - file loading error
*/
int LoadSelection(int index) {
	UnloadSelection();
	if (index >= 0) {
		if (!path.empty()) {
			wstring fullPath = path + L"\\" + wstring(files[index].cFileName);
			WIN32_FIND_DATA fileData;
			HANDLE fileExist = FindFirstFile(fullPath.c_str(), &fileData);
			if (fileExist == INVALID_HANDLE_VALUE) {
				MessageBox(mainWindow, L"Selected file not found. Please make sure that it exists or it has not been moved or deleted. Press F5 to refresh the list and remove invalid entries.", L"File not found", MB_ICONERROR);
				return -2;
			}
			else {
				wstring fullCommandS = L"open \"" + fullPath + L"\" type mpegvideo alias mp3";
				LPCWSTR fullCommand = fullCommandS.c_str();

				if (mciSendString(fullCommand, NULL, 0, mainWindow) != 0) {
					UnloadSelection();
					MessageBox(mainWindow, L"Something went wrong while trying to play this file. Please make sure that the file is not corrupted.", L"File loading error", MB_ICONERROR);
					return -4;
				}
				selected = index;
				return 0;
			}
		}
		else {
			MessageBox(mainWindow, L"State access violation. The program will shut down.", L"Critical error", MB_ICONERROR);
			ExitProcess(0);
			return -3;
		}
	}
	else {
		return -1;
	}
}

//FILL THE LIST BOX
void GenerateList(vector<WIN32_FIND_DATA> list) {
	SendMessage(filesListCtrl, LB_RESETCONTENT, NULL, NULL);
	for (int i = 0; i < list.size(); i++) {
		wstring fileName = list[i].cFileName;
		if (!showExtensions) {
			int dotPos = fileName.find_last_of('.');
			fileName.erase(dotPos, 4);
		}
		SendMessage(filesListCtrl, LB_ADDSTRING, NULL, (LPARAM)fileName.c_str());
	}
}

//UNLOAD DIRECTORY
void UnloadDir() {
	SendMessage(filesListCtrl, LB_RESETCONTENT, NULL, NULL);
	path.clear();
	files.clear();
	if (selected != -1) {
		UnloadSelection();
	}
}

//LOAD DIRECTORY
/*
* ERROR CODES
* -1 - empty string
* -2 - invalid directory
* -3 - empty directory
*/
int LoadDir(wstring dir) {
	UnloadDir();
	if (!dir.empty()) {
		DWORD dirA = GetFileAttributes(dir.c_str());
		if (dirA & FILE_ATTRIBUTE_DIRECTORY) {
			for (int i = 0; i < EXT_LENGTH; i++) {
				wstring findPathS = dir + L"\\*." + ext[i];
				LPCWSTR findPath = findPathS.c_str();
				WIN32_FIND_DATA dirData;
				HANDLE finder = FindFirstFile(findPath, &dirData);
				if (finder != INVALID_HANDLE_VALUE) {
					do {
						files.push_back(dirData);
					} while (FindNextFile(finder, &dirData));
				}
			}
			if (files.empty()) {
				MessageBox(mainWindow, L"It looks like there are no files that can be opened by ClickPlay. Please choose another folder and try again.", L"No files", MB_ICONEXCLAMATION);
				files.clear();
				return -3;
			}
			std::sort(files.begin(), files.end(), alphabeticSort);
			GenerateList(files);
			path = dir;
			return 0;
		}
		else {
			MessageBox(mainWindow, L"Cannot access this directory.", L"Incorrect directory", MB_ICONERROR);
			return -2;
		}
	}
	else {
		MessageBox(mainWindow, L"There is no directory loaded. Use the BROWSE button to load a directory.", L"No directory", MB_ICONERROR);
		return -1;
	}
}

//INITALIZE FOLDER PICKER
int FolderBrowseDialog(HWND window) {
	BROWSEINFO dialog = { 0 };
	dialog.hwndOwner = window;
	dialog.ulFlags = BIF_NEWDIALOGSTYLE;
	dialog.lpszTitle = L"Pick a folder, that will be opened by the program...";
	LPITEMIDLIST pid = SHBrowseForFolder(&dialog);
	TCHAR currentPath[MAX_PATH];
	if (pid != 0) {
		SHGetPathFromIDList(pid, currentPath);
		if (LoadDir((wstring)currentPath) == 0) {
			SetCurrentDirectory(path.c_str());
			SetWindowText(dirLabel, path.c_str());
			return 0;
		}
		else {
			return -2;
		}
	}
	else {
		return -1;
	}
}

//SEARCH DIRECTORY
/*ERROR CODES
* -1 - no files found
* -2 - LoadDir error
* -3 - empty input
*/
int SearchDir(wstring phrase) {
	//PERFORM CHECKING
	if (path.empty()) {
		return -2;
	}
	if (phrase == currentSearchPhrase) {
		return 0;
	}
	//RELOAD DIR
	if (LoadDir(path) != 0) {
		return -2;
	}
	if (phrase.empty()) {
		return -3;
	}
	vector<WIN32_FIND_DATA> results;
	//GET FILES
	for (int i = 0; i < files.size(); i++) {
		if (wstring(files[i].cFileName).find(phrase) != wstring::npos) {
			results.push_back(files[i]);
		}
	}
	if (results.empty()) {
		MessageBox(mainWindow, L"There are no files that match the entered phrase.", L"No files", MB_ICONEXCLAMATION);
		currentSearchPhrase.clear();
		return -1;
	}
	std::sort(results.begin(), results.end(), alphabeticSort);
	files = results;
	currentSearchPhrase = phrase;
	//ADD FILTERED CONTENT
	GenerateList(files);
	return 0;
}

//GET SEARCH PHRASE FROM searchBox
wstring GetSearchPhrase() {
	__int64 searchLength = SendMessage(searchBox, WM_GETTEXTLENGTH, 0, NULL);
	TCHAR* searchPhrase = new TCHAR[searchLength + 1];
	SendMessage(searchBox, WM_GETTEXT, searchLength + 1, (LPARAM)searchPhrase);
	searchPhrase[(int)searchLength] = _T('\0');
	return wstring(searchPhrase);
}

//MESSAGE LOOP
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	int windowId, windowEvent, randIndex;
	switch (msg) {
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_CREATE:
			CreateControls(hwnd);
			break;
		case WM_COMMAND:
			windowId = LOWORD(wParam);
			windowEvent = HIWORD(wParam);

			switch (windowId) {
				case ID_PLIK_OPENDIR:
				case IDC_BROWSEBTN:
					switch (FolderBrowseDialog(mainWindow)) {
						case 0:
							SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
							break;
						case -2:
							SendMessage(mainWindow, STATE_CHANGE, -1, NULL);
							break;
					}
					break;
				case IDC_LIST:
					if (windowEvent == LBN_SELCHANGE) {
						if (selected != SendMessage(filesListCtrl, LB_GETCURSEL, 0, 0)) {
							if (currentState == 1 || currentState == 2) { currentState = 1; isQueued = TRUE; }
							if (LoadSelection(GetIndex()) == 0) {
								if (isOnClickPlayEnabled == TRUE) {
									SendMessage(mainWindow, STATE_CHANGE, 1, NULL);
								}
							}
							else {
								SendMessage(mainWindow, STATE_CHANGE, 0, 0);
							}
						}
					}
					break;
				case IDC_SEARCHBTN:
					if (SearchDir(GetSearchPhrase()) == -2) {
						SendMessage(mainWindow, STATE_CHANGE, -1, 0);
					}
					else {
						SendMessage(mainWindow, STATE_CHANGE, 0, 0);
					}
					break;
				case IDC_PLAYBTN:
					switch (currentState) {
						case 0:
							if (LoadSelection(GetIndex()) == 0) {
								SendMessage(mainWindow, STATE_CHANGE, 1, NULL);
							}
							else {
								SendMessage(mainWindow, STATE_CHANGE, 0, 0);
							}
							break;
						case 1:
							SendMessage(mainWindow, STATE_CHANGE, 2, NULL);
							break;
						case 2:
							SendMessage(mainWindow, STATE_CHANGE, 1, NULL);
							break;
					}
					break;
				case IDC_STOPBTN:
					isQueued = FALSE;
					UnloadSelection();
					SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
					SendMessage(filesListCtrl, LB_SETCURSEL, -1, 0);
					break;
				case IDM_EXIT:
					SendMessage(mainWindow, WM_CLOSE, NULL, NULL);
					break;
				case IDM_ABOUT:
					DialogBox(globalInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), mainWindow, About);
					break;
				case ID_REFRESH:
					if (LoadDir(path) != 0) {
						SendMessage(mainWindow, STATE_CHANGE, -1, 0);
					}
					if (SendMessage(searchBox, WM_GETTEXTLENGTH, 0, 0) > 0) {
						if (SearchDir(GetSearchPhrase()) == -2) {
							SendMessage(mainWindow, STATE_CHANGE, -1, 0);
						}
					}
					break;
				case ID_OPCJE_ONCLICK:
					if (isOnClickPlayEnabled) {
						CheckMenuItem(menuBar, ID_OPCJE_ONCLICK, MF_UNCHECKED);
						isOnClickPlayEnabled = FALSE;
					}
					else {
						CheckMenuItem(menuBar, ID_OPCJE_ONCLICK, MF_CHECKED);
						isOnClickPlayEnabled = TRUE;
					}
					break;
				case ID_OPCJE_LOOP:
					if (isLoopEnabled) {
						CheckMenuItem(menuBar, ID_OPCJE_LOOP, MF_UNCHECKED);
						isLoopEnabled = FALSE;
					}
					else {
						CheckMenuItem(menuBar, ID_OPCJE_LOOP, MF_CHECKED);
						isLoopEnabled = TRUE;
					}
					UnloadSelection();
					SendMessage(mainWindow, STATE_CHANGE, 0, 0);
					SendMessage(filesListCtrl, LB_SETCURSEL, -1, 0);
					break;
				case ID_OPCJE_SHOWEXT:
					if (showExtensions) {
						CheckMenuItem(menuBar, ID_OPCJE_SHOWEXT, MF_UNCHECKED);
						showExtensions = FALSE;
					}
					else {
						CheckMenuItem(menuBar, ID_OPCJE_SHOWEXT, MF_CHECKED);
						showExtensions = TRUE;
					}
					if (!path.empty() && !files.empty()) {
						GenerateList(files);
						if (selected != -1) {
							SendMessage(filesListCtrl, LB_SETCURSEL, selected, 0);
						}
					}
					break;
				case ID_PLIK_CLOSEDIR:
					SendMessage(mainWindow, STATE_CHANGE, -1, NULL);
					break;
				case ID_AKCJE_SRCLEAR:
					currentSearchPhrase.clear();
					SendMessage(searchBox, WM_SETTEXT, (WPARAM)L"", 0);
					SendMessage(mainWindow, STATE_CHANGE, 0, 0);
					if (LoadDir(path) != 0) {
						SendMessage(mainWindow, STATE_CHANGE, -1, 0);
					}
					break;
				case ID_AKCJE_RANDOM:
					if (currentState == 1) {
						isQueued = TRUE;
					}
					//GET SEED
					srand(time(NULL));
					//LOAD AND PLAY
					randIndex = rand() % files.size();
					if (randIndex == selected && randIndex + 1 < files.size()) { randIndex += 1; }
					if (LoadSelection(randIndex) == 0) {
						SendMessage(filesListCtrl, LB_SETCURSEL, randIndex, 0);
						SendMessage(mainWindow, STATE_CHANGE, 1, 0);
					}
					else {
						SendMessage(mainWindow, STATE_CHANGE, 0, 0);
					}
					break;
				case ID_OPCJE_AUTOPLAY:
					if (isAutoPlayEnabled) {
						CheckMenuItem(menuBar, ID_OPCJE_AUTOPLAY, MF_UNCHECKED);
						isAutoPlayEnabled = FALSE;
					}
					else {
						CheckMenuItem(menuBar, ID_OPCJE_AUTOPLAY, MF_CHECKED);
						isAutoPlayEnabled = TRUE;
					}
					UnloadSelection();
					SendMessage(mainWindow, STATE_CHANGE, 0, 0);
					SendMessage(filesListCtrl, LB_SETCURSEL, -1, 0);
					break;
				case ID_AKCJE_JUMP:
					DialogBox(globalInstance, MAKEINTRESOURCE(IDD_JUMPTODIALOG), mainWindow, JumpToProc);
					break;
			}
			break;
		case MM_MCINOTIFY:
			if (!isQueued) {
				if (isAutoPlayEnabled) {
					if (selected != -1 && !(selected + 1 > files.size() - 1)) {
						if (LoadSelection(selected + 1) == 0) {
							SendMessage(filesListCtrl, LB_SETCURSEL, selected, 0);
							SendMessage(mainWindow, STATE_CHANGE, 1, 0);
						}
						else {
							SendMessage(mainWindow, STATE_CHANGE, 0, 0);
						}
					}
					else if(isLoopEnabled && selected != -1) {
						if (LoadSelection(0) == 0) {
							SendMessage(filesListCtrl, LB_SETCURSEL, selected, 0);
							SendMessage(mainWindow, STATE_CHANGE, 1, 0);
						}
						else {
							SendMessage(mainWindow, STATE_CHANGE, 0, 0);
						}
					}
					else {
						if (selected != -1) {
							UnloadSelection();
							SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
							SendMessage(filesListCtrl, LB_SETCURSEL, -1, 0);
						}
					}
				}
				else {
					if (selected != -1) {
						UnloadSelection();
						SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
						SendMessage(filesListCtrl, LB_SETCURSEL, -1, 0);
					}
				}
			}
			else {
				isQueued = FALSE;
			}
			break;
		case STATE_CHANGE:
			switch ((int)wParam) {
				case -1:
					UnloadSelection();
					UnloadDir();

					EnableWindow(filesListCtrl, FALSE);
					EnableWindow(playButton, FALSE);
					EnableWindow(stopButton, FALSE);
					EnableWindow(searchBtn, FALSE);
					EnableMenuItem(menuBar, ID_REFRESH, MF_DISABLED);
					EnableMenuItem(menuBar, ID_PLIK_CLOSEDIR, MF_DISABLED);
					EnableMenuItem(menuBar, ID_AKCJE_RANDOM, MF_DISABLED);
					EnableMenuItem(menuBar, ID_AKCJE_SRCLEAR, MF_DISABLED);
					EnableMenuItem(menuBar, ID_AKCJE_JUMP, MF_DISABLED);
					SetWindowText(dirLabel, NO_DIR_LOADED_LABEL);
					SetWindowText(npLabel, NOT_PLAYING_LABEL);
					SetWindowText(playButton, PLAY_LABEL);
					break;
				case 0:
					if (!path.empty()) {
						EnableWindow(filesListCtrl, TRUE);
						EnableWindow(playButton, TRUE);
						EnableWindow(stopButton, TRUE);
						EnableWindow(searchBtn, TRUE);
						EnableMenuItem(menuBar, ID_REFRESH, MF_ENABLED);
						EnableMenuItem(menuBar, ID_PLIK_CLOSEDIR, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_RANDOM, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_SRCLEAR, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_JUMP, MF_DISABLED);
						SetWindowText(npLabel, NOT_PLAYING_LABEL);
						SetWindowText(playButton, PLAY_LABEL);
						break;
					}
					else {
						SendMessage(mainWindow, STATE_CHANGE, -1, 0);
						break;
					}
				case 1:
					if (!path.empty() && selected != -1) {
						int playerState = 0;
						if (currentState == 2) {
							playerState = mciSendString(L"resume mp3", NULL, 0, mainWindow);
						}
						else {
							if (isLoopEnabled && !isAutoPlayEnabled) {
								playerState = mciSendString(L"play mp3 repeat", NULL, 0, mainWindow);
							}
							else {
								playerState = mciSendString(L"play mp3 notify", NULL, 0, mainWindow);
							}
						}

						if (playerState != 0) {
							MessageBox(mainWindow, L"An error occurred while playing this file. Please make sure that the file is not corrupted.", L"Playback error", MB_ICONERROR);
							SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
							break;
						}

						EnableWindow(filesListCtrl, TRUE);
						EnableWindow(playButton, TRUE);
						EnableWindow(stopButton, TRUE);
						EnableWindow(searchBtn, TRUE);
						EnableMenuItem(menuBar, ID_REFRESH, MF_DISABLED);
						EnableMenuItem(menuBar, ID_PLIK_CLOSEDIR, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_RANDOM, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_SRCLEAR, MF_ENABLED);
						EnableMenuItem(menuBar, ID_AKCJE_JUMP, MF_ENABLED);
						SetWindowText(npLabel, files[selected].cFileName);
						SetWindowText(playButton, PAUSE_LABEL);
						break;
					}
					else {
						SendMessage(mainWindow, STATE_CHANGE, 0, NULL);
						break;
					}
				case 2:
					mciSendString(L"pause mp3", NULL, 0, mainWindow);

					wstring pauseStringWS = wstring(files[selected].cFileName) + L" [PAUSE]";
					LPCWSTR pauseString = pauseStringWS.c_str();

					EnableWindow(filesListCtrl, TRUE);
					EnableWindow(playButton, TRUE);
					EnableWindow(stopButton, TRUE);
					EnableWindow(searchBtn, TRUE);
					EnableMenuItem(menuBar, ID_REFRESH, MF_DISABLED);
					EnableMenuItem(menuBar, ID_PLIK_CLOSEDIR, MF_ENABLED);
					EnableMenuItem(menuBar, ID_AKCJE_RANDOM, MF_ENABLED);
					EnableMenuItem(menuBar, ID_AKCJE_SRCLEAR, MF_ENABLED);
					EnableMenuItem(menuBar, ID_AKCJE_JUMP, MF_ENABLED);
					SetWindowText(npLabel, pauseString);
					SetWindowText(playButton, L"Wznów");
					break;
			}
			currentState = (int)wParam;
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return NULL;
}

//JUMP TO DIALOG LOOP
INT_PTR CALLBACK JumpToProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (msg) {
		case WM_INITDIALOG:
			SetDlgItemText(hwnd, IDC_TSMIN, L"00");
			SetDlgItemText(hwnd, IDC_TSSEC, L"00");
			return (INT_PTR)TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				TCHAR* minB = new TCHAR[32];
				TCHAR* secB = new TCHAR[32];

				GetDlgItemText(hwnd, IDC_TSMIN, minB, 30);
				GetDlgItemText(hwnd, IDC_TSSEC, secB, 30);

				int min = 0;
				int sec = 0;

				try {
					min = std::stoi(minB);
					sec = std::stoi(secB);
						
					if (!(min >= 0 && min <= 999 && sec >= 0 && sec <= 59)) {
						MessageBox(mainWindow, L"Incorrect parameters.", L"Incorrect parameters", MB_ICONEXCLAMATION);
						EndDialog(hwnd, LOWORD(wParam));
						return (INT_PTR)TRUE;
					}
				}
				catch (...) {
					MessageBox(mainWindow, L"Incorrect parameters.", L"Incorrect parameters", MB_ICONEXCLAMATION);
					EndDialog(hwnd, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
				
				int timestamp = (((min * 60) + sec) * 1000);

				isQueued = TRUE;
				wstring mciCmd = L"play mp3 from " + std::to_wstring(timestamp);
				mciSendString(mciCmd.c_str(), NULL, 0, mainWindow);
				SendMessage(mainWindow, STATE_CHANGE, 1, 0);

				EndDialog(hwnd, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			else if (LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
	}
	return (INT_PTR)FALSE;
}

//ABOUT DIALOG LOOP
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message) {
		case WM_INITDIALOG:
			return (INT_PTR)TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

//SORTING FUNCTION
bool alphabeticSort(WIN32_FIND_DATA a, WIN32_FIND_DATA b) {
	wstring aStr = a.cFileName;
	wstring bStr = b.cFileName;
	wstring aBuf;
	wstring bBuf;
	int aNum = 0;
	int bNum = 0;
	for (int i = 0; i < aStr.size(); i++) {
		if (std::isdigit(static_cast<unsigned char>(aStr.at(i)))) {
			aBuf += aStr.at(i);
		}
		else {
			break;
		}
	}
	for (int i = 0; i < bStr.size(); i++) {
		if (std::isdigit(static_cast<unsigned char>(bStr.at(i)))) {
			bBuf += bStr.at(i);
		}
		else {
			break;
		}
	}
	if (!aBuf.empty()) {
		aNum = std::stoi(aBuf);
	}
	if (!bBuf.empty()) {
		bNum = std::stoi(bBuf);
	}
	if (aNum != 0 || bNum != 0) {
		return aNum < bNum;
	}
	return std::tolower(aStr.at(0)) < std::tolower(bStr.at(0));
}