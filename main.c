#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>

#include "quadrum_audio.h"
#include "quadrum_engine.h"
#include "quadrum_ui.h"

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(linker, "/subsystem:windows")
#endif

/* ========================================================================
   Window & Layout Configuration Constants
   ======================================================================== */

#define QUADRUM_CLASS_NAME       "QuadrumSynthWindow"
#define QUADRUM_WINDOW_TITLE     "quadrum"

#define QUADRUM_MIN_CLIENT_W     960
#define QUADRUM_MIN_CLIENT_H     460

#define QUADRUM_WINDOW_STYLE     (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX)
#define QUADRUM_WINDOW_EX_STYLE  (WS_EX_APPWINDOW)

#define QUADRUM_VERSION_STRING "1.0"

/* ========================================================================
   Font Resource Helpers
   ======================================================================== */

static const char* const kFontList[] = {
    "Inter.ttf",
    "inter.ttf",
    "Inter-VariableFont_opsz,wght.ttf"
};
#define FONT_COUNT ((int)(sizeof(kFontList) / sizeof(kFontList[0])))

static void load_application_fonts(void) {
    for (int i = 0; i < FONT_COUNT; i++) {
        AddFontResourceExA(kFontList[i], FR_PRIVATE, NULL);
    }
}

static void unload_application_fonts(void) {
    for (int i = 0; i < FONT_COUNT; i++) {
        RemoveFontResourceExA(kFontList[i], FR_PRIVATE, NULL);
    }
}

/* ========================================================================
   DPI Scaling Initialization
   ======================================================================== */

static void enable_dpi_awareness(void) {
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextProc setDpiContext = 
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (setDpiContext) {
            setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

/* ========================================================================
   Application Entry Point
   ======================================================================== */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev;
    (void)lpCmd;

    enable_dpi_awareness();

    INITCOMMONCONTROLSEX icc = { 
        .dwSize = sizeof(icc), 
        .dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES 
    };
    InitCommonControlsEx(&icc);

    load_application_fonts();
    audio_init();

    /* Register Window Class */
    WNDCLASSEXA wc = {
        .cbSize        = sizeof(WNDCLASSEXA),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = QuadrumWndProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(1)),
        .lpszClassName = QUADRUM_CLASS_NAME
    };

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Quadrum Error", MB_ICONERROR);
        audio_shutdown();
        unload_application_fonts();
        return 1;
    }

    /* Compute outer window bounds to guarantee exact client dimensions */
    RECT win_rc = { 0, 0, QUADRUM_MIN_CLIENT_W, QUADRUM_MIN_CLIENT_H };
    AdjustWindowRectEx(&win_rc, QUADRUM_WINDOW_STYLE, FALSE, QUADRUM_WINDOW_EX_STYLE);
    int init_w = win_rc.right - win_rc.left;
    int init_h = win_rc.bottom - win_rc.top;

    char windowTitle[64];
    snprintf(windowTitle, sizeof(windowTitle), "quadrum");

    HWND hwnd = CreateWindowExA(
        QUADRUM_WINDOW_EX_STYLE,
        QUADRUM_CLASS_NAME,
        windowTitle,
        QUADRUM_WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        init_w, init_h,
        NULL, NULL, hInst, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create application window.", "Quadrum Error", MB_ICONERROR);
        audio_shutdown();
        unload_application_fonts();
        return 1;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    /* Main Event Loop */
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    /* Clean Application Shutdown */
    audio_shutdown();
    unload_application_fonts();

    return (int)msg.wParam;
}