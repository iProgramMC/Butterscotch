#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

static HWND hWnd;

static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

int main(int argc, char* argv[])
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	static WNDCLASS windowClass;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = TEXT("Butterscotch");
	windowClass.hbrBackground = GetStockBrush(WHITE_BRUSH);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hIcon = NULL;
	
	if (!RegisterClass(&windowClass)) {
		fprintf(stderr, "Failed to register window class. Last error: %lu\n", GetLastError());
		return 1;
	}
	
	RECT rc;
	rc.left = rc.top = 0;
	rc.right = WINDOW_WIDTH;
	rc.bottom = WINDOW_HEIGHT;
	
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	
	hWnd = CreateWindow(
		TEXT("Butterscotch"), // Class Name
		TEXT("Butterscotch"), // Caption
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		NULL, // Parent
		NULL, // Menu
		hInstance,
		NULL  // LPParam
	);
	
	if (!hWnd) {
		fprintf(stderr, "Failed to create window. Last error: %lu\n", GetLastError());
		return 1;
	}
	
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	
	MSG msg = {};
	while (GetMessage (&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	return 0;
}

#endif
