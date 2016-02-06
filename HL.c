#define _WIN32_DCOM

#include <windows.h>
#include "resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include <comdef.h>
#include <Wbemidl.h>


HRESULT                 hr = S_OK;
IWbemRefresher          *pRefresher = NULL;
IWbemConfigureRefresher *pConfig = NULL;
IWbemHiPerfEnum         *pEnum = NULL;
IWbemServices           *pNameSpace = NULL;
IWbemLocator            *pWbemLocator = NULL;
IWbemObjectAccess       **apEnumAccess = NULL;
BSTR                    bstrNameSpace = NULL;
long                    lID = 0;
DWORD                   dwNumReturned = 0;

void MyUninitCom(void)
{
	if (NULL != bstrNameSpace)
    {
        SysFreeString(bstrNameSpace);
    }

    if (NULL != apEnumAccess)
    {
        for (DWORD i = 0; i < dwNumReturned; i++)
        {
            if (apEnumAccess[i] != NULL)
            {
                apEnumAccess[i]->Release();
                apEnumAccess[i] = NULL;
            }
        }
        delete [] apEnumAccess;
    }
    if (NULL != pWbemLocator)
    {
        pWbemLocator->Release();
    }
    if (NULL != pNameSpace)
    {
        pNameSpace->Release();
    }
    if (NULL != pEnum)
    {
        pEnum->Release();
    }
    if (NULL != pConfig)
    {
        pConfig->Release();
    }
    if (NULL != pRefresher)
    {
        pRefresher->Release();
    }

    CoUninitialize();

    if (FAILED (hr))
    {
        wprintf (L"Error status=%08x\n",hr);
    }
}

int MyInitCom(void)
{
	    if (FAILED (hr = CoInitializeEx(NULL,COINIT_MULTITHREADED)))
    {
        goto CLEANUP;
    }

    if (FAILED (hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_NONE,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, 0)))
    {
        goto CLEANUP;
    }

    if (FAILED (hr = CoCreateInstance(
        CLSID_WbemLocator, 
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (void**) &pWbemLocator)))
    {
        goto CLEANUP;
    }

    // Connect to the desired namespace.
    bstrNameSpace = SysAllocString(L"\\\\.\\root\\cimv2");
    if (NULL == bstrNameSpace)
    {
        hr = E_OUTOFMEMORY;
        goto CLEANUP;
    }
    if (FAILED (hr = pWbemLocator->ConnectServer(
        bstrNameSpace,
        NULL, // User name
        NULL, // Password
        NULL, // Locale
        0L,   // Security flags
        NULL, // Authority
        NULL, // Wbem context
        &pNameSpace)))
    {
        goto CLEANUP;
    }
    pWbemLocator->Release();
    pWbemLocator=NULL;
    SysFreeString(bstrNameSpace);
    bstrNameSpace = NULL;

    if (FAILED (hr = CoCreateInstance(
        CLSID_WbemRefresher,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemRefresher, 
        (void**) &pRefresher)))
    {
        goto CLEANUP;
    }

    if (FAILED (hr = pRefresher->QueryInterface(
        IID_IWbemConfigureRefresher,
        (void **)&pConfig)))
    {
        goto CLEANUP;
    }

	// Add an enumerator to the refresher.
    if (FAILED (hr = pConfig->AddEnum(
        pNameSpace, 
        L"Win32_PerfFormattedData_PerfDisk_PhysicalDisk", 
        0, 
        NULL, 
        &pEnum, 
        &lID)))
    {
        goto CLEANUP;
    }
    pConfig->Release();
    pConfig = NULL;

	return 0;

CLEANUP:
	MyUninitCom();

	return -1;
}

