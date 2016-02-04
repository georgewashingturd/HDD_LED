#include <windows.h>
#include "resource.h"



typedef int ( __cdecl * _MyInitCom) (void);
typedef void ( __cdecl * _MyUninitCom) (void);
typedef int ( __cdecl * _GetDiskPerfData) (DWORD * dwDiskBytesPerSec, DWORD * dwDiskReadBytesPerSec,DWORD * dwDiskWriteBytesPerSec);

_MyInitCom MyInitCom;
_MyUninitCom MyUninitCom;
_GetDiskPerfData GetDiskPerfData;

LPCTSTR szAppName = TEXT("HDD LED");
LPCTSTR szWndName = TEXT("HDD LED");

HMENU hmenu;//The menu handle
HWND mainhwnd;

NOTIFYICONDATA nid = {0};
int ledstate = -1;

HICON greenicon;
HICON redicon;
HICON idleicon;

static DWORD WINAPI HddActivityThread(void * clientarg)
{
    DWORD all, read, write;
    int newstate;

    
    while (1)
    {
        if (GetDiskPerfData(&all, &read, &write) != 0)
        {
            DestroyWindow(mainhwnd);
            return 0;
        }
        
        if (all == 0)
        {
            newstate = 0;
            nid.hIcon = idleicon;
        }
        else
        {
            if (read > write)
            {
                newstate = 1;
                nid.hIcon = greenicon;
            }
            else
            {
                newstate = 2;
                nid.hIcon = redicon;
            }
        }
        
        if (newstate != ledstate)
        {
            Shell_NotifyIcon(NIM_MODIFY, &nid);
            ledstate = newstate;
        }
        
        Sleep(100);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT WM_TASKBARCREATED;
	POINT pt;//To receive the coordinates of the mouse
	int ret;//The menu options for receiving the return value

	// Do not modify the TaskbarCreated system tray, which is a custom message
	WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
	switch (message)
	{
	case WM_CREATE: //Window creation time news
        mainhwnd = hwnd;
		nid.cbSize = sizeof(nid);
		nid.hWnd = hwnd;
		nid.uID = 0;
		nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;
		nid.uCallbackMessage = WM_USER;

        nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_IDLE));
		
        lstrcpy(nid.szTip, szAppName);
		Shell_NotifyIcon(NIM_ADD, &nid);
		hmenu = CreatePopupMenu();//Create menu
		
		AppendMenu(hmenu, MF_STRING, IDR_INFO,"MinGW version of Codegasm 6 HDD LED");
        AppendMenu(hmenu, MF_STRING, IDR_QUIT,"Quit"); // add two options for the menu
        
        greenicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_BUSY_GREEN));
        redicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_BUSY_RED));
        idleicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_IDLE));

        CreateThread(
                     NULL,
                     0,
                     HddActivityThread,
                     (void*)NULL,
                     0,
                     NULL);
		break;
        
	case WM_USER: //Continuous use of the procedure when the news
		if (lParam == WM_LBUTTONDOWN)
			MessageBox(NULL, TEXT("You can exit the double-click the tray!"), szAppName, MB_OK);
        
		if (lParam == WM_LBUTTONDBLCLK)//Double click on the tray news, exit
			SendMessage(hwnd, WM_CLOSE, wParam, lParam);
		
        if (lParam == WM_RBUTTONDOWN)
		{
			GetCursorPos(&pt);//Take the mouse coordinates
			::SetForegroundWindow(hwnd);//To solve the menu and click the menu does not disappear problem
			EnableMenuItem(hmenu,IDR_INFO,MF_GRAYED);//Let a grayed menu

			ret=TrackPopupMenu(hmenu,TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);//Display menu and the option to get ID
			if(ret==IDR_INFO) 
                MessageBox(hwnd, TEXT("This program was based on Barnacules's COdegasm #6"), szAppName, MB_OK);
			if(ret==IDR_QUIT)
                DestroyWindow(hwnd);
			if(ret==0)
                PostMessage(hwnd, WM_LBUTTONDOWN, 0, 0);
		}
		break;
	case WM_DESTROY: //Window destroyed when news
		Shell_NotifyIcon(NIM_DELETE, &nid);
        MyUninitCom();
		PostQuitMessage(0);
		break;
	default:
		/*
		* When the Explorer.exe after the collapse prevention program in the system, in the system tray icon will disappear
		*
		* Principle: Explorer.exe after reload will rebuild the system tray. When the system tray to all established within the system
		* Register to receive TaskbarCreated messages of the top window sends a message, we only need to capture this information, and rebuild the system
		* System tray icon. 
		*/
		if (message == WM_TASKBARCREATED)
			SendMessage(hwnd, WM_CREATE, wParam, lParam);
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				   LPSTR szCmdLine, int iCmdShow)
{
    HINSTANCE wmidll;
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;

    if ((wmidll = LoadLibrary("WMI_DLL.dll")) == NULL)
    {
        MessageBox(NULL, TEXT("Error loading WMI_DLL.dll"), szAppName, MB_ICONERROR);
        return 0;
    }
    
    if ((MyInitCom = (_MyInitCom) GetProcAddress(wmidll, "MyInitCom")) == NULL ||
        (MyUninitCom = (_MyUninitCom) GetProcAddress(wmidll, "MyUninitCom")) == NULL ||
        (GetDiskPerfData = (_GetDiskPerfData) GetProcAddress(wmidll, "GetDiskPerfData")) == NULL )
    {
        MessageBox(NULL, TEXT("Error function addresses from WMI_DLL.dll"), szAppName, MB_ICONERROR);
        return 0;
    }
    
    if (MyInitCom() != 0)
    {
        MessageBox(NULL, TEXT("Error initting COM"), szAppName, MB_ICONERROR);
        return 0;
    }

	HWND handle = FindWindow(NULL, szWndName);
	if (handle != NULL)
	{
		MessageBox(NULL, TEXT("Application is already running"), szAppName, MB_ICONERROR);
		return 0;
	}

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szAppName;

	if (!RegisterClass(&wndclass))
	{
		MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
		return 0;
	}

	// Use the WS_EX_TOOLWINDOW property to hide in the taskbar window program button here
	hwnd = CreateWindowEx(WS_EX_TOOLWINDOW,
		szAppName, szWndName,
		WS_POPUP,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, iCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