int GetDiskPerfData(DWORD * dwDiskBytesPerSec, DWORD * dwDiskReadBytesPerSec,DWORD * dwDiskWriteBytesPerSec)
{
	// To add error checking,
    // check returned HRESULT below where collected.

    long                    lDiskBytesPerSecHandle = 0;
    long                    lDiskReadBytesPerSecHandle = 0;
    long                    lDiskWriteBytesPerSecHandle = 0;
    DWORD                   dwNumObjects = 0;
    DWORD                   i=0;
    int                     x=0;

	*dwDiskBytesPerSec = 0;
	*dwDiskReadBytesPerSec = 0;
	*dwDiskWriteBytesPerSec = 0;

    

    // Get a property handle for the VirtualBytes property.

    // Refresh the object ten times and retrieve the value.
    for(x = 0; x < 1; x++)
    {
        dwNumReturned = 0;
        dwNumObjects = 0;

        if (FAILED (hr =pRefresher->Refresh(0L)))
        {
            goto CLEANUP;
        }

        hr = pEnum->GetObjects(0L, 
            dwNumObjects, 
            apEnumAccess, 
            &dwNumReturned);
        // If the buffer was not big enough,
        // allocate a bigger buffer and retry.
        if (hr == WBEM_E_BUFFER_TOO_SMALL 
            && dwNumReturned > dwNumObjects)
        {
            apEnumAccess = new IWbemObjectAccess*[dwNumReturned];
            if (NULL == apEnumAccess)
            {
                hr = E_OUTOFMEMORY;
                goto CLEANUP;
            }
            SecureZeroMemory(apEnumAccess,
                dwNumReturned*sizeof(IWbemObjectAccess*));
            dwNumObjects = dwNumReturned;

            if (FAILED (hr = pEnum->GetObjects(0L, 
                dwNumObjects, 
                apEnumAccess, 
                &dwNumReturned)))
            {
                goto CLEANUP;
            }
        }
        else
        {
            if (hr == WBEM_S_NO_ERROR)
            {
                hr = WBEM_E_NOT_FOUND;
                goto CLEANUP;
            }
        }

        // First time through, get the handles.
        if (0 == x)
        {
            CIMTYPE DiskBytesPerSecType;
            if (FAILED (hr = apEnumAccess[0]->GetPropertyHandle(
                L"DiskBytesPerSec",
                &DiskBytesPerSecType,
                &lDiskBytesPerSecHandle)))
            {
                goto CLEANUP;
            }
            CIMTYPE DiskReadBytesPerSecType;
            if (FAILED (hr = apEnumAccess[0]->GetPropertyHandle(
                L"DiskReadBytesPerSec",
                &DiskReadBytesPerSecType,
                &lDiskReadBytesPerSecHandle)))
            {
                goto CLEANUP;
            }
            CIMTYPE DiskWriteBytesPerSecType;
            if (FAILED (hr = apEnumAccess[0]->GetPropertyHandle(
                L"DiskWriteBytesPerSec",
                &DiskWriteBytesPerSecType,
                &lDiskWriteBytesPerSecHandle)))
            {
                goto CLEANUP;
            }
        }
           
        for (i = 0; i < dwNumReturned; i++)
        {
            DWORD temp;
            if (FAILED (hr = apEnumAccess[i]->ReadDWORD(
                lDiskBytesPerSecHandle,
                &temp)))
            {
                goto CLEANUP;
            }
            *dwDiskBytesPerSec = *dwDiskBytesPerSec + temp;
			if (FAILED (hr = apEnumAccess[i]->ReadDWORD(
                lDiskReadBytesPerSecHandle,
                &temp)))
            {
                goto CLEANUP;
            }
            *dwDiskReadBytesPerSec = *dwDiskReadBytesPerSec + temp;
			if (FAILED (hr = apEnumAccess[i]->ReadDWORD(
                lDiskWriteBytesPerSecHandle,
                &temp)))
            {
                goto CLEANUP;
            }
            *dwDiskWriteBytesPerSec = *dwDiskWriteBytesPerSec + temp;

            //wprintf(L"Disk is using %lu %lu %lu %lu\n",
            //    *dwDiskBytesPerSec,*dwDiskReadBytesPerSec+*dwDiskWriteBytesPerSec,*dwDiskReadBytesPerSec,*dwDiskWriteBytesPerSec);

            // Done with the object
            apEnumAccess[i]->Release();
            apEnumAccess[i] = NULL;
        }

        if (NULL != apEnumAccess)
        {
            delete [] apEnumAccess;
            apEnumAccess = NULL;
        }

       // Sleep for a second.
       //Sleep(1000);
    }

// exit loop here

	return 0;
CLEANUP:

    MyUninitCom();

	return -1;
}


// ======================================================

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
		
		AppendMenu(hmenu, MF_STRING, IDR_INFO,"MinGW w64 version of Codegasm 6 HDD LED");
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
			MessageBox(NULL, TEXT("You can exit by double-clicking the tray!"), szAppName, MB_OK);
        
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
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;

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

