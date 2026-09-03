#ifndef QUADRUM_UI_H
#define QUADRUM_UI_H

#include "quadrum_engine.h"
#include "quadrum_audio.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ========================================================================
   Constants – Dark Sequencer Palette
   ======================================================================== */

#define UI_COLOR_BG         RGB(22, 25, 32)     /* #161920 – balanced slate charcoal */
#define UI_COLOR_PANEL      RGB(22, 26, 33)     /* #161A21 – dark panel background */
#define UI_COLOR_PANEL_HDR  RGB(28, 33, 42)     /* #1C212A – card header ribbon */
#define UI_COLOR_BORDER     RGB(38, 46, 58)     /* #262E3A – refined subtle border */
#define UI_COLOR_ACCENT     RGB(56, 194, 224)   /* #38C2E0 – vibrant cyan from cseq */
#define UI_COLOR_ACCENT_DIM RGB(28, 92, 112)    /* #1C5C70 – dial trail color */
#define UI_COLOR_PAD_BG     RGB(20, 24, 31)     /* #14181F – voice pad unselected */
#define UI_COLOR_PAD_ACTIVE RGB(56, 194, 224)   /* #38C2E0 – flash color */
#define UI_COLOR_TEXT       RGB(230, 237, 243)  /* #E6EDF3 – primary text */
#define UI_COLOR_TEXT_DIM   RGB(138, 150, 166)  /* #8A96A6 – secondary text */
#define UI_COLOR_SCOPE_BG   RGB(10, 12, 15)     /* #0A0C0F – pitch-dark scope viewport */
#define UI_COLOR_SCOPE_GRID RGB(20, 25, 33)     /* #141921 – subtle scope grid */

#define UI_MARGIN           16
#define UI_MARGIN_DOUBLE    32
#define UI_CARD_GAP         8

/* ========================================================================
   Async WAV Export Job
   ======================================================================== */

typedef struct {
    char filepath[MAX_PATH];
    float buffer[QUADRUM_MAX_SAMPLES];
    int count;
    HWND notify_hwnd;
} ExportJob;

/* ========================================================================
   Types
   ======================================================================== */

typedef struct {
    int id;
    const char* label;
    const char* unit;
    double* param_ptr;
    double min_val;
    double max_val;
    int is_int;
    RECT rect;
} KnobCtrl;

typedef struct {
    VoiceType cur_voice;
    QuadrumParams params[VOICE_COUNT];
    float sample_buffer[QUADRUM_MAX_SAMPLES];
    int sample_count;

    float title_alpha[7]; /* Opacity (0.0 .. 1.0) for each letter in "quadrum" */

    float scope_cache[512];
    int scope_cache_len;

    DWORD logo_trigger_time;
    int   logo_step;
    int   logo_anim_element;  /* 0=left stick, 1=right stick, 2=drum rim */

    int active_knob_idx;
    int drag_start_y;
    double drag_start_val;

    int master_dragging;
    int master_drag_start_y;
    float master_drag_start_val;

    DWORD pad_flash_time[VOICE_COUNT];
    DWORD btn_flash_time[4];  /* 0=play, 1=export, 2=reset, 3=keybinds */

    int   hovered_btn;        /* 0=none, 1=play, 2=export, 3=reset, 4=keybinds */
    int   synth_dirty;        /* Deferred waveform synthesis flag */

    /* Async Background Export */
    volatile LONG export_in_progress;   /* Interlocked access (cross-thread) */
    volatile LONG export_progress;      /* Interlocked access (cross-thread) */
    DWORD        export_finish_time;
    char         export_filename[MAX_PATH];

    KnobCtrl knobs[16];
    int knob_count;

    HWND hwnd_main;
    HFONT font_regular;
    HFONT font_header;
    HFONT font_title;
    HFONT font_small;
} UIState;

/* ========================================================================
   Global State
   ======================================================================== */

static UIState g_ui = {0};
static float g_master_volume = 0.75f;   /* 0.0 .. 1.0 */

/* ========================================================================
   Forward Declarations
   ======================================================================== */

static void ui_synthesize(int play);
static void ui_init_knobs(void);
static void ui_update_knob_layout(int w, int h);
static void ui_paint(HWND hwnd, HDC hdc);

/* ========================================================================
   Helper Functions
   ======================================================================== */

static void get_master_knob_rect(int w, RECT* r) {
    int knob_w = 68;
    r->left = w - 368;
    r->right = r->left + knob_w;
    r->top = 12;
    r->bottom = 38;
}

static int is_knob_disabled(int idx) {
    if (g_ui.cur_voice == VOICE_CLAP && idx < 4)
        return 1;
    return 0;
}

static HFONT create_ui_font(int height) {
    HFONT hFont = CreateFontA(
        -height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Inter"
    );
    if (!hFont) {
        hFont = CreateFontA(
            -height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
        );
    }
    return hFont;
}

static COLORREF blend_color(COLORREF c_bg, COLORREF c_fg, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    int r = (int)(GetRValue(c_bg) + alpha * (GetRValue(c_fg) - GetRValue(c_bg)));
    int g = (int)(GetGValue(c_bg) + alpha * (GetGValue(c_fg) - GetGValue(c_bg)));
    int b = (int)(GetBValue(c_bg) + alpha * (GetBValue(c_fg) - GetBValue(c_bg)));
    return RGB(r, g, b);
}

static void draw_rounded_rect(HDC hdc, RECT r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_br = SelectObject(hdc, br);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, old_br);
    SelectObject(hdc, old_pen);
    DeleteObject(br);
    DeleteObject(pen);
}

/* ========================================================================
   Keybinds Popup Dialog
   ======================================================================== */

static HWND g_keybindsHwnd = NULL;

typedef struct {
    const char* key;
    const char* desc;
} KeybindRow;

static const KeybindRow kKeybindList[] = {
    { "1 .. 8",          "Select drum sound (Kick to Cymbal)" },
    { "Space / Enter",   "Audition current drum sound" },
    { "Ctrl + E",        "Export audio to 32-bit WAV" }, /* or "E" if you prefer single key */
    { "K",               "Open keybinds window" },
    { "Click + Drag",    "Adjust parameter" },
    { "Shift + Drag",    "Fine-tune parameter" },
    { "Mouse Wheel",     "Step adjust parameter (hover)" },
    { "Shift + Wheel",   "Fine-tune parameter (hover)" },
};
#define KEYBIND_COUNT ((int)(sizeof(kKeybindList) / sizeof(kKeybindList[0])))

static LRESULT CALLBACK KeybindsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w <= 0 || h <= 0) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            HBRUSH bg = CreateSolidBrush(UI_COLOR_BG);
            FillRect(memDC, &rc, bg);
            DeleteObject(bg);

            SetBkMode(memDC, TRANSPARENT);

            int midX = w / 2;
            int startY = 16;
            int rowH = 22;
            int halfCount = (KEYBIND_COUNT + 1) / 2;

            /* Center vertical divider */
            HPEN divPen = CreatePen(PS_SOLID, 1, UI_COLOR_BORDER);
            HGDIOBJ oldPen = SelectObject(memDC, divPen);
            MoveToEx(memDC, midX, startY - 2, NULL);
            LineTo(memDC, midX, startY + halfCount * rowH + 2);
            SelectObject(memDC, oldPen);
            DeleteObject(divPen);

            SelectObject(memDC, g_ui.font_regular ? g_ui.font_regular : GetStockObject(DEFAULT_GUI_FONT));

            for (int i = 0; i < KEYBIND_COUNT; ++i) {
                int isRightCol = (i >= halfCount);
                int row = isRightCol ? (i - halfCount) : i;
                int y = startY + row * rowH;

                RECT keyRc, descRc;
                if (!isRightCol) {
                    keyRc = (RECT){ 16, y, 120, y + rowH };
                    descRc = (RECT){ 134, y, midX - 16, y + rowH };
                } else {
                    keyRc = (RECT){ midX + 16, y, midX + 130, y + rowH };
                    descRc = (RECT){ midX + 144, y, w - 16, y + rowH };
                }

                /* Key Shortcut */
                SetTextColor(memDC, UI_COLOR_ACCENT);
                DrawTextA(memDC, kKeybindList[i].key, -1, &keyRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                /* Action Description */
                SetTextColor(memDC, UI_COLOR_TEXT_DIM);
                DrawTextA(memDC, kKeybindList[i].desc, -1, &descRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* Bottom Notice */
            SetTextColor(memDC, RGB(100, 115, 135));
            SelectObject(memDC, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
            RECT hintRc = { 0, h - 22, w, h - 4 };
            DrawTextA(memDC, "Press [ESC] or [ENTER] to close", -1, &hintRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            g_keybindsHwnd = NULL;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void open_keybinds_dialog(HWND parentHwnd) {
    if (!g_keybindsHwnd) {
        static int s_kbRegistered = 0;
        if (!s_kbRegistered) {
            WNDCLASSA wc = { 0 };
            wc.lpfnWndProc = KeybindsWndProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = "QuadrumKeybindsClass";
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            RegisterClassA(&wc);
            s_kbRegistered = 1;
        }

        int rw = 820, rh = 180;
        int scrW = GetSystemMetrics(SM_CXSCREEN);
        int scrH = GetSystemMetrics(SM_CYSCREEN);
        int rx = (scrW - rw) / 2;
        int ry = (scrH - rh) / 2;

        g_keybindsHwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            "QuadrumKeybindsClass",
            "quadrum - keybinds",
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            rx, ry, rw, rh,
            parentHwnd, NULL, GetModuleHandle(NULL), NULL
        );
    }

    ShowWindow(g_keybindsHwnd, SW_SHOW);
    SetForegroundWindow(g_keybindsHwnd);
    InvalidateRect(g_keybindsHwnd, NULL, FALSE);
}

/* ========================================================================
   Async Background WAV Export Worker
   ======================================================================== */

static DWORD WINAPI async_export_worker(LPVOID param) {
    ExportJob* job = (ExportJob*)param;
    if (!job) return 0;

    FILE* f = fopen(job->filepath, "wb");
    if (!f) {
        InterlockedExchange(&g_ui.export_in_progress, 0);
        free(job);
        return 0;
    }

    int32_t sr = QUADRUM_SR;
    int32_t data_size = job->count * (int32_t)sizeof(float);
    int32_t chunk_size = 36 + data_size;
    int16_t num_channels = 1;
    int16_t bits = 32;
    int16_t block_align = num_channels * (bits / 8);
    int16_t audio_format = 3; /* WAVE_FORMAT_IEEE_FLOAT */
    int32_t byte_rate = sr * block_align;
    int32_t subchunk_size = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&subchunk_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    int chunk_samples = 2048;
    int offset = 0;
    while (offset < job->count) {
        int to_write = job->count - offset;
        if (to_write > chunk_samples) to_write = chunk_samples;
        fwrite(&job->buffer[offset], sizeof(float), to_write, f);
        offset += to_write;
        InterlockedExchange(&g_ui.export_progress, (LONG)((int64_t)offset * 100 / job->count));
        Sleep(2);
    }

    fclose(f);

    InterlockedExchange(&g_ui.export_progress, 100);
    InterlockedExchange(&g_ui.export_in_progress, 0);
    g_ui.export_finish_time = GetTickCount();

    if (job->notify_hwnd) {
        InvalidateRect(job->notify_hwnd, NULL, FALSE);
    }

    free(job);
    return 0;
}

/* ========================================================================
   Drawing Functions
   ======================================================================== */

/* Animated Snare Drum Logo with Neon Flare */
static void draw_neon_drum_logo(HDC hdc, int x, int y, int size) {
    int ss = 4;
    int ss_size = size * ss;
    int ss_cx = ss_size / 2;
    int ss_cy = ss_size / 2;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_size;
    bmi.bmiHeader.biHeight = -ss_size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBmp || !memDC) {
        if (memDC) DeleteDC(memDC);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    RECT bg_rc = {0, 0, ss_size, ss_size};
    HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
    FillRect(memDC, &bg_rc, bg_br);
    DeleteObject(bg_br);

    DWORD elapsed = GetTickCount() - g_ui.logo_trigger_time;
    int is_glowing = (elapsed < 1000);

    int elem = g_ui.logo_anim_element;
    if (elem < 0) elem = 0;
    if (elem > 2) elem = 2;

    int glow_left  = is_glowing && (elem == 0);
    int glow_right = is_glowing && (elem == 1);
    int glow_drum  = is_glowing && (elem == 2);

    g_ui.logo_step = is_glowing ? elem : -1;

    COLORREF col_neon_stick = RGB(160, 245, 255);
    COLORREF col_neon_drum  = UI_COLOR_ACCENT;
    COLORREF col_idle_drum  = RGB(55, 95, 120);
    COLORREF col_idle_rope  = RGB(40, 70, 90);
    COLORREF col_idle_stick = RGB(75, 115, 140);

    /* Drum Shell */
    int drum_pen_w = glow_drum ? (3 * ss) : (2 * ss);
    HPEN drum_pen = CreatePen(PS_SOLID, drum_pen_w, glow_drum ? col_neon_drum : col_idle_drum);
    HGDIOBJ old_pen = SelectObject(memDC, drum_pen);

    HBRUSH head_br = CreateSolidBrush(glow_drum ? RGB(20, 42, 56) : RGB(16, 22, 30));
    HGDIOBJ old_br = SelectObject(memDC, head_br);
    Ellipse(memDC, ss_cx - 11 * ss, ss_cy - 7 * ss, ss_cx + 11 * ss, ss_cy + 3 * ss);
    SelectObject(memDC, old_br);
    DeleteObject(head_br);

    MoveToEx(memDC, ss_cx - 11 * ss, ss_cy - 2 * ss, NULL);
    LineTo(memDC, ss_cx - 10 * ss, ss_cy + 8 * ss);
    MoveToEx(memDC, ss_cx + 11 * ss, ss_cy - 2 * ss, NULL);
    LineTo(memDC, ss_cx + 10 * ss, ss_cy + 8 * ss);

    for (double a = 0.0; a <= 3.1416; a += 0.08) {
        int px = ss_cx + (int)(10.0 * ss * cos(3.1416 - a));
        int py = (ss_cy + 7 * ss) + (int)(4.0 * ss * sin(a));
        if (a == 0.0) MoveToEx(memDC, px, py, NULL);
        else LineTo(memDC, px, py);
    }

    HPEN rope_pen = CreatePen(PS_SOLID, 2 * ss, glow_drum ? RGB(160, 240, 255) : col_idle_rope);
    SelectObject(memDC, rope_pen);
    POINT rope_pts[7] = {
        {ss_cx - 10 * ss, ss_cy + 7 * ss},
        {ss_cx - 7 * ss,  ss_cy + 2 * ss},
        {ss_cx - 4 * ss,  ss_cy + 9 * ss},
        {ss_cx,           ss_cy + 3 * ss},
        {ss_cx + 4 * ss,  ss_cy + 9 * ss},
        {ss_cx + 7 * ss,  ss_cy + 2 * ss},
        {ss_cx + 10 * ss, ss_cy + 7 * ss}
    };
    Polyline(memDC, rope_pts, 7);

    SelectObject(memDC, old_pen);
    DeleteObject(drum_pen);
    DeleteObject(rope_pen);

    /* Sticks */
    int stick_w = glow_left ? (4 * ss) : (2 * ss);
    HPEN stick_l_pen = CreatePen(PS_SOLID | PS_ENDCAP_ROUND, stick_w, glow_left ? col_neon_stick : col_idle_stick);
    old_pen = SelectObject(memDC, stick_l_pen);
    MoveToEx(memDC, ss_cx - 13 * ss, ss_cy + 4 * ss, NULL);
    LineTo(memDC, ss_cx + 8 * ss, ss_cy - 8 * ss);
    Ellipse(memDC, ss_cx + 6 * ss, ss_cy - 11 * ss, ss_cx + 11 * ss, ss_cy - 6 * ss);

    SelectObject(memDC, old_pen);
    DeleteObject(stick_l_pen);

    stick_w = glow_right ? (4 * ss) : (2 * ss);
    HPEN stick_r_pen = CreatePen(PS_SOLID | PS_ENDCAP_ROUND, stick_w, glow_right ? col_neon_stick : col_idle_stick);
    old_pen = SelectObject(memDC, stick_r_pen);
    MoveToEx(memDC, ss_cx - 9 * ss, ss_cy - 8 * ss, NULL);
    LineTo(memDC, ss_cx + 13 * ss, ss_cy + 4 * ss);
    Ellipse(memDC, ss_cx - 12 * ss, ss_cy - 11 * ss, ss_cx - 7 * ss, ss_cy - 6 * ss);

    SelectObject(memDC, old_pen);
    DeleteObject(stick_r_pen);

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, x, y, size, size, memDC, 0, 0, ss_size, ss_size, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
}

/* Supersampled Antialiased Knob Renderer */
static void draw_knob(HDC hdc, KnobCtrl* k, int is_active, int is_disabled) {
    int kw = k->rect.right - k->rect.left;
    int kh = k->rect.bottom - k->rect.top;
    if (kw <= 0 || kh <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, is_disabled ? RGB(70, 80, 95) : UI_COLOR_TEXT);
    SelectObject(hdc, g_ui.font_regular);
    RECT lr = {k->rect.left, k->rect.top, k->rect.right, k->rect.top + 20};
    DrawTextA(hdc, k->label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int cx = (k->rect.left + k->rect.right) / 2;
    int cy = k->rect.top + 46;
    int r = 18;
    int dial_box = 44;

    double norm = (*k->param_ptr - k->min_val) / (k->max_val - k->min_val);
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;

    int ss = 3;
    int ss_dim = dial_box * ss;
    int ss_cx = ss_dim / 2;
    int ss_cy = ss_dim / 2;
    int ss_r = r * ss;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_dim;
    bmi.bmiHeader.biHeight = -ss_dim;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBmp && memDC) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        RECT bg_rc = {0, 0, ss_dim, ss_dim};
        HBRUSH bg_br = CreateSolidBrush(UI_COLOR_PANEL);
        FillRect(memDC, &bg_rc, bg_br);
        DeleteObject(bg_br);

        COLORREF track_fill = is_disabled ? RGB(18, 21, 26) : RGB(14, 16, 21);
        COLORREF track_border = is_disabled ? RGB(32, 38, 46) : UI_COLOR_BORDER;
        HBRUSH track_br = CreateSolidBrush(track_fill);
        HPEN track_pen = CreatePen(PS_SOLID, 1 * ss, track_border);
        HGDIOBJ old_br = SelectObject(memDC, track_br);
        HGDIOBJ old_pen = SelectObject(memDC, track_pen);
        Ellipse(memDC, ss_cx - ss_r, ss_cy - ss_r, ss_cx + ss_r, ss_cy + ss_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(track_br);
        DeleteObject(track_pen);

        double start_ang = 3.92699;   /* 225 degrees */
        double sweep_total = 4.71239; /* 270 degrees sweep */
        double cur_ang = start_ang - norm * sweep_total;

        COLORREF arc_color = is_disabled ? RGB(45, 54, 66) : (is_active ? UI_COLOR_ACCENT : UI_COLOR_ACCENT_DIM);
        HPEN arc_pen = CreatePen(PS_SOLID, 3 * ss, arc_color);
        old_pen = SelectObject(memDC, arc_pen);
        for (double a = start_ang; a >= cur_ang; a -= 0.05) {
            int ax = ss_cx + (int)((ss_r - 2 * ss) * cos(a));
            int ay = ss_cy - (int)((ss_r - 2 * ss) * sin(a));
            if (a == start_ang) MoveToEx(memDC, ax, ay, NULL);
            else LineTo(memDC, ax, ay);
        }
        SelectObject(memDC, old_pen);
        DeleteObject(arc_pen);

        COLORREF cap_fill = is_disabled ? RGB(22, 26, 32) : (is_active ? RGB(36, 44, 56) : RGB(28, 33, 42));
        COLORREF cap_border = is_disabled ? RGB(38, 46, 56) : (is_active ? UI_COLOR_ACCENT : UI_COLOR_BORDER);
        HBRUSH cap_br = CreateSolidBrush(cap_fill);
        HPEN cap_pen = CreatePen(PS_SOLID, 1 * ss, cap_border);
        old_br = SelectObject(memDC, cap_br);
        old_pen = SelectObject(memDC, cap_pen);
        int cap_r = ss_r - 5 * ss;
        Ellipse(memDC, ss_cx - cap_r, ss_cy - cap_r, ss_cx + cap_r, ss_cy + cap_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(cap_br);
        DeleteObject(cap_pen);

        HPEN needle_pen = CreatePen(PS_SOLID, 2 * ss, is_disabled ? RGB(65, 75, 90) : RGB(245, 248, 252));
        old_pen = SelectObject(memDC, needle_pen);
        MoveToEx(memDC, ss_cx, ss_cy, NULL);
        LineTo(memDC, ss_cx + (int)((ss_r - 6 * ss) * cos(cur_ang)),
                      ss_cy - (int)((ss_r - 6 * ss) * sin(cur_ang)));
        SelectObject(memDC, old_pen);
        DeleteObject(needle_pen);

        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        StretchBlt(hdc, cx - dial_box / 2, cy - dial_box / 2, dial_box, dial_box,
                   memDC, 0, 0, ss_dim, ss_dim, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
    }

    char vbuf[32];
    if (is_disabled) {
        if (k->id == 0 && g_ui.cur_voice == VOICE_KICK) {
            snprintf(vbuf, sizeof(vbuf), "%.0f Hz (sub)", *k->param_ptr);
        } else {
            snprintf(vbuf, sizeof(vbuf), "%.2f %s", *k->param_ptr, k->unit);
        }
    } else if (k->is_int) {
        snprintf(vbuf, sizeof(vbuf), "%d %s", (int)(*k->param_ptr + 0.5), k->unit);
    } else if (k->max_val >= 1000.0) {
        snprintf(vbuf, sizeof(vbuf), "%.0f %s", *k->param_ptr, k->unit);
    } else {
        snprintf(vbuf, sizeof(vbuf), "%.2f %s", *k->param_ptr, k->unit);
    }

    SetTextColor(hdc, is_disabled ? RGB(70, 80, 95) : UI_COLOR_TEXT);
    SelectObject(hdc, g_ui.font_regular);
    RECT vr = {k->rect.left, k->rect.top + 70, k->rect.right, k->rect.top + 88};
    DrawTextA(hdc, vbuf, -1, &vr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/* Master Volume Dial */
static void draw_master_volume_knob(HDC hdc, int w) {
    RECT r;
    get_master_knob_rect(w, &r);

    int radius = 10;
    int dial_box = 24;
    int cx = r.left + 14;
    int cy_center = (r.top + r.bottom) / 2;

    int ss = 3;
    int ss_dim = dial_box * ss;
    int ss_cx = ss_dim / 2;
    int ss_cy = ss_dim / 2;
    int ss_r = radius * ss;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_dim;
    bmi.bmiHeader.biHeight = -ss_dim;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBmp && memDC) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        RECT bg_rc = {0, 0, ss_dim, ss_dim};
        HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
        FillRect(memDC, &bg_rc, bg_br);
        DeleteObject(bg_br);

        HBRUSH track_br = CreateSolidBrush(UI_COLOR_PANEL);
        HPEN track_pen = CreatePen(PS_SOLID, 1 * ss, UI_COLOR_BORDER);
        HGDIOBJ old_br = SelectObject(memDC, track_br);
        HGDIOBJ old_pen = SelectObject(memDC, track_pen);
        Ellipse(memDC, ss_cx - ss_r, ss_cy - ss_r, ss_cx + ss_r, ss_cy + ss_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(track_br);
        DeleteObject(track_pen);

        float norm = g_master_volume;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        double start_ang = 3.92699;
        double sweep_total = 4.71239;
        double cur_ang = start_ang - norm * sweep_total;

        HPEN arc_pen = CreatePen(PS_SOLID, 2 * ss, UI_COLOR_ACCENT);
        old_pen = SelectObject(memDC, arc_pen);
        for (double a = start_ang; a >= cur_ang; a -= 0.04) {
            int ax = ss_cx + (int)((ss_r - 2 * ss) * cos(a));
            int ay = ss_cy - (int)((ss_r - 2 * ss) * sin(a));
            if (a == start_ang) MoveToEx(memDC, ax, ay, NULL);
            else LineTo(memDC, ax, ay);
        }
        SelectObject(memDC, old_pen);
        DeleteObject(arc_pen);

        HBRUSH cap_br = CreateSolidBrush(RGB(28, 33, 42));
        HPEN cap_pen = CreatePen(PS_SOLID, 1 * ss, UI_COLOR_BORDER);
        old_br = SelectObject(memDC, cap_br);
        old_pen = SelectObject(memDC, cap_pen);
        int cap_r = ss_r - 4 * ss;
        Ellipse(memDC, ss_cx - cap_r, ss_cy - cap_r, ss_cx + cap_r, ss_cy + cap_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(cap_br);
        DeleteObject(cap_pen);

        HPEN needle_pen = CreatePen(PS_SOLID, 2 * ss, RGB(245, 248, 252));
        old_pen = SelectObject(memDC, needle_pen);
        MoveToEx(memDC, ss_cx, ss_cy, NULL);
        LineTo(memDC, ss_cx + (int)((ss_r - 5 * ss) * cos(cur_ang)),
                      ss_cy - (int)((ss_r - 5 * ss) * sin(cur_ang)));
        SelectObject(memDC, old_pen);
        DeleteObject(needle_pen);

        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        StretchBlt(hdc, cx - dial_box / 2, cy_center - dial_box / 2, dial_box, dial_box,
                   memDC, 0, 0, ss_dim, ss_dim, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
    }

    /* Percentage label placed neatly beside the dial, vertically centered */
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%.0f%%", g_master_volume * 100.0f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UI_COLOR_TEXT_DIM);
    SelectObject(hdc, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
    RECT vr = {r.left + 30, r.top, r.right, r.bottom};
    DrawTextA(hdc, vbuf, -1, &vr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* Oscilloscope Display */
static void draw_oscilloscope(HDC hdc, RECT r) {
    draw_rounded_rect(hdc, r, UI_COLOR_SCOPE_BG, UI_COLOR_BORDER, 6);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, UI_COLOR_SCOPE_GRID);
    HGDIOBJ old_pen = SelectObject(hdc, grid_pen);

    int scope_header_h = 24;
    int wave_area_top = r.top + scope_header_h;
    int wave_area_bottom = r.bottom - 6;
    int mid_y = (wave_area_top + wave_area_bottom) / 2;

    MoveToEx(hdc, r.left + 8, mid_y, NULL);
    LineTo(hdc, r.right - 8, mid_y);

    int w = r.right - r.left - 16;
    for (int gx = r.left + 8; gx < r.right - 8; gx += w / 6) {
        MoveToEx(hdc, gx, wave_area_top + 4, NULL);
        LineTo(hdc, gx, wave_area_bottom - 2);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(grid_pen);

    if (g_ui.scope_cache_len > 1 && w > 0) {
        HPEN wave_pen = CreatePen(PS_SOLID | PS_JOIN_ROUND | PS_ENDCAP_ROUND, 2, UI_COLOR_ACCENT);
        old_pen = SelectObject(hdc, wave_pen);

        int max_amp = (wave_area_bottom - wave_area_top) / 2 - 4;
        if (max_amp < 6) max_amp = 6;

        POINT pts[512];
        int num_pts = g_ui.scope_cache_len;
        if (num_pts > 512) num_pts = 512;

        for (int i = 0; i < num_pts; i++) {
            pts[i].x = r.left + 8 + (int)((int64_t)i * w / (num_pts - 1));
            pts[i].y = mid_y - (int)(g_ui.scope_cache[i] * max_amp);
        }

        Polyline(hdc, pts, num_pts);

        SelectObject(hdc, old_pen);
        DeleteObject(wave_pen);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UI_COLOR_TEXT_DIM);
    SelectObject(hdc, g_ui.font_regular);

    double duration_ms = (double)g_ui.sample_count * 1000.0 / QUADRUM_SR;
    char meta[64];
    if (duration_ms >= 1000.0) {
        snprintf(meta, sizeof(meta), "%s  \xB7  %.2f s", VOICE_NAMES[g_ui.cur_voice], duration_ms / 1000.0);
    } else {
        snprintf(meta, sizeof(meta), "%s  \xB7  %.1f ms", VOICE_NAMES[g_ui.cur_voice], duration_ms);
    }

    /* Utilizes full header height (24px) with vertical centering to prevent text clipping */
    RECT tr = {r.left + 12, r.top, r.right - 12, r.top + scope_header_h};
    DrawTextA(hdc, meta, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* ========================================================================
   Main Interface Drawing
   ======================================================================== */

static void ui_paint(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    ui_update_knob_layout(w, h);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
    FillRect(memDC, &rc, bg_br);
    DeleteObject(bg_br);

    SetBkMode(memDC, TRANSPARENT);
    DWORD now = GetTickCount();

    /* 1. Header & Dynamic Logo (Per-Letter Opacity) */
    SelectObject(memDC, g_ui.font_title);
    
    const char title_str[] = "quadrum";
    int cur_x = UI_MARGIN;
    
    for (int i = 0; i < 7; i++) {
        SIZE ch_sz = {0};
        GetTextExtentPoint32A(memDC, &title_str[i], 1, &ch_sz);

        float a = g_ui.title_alpha[i];
        if (a <= 0.0f) a = 1.0f; /* Fallback default */

        COLORREF letter_col = blend_color(UI_COLOR_BG, UI_COLOR_TEXT, a);
        SetTextColor(memDC, letter_col);

        RECT ch_rc = {cur_x, 12, cur_x + ch_sz.cx, 38};
        DrawTextA(memDC, &title_str[i], 1, &ch_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        cur_x += ch_sz.cx;
    }

    /* Keep exact positioning for the neon snare logo */
    SIZE total_sz = {0};
    GetTextExtentPoint32A(memDC, title_str, 7, &total_sz);
    int logo_x = UI_MARGIN + total_sz.cx + 10;
    draw_neon_drum_logo(memDC, logo_x, 12, 26);

    /* 2. [keybinds] Button */
    RECT kb_btn = {logo_x + 36, 12, logo_x + 36 + 88, 38};
    int kb_flash = (now - g_ui.btn_flash_time[3] < 120);
    int kb_hover = (g_ui.hovered_btn == 4);
    COLORREF kb_fill = kb_flash ? UI_COLOR_PAD_ACTIVE : (kb_hover ? RGB(32, 38, 48) : UI_COLOR_PANEL);
    COLORREF kb_bdr  = (kb_flash || kb_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF kb_txt  = kb_flash ? RGB(10, 14, 18) : (kb_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, kb_btn, kb_fill, kb_bdr, 4);
    SetTextColor(memDC, kb_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[keybinds]", -1, &kb_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* 3. Action Buttons */
    RECT play_btn = {w - 290, 12, w - 204, 38};
    int play_flash = (now - g_ui.btn_flash_time[0] < 120);
    int play_hover = (g_ui.hovered_btn == 1);
    COLORREF play_fill = play_flash ? UI_COLOR_PAD_ACTIVE : (play_hover ? RGB(32, 38, 48) : UI_COLOR_PANEL);
    COLORREF play_bdr  = (play_flash || play_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF play_txt  = play_flash ? RGB(10, 14, 18) : (play_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, play_btn, play_fill, play_bdr, 4);
    SetTextColor(memDC, play_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[play]", -1, &play_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT exp_btn = {w - 196, 12, w - 110, 38};
    int exp_flash = (now - g_ui.btn_flash_time[1] < 120);
    int exp_hover = (g_ui.hovered_btn == 2);
    COLORREF exp_fill = exp_flash ? UI_COLOR_PAD_ACTIVE : (exp_hover ? RGB(32, 38, 48) : UI_COLOR_PANEL);
    COLORREF exp_bdr  = (exp_flash || exp_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF exp_txt  = exp_flash ? RGB(10, 14, 18) : (exp_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, exp_btn, exp_fill, exp_bdr, 4);
    SetTextColor(memDC, exp_txt);
    DrawTextA(memDC, "[export]", -1, &exp_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT rst_btn = {w - 102, 12, w - UI_MARGIN, 38};
    int rst_flash = (now - g_ui.btn_flash_time[2] < 120);
    int rst_hover = (g_ui.hovered_btn == 3);
    COLORREF rst_fill = rst_flash ? UI_COLOR_PAD_ACTIVE : (rst_hover ? RGB(32, 38, 48) : UI_COLOR_PANEL);
    COLORREF rst_bdr  = (rst_flash || rst_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF rst_txt  = rst_flash ? RGB(10, 14, 18) : (rst_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, rst_btn, rst_fill, rst_bdr, 4);
    SetTextColor(memDC, rst_txt);
    DrawTextA(memDC, "[reset]", -1, &rst_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    draw_master_volume_knob(memDC, w);

    /* 4. Drum Voice Pads */
    int pad_w = (w - UI_MARGIN_DOUBLE - (VOICE_COUNT - 1) * 8) / VOICE_COUNT;
    int pad_y = 48;
    int pad_h = 50;

    for (int i = 0; i < VOICE_COUNT; i++) {
        RECT pr = {UI_MARGIN + i * (pad_w + 8), pad_y,
                   UI_MARGIN + i * (pad_w + 8) + pad_w, pad_y + pad_h};
        
        int is_selected = (i == (int)g_ui.cur_voice);

        /* Clean dark surface; focused pad highlights only with cyan border */
        COLORREF fill   = is_selected ? RGB(24, 30, 39) : UI_COLOR_PAD_BG;
        COLORREF border = is_selected ? UI_COLOR_ACCENT : UI_COLOR_BORDER;

        draw_rounded_rect(memDC, pr, fill, border, 5);

        SetTextColor(memDC, is_selected ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
        SelectObject(memDC, g_ui.font_regular);
        RECT name_r = {pr.left, pr.top + 4, pr.right, pr.top + 26};
        DrawTextA(memDC, VOICE_NAMES[i], -1, &name_r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        char key_hint[16];
        snprintf(key_hint, sizeof(key_hint), "[%d]", i + 1);

        SetTextColor(memDC, UI_COLOR_TEXT_DIM);
        RECT key_r = {pr.left, pr.bottom - 22, pr.right, pr.bottom - 4};
        DrawTextA(memDC, key_hint, -1, &key_r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    /* 5. Oscilloscope */
    RECT scope_r = {UI_MARGIN, 108, w - UI_MARGIN, 190};
    draw_oscilloscope(memDC, scope_r);

    /* 6. Module Panels (Leaves 24px clearance for bottom status bar) */
    int card_y = 195;
    int bottom_bar_h = 24;
    int card_h = h - card_y - bottom_bar_h;
    if (card_h < 224) card_h = 224;
    int card_w = (w - UI_MARGIN_DOUBLE - (4 - 1) * UI_CARD_GAP) / 4;
    const char* module_titles[4] = {
        "oscillator",
        "noise",
        "filter",
        "envelope"
    };

    for (int col = 0; col < 4; col++) {
        RECT cr = {UI_MARGIN + col * (card_w + UI_CARD_GAP), card_y, UI_MARGIN + col * (card_w + UI_CARD_GAP) + card_w, card_y + card_h};
        draw_rounded_rect(memDC, cr, UI_COLOR_PANEL, UI_COLOR_BORDER, 6);

        RECT hr = {cr.left, cr.top, cr.right, cr.top + 22};
        draw_rounded_rect(memDC, hr, UI_COLOR_PANEL_HDR, UI_COLOR_BORDER, 6);
        SetTextColor(memDC, UI_COLOR_ACCENT);
        SelectObject(memDC, g_ui.font_regular);
        DrawTextA(memDC, module_titles[col], -1, &hr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 7. Knobs */
    for (int i = 0; i < g_ui.knob_count; i++) {
        int is_disabled = is_knob_disabled(i);
        draw_knob(memDC, &g_ui.knobs[i], (i == g_ui.active_knob_idx), is_disabled);
    }

    /* 8. Export Status Indicator (Clean, Readable Typography) */
    DWORD export_elapsed = GetTickCount() - g_ui.export_finish_time;
    if (g_ui.export_in_progress || (g_ui.export_finish_time > 0 && export_elapsed < 3000)) {
        char exp_str[256];
        COLORREF exp_col;
        if (g_ui.export_in_progress) {
            snprintf(exp_str, sizeof(exp_str), "Exporting... %d%%", g_ui.export_progress);
            exp_col = UI_COLOR_ACCENT;
        } else {
            snprintf(exp_str, sizeof(exp_str), "File saved to %s", g_ui.export_filename);
            exp_col = RGB(80, 220, 160);
        }

        /* Use font_regular (14px) or font_header (13px) for clear readability */
        SelectObject(memDC, g_ui.font_regular ? g_ui.font_regular : g_ui.font_header);
        SetTextColor(memDC, exp_col);

        /* Vertically centered in the 24px bottom margin with auto-ellipsis for long paths */
        RECT exp_rc = {UI_MARGIN, h - 22, w - UI_MARGIN, h - 2};
        DrawTextA(memDC, exp_str, -1, &exp_rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

/* ========================================================================
   UI Layout & Interaction Management
   ======================================================================== */

static void ui_update_knob_layout(int w, int h) {
    (void)h;
    int card_y = 195;
    int card_w = (w - UI_MARGIN_DOUBLE - (4 - 1) * UI_CARD_GAP) / 4;
    int knob_w = 80;

    for (int col = 0; col < 4; col++) {
        int cx = UI_MARGIN + col * (card_w + UI_CARD_GAP);
        int half_w = card_w / 2;

        for (int row = 0; row < 2; row++) {
            for (int sub = 0; sub < 2; sub++) {
                int idx = col * 4 + row * 2 + sub;
                int kx = cx + sub * half_w + (half_w - knob_w) / 2;
                int ky = card_y + 25 + row * 100;
                g_ui.knobs[idx].rect = (RECT){kx, ky, kx + knob_w, ky + 88};
            }
        }
    }
}

static void ui_init_knobs(void) {
    QuadrumParams* p = &g_ui.params[g_ui.cur_voice];
    g_ui.knob_count = 16;

    #define SET_KNOB(idx, lbl, unt, ptr, mn, mx, is_i) do { \
        g_ui.knobs[idx].id = idx; \
        g_ui.knobs[idx].label = lbl; \
        g_ui.knobs[idx].unit = unt; \
        g_ui.knobs[idx].param_ptr = ptr; \
        g_ui.knobs[idx].min_val = mn; \
        g_ui.knobs[idx].max_val = mx; \
        g_ui.knobs[idx].is_int = is_i; \
    } while (0)

    double pitch_max = (g_ui.cur_voice == VOICE_KICK) ? 120.0 : 520.0;

    SET_KNOB(0,  "pitch",       "Hz",  &p->pitch,         20.0,  pitch_max, 0);
    SET_KNOB(1,  "sweep",       "%",   &p->pitch_env,     0.0,   1.0,    0);
    SET_KNOB(2,  "pitch decay", "s",   &p->pitch_decay,   0.002, 0.35,   0);
    SET_KNOB(3,  "fm depth",    "x",   &p->fm_depth,      0.0,   8.0,    0);
    SET_KNOB(4,  "noise mix",   "%",   &p->noise_mix,     0.0,   1.0,    0);
    SET_KNOB(5,  "noise decay", "s",   &p->noise_decay,   0.005, 1.2,    0);
    SET_KNOB(6,  "noise cutoff","Hz",  &p->noise_cutoff,  100.0, 16000.0,0);
    SET_KNOB(7,  "click snap",  "%",   &p->click,         0.0,   1.0,    0);
    SET_KNOB(8,  "cutoff",      "Hz",  &p->filter_cutoff, 100.0, 18000.0,0);
    SET_KNOB(9,  "resonance",   "Q",   &p->filter_q,      0.3,   8.0,    0);
    SET_KNOB(10, "drive sat",   "x",   &p->drive,         1.0,   5.0,    0);
    SET_KNOB(11, "fm ratio",    "x",   &p->fm_ratio,      0.5,   6.0,    0);
    SET_KNOB(12, "amp decay",   "s",   &p->decay,         0.01,  1.8,    0);
    SET_KNOB(13, "clap taps",   "",    &p->clap_taps,     1.0,   5.0,    1);
    SET_KNOB(14, "tap spread",  "s",   &p->clap_spread,   0.005, 0.035,  0);
    SET_KNOB(15, "filter type", "",    &p->filter_type,   0.0,   2.0,    1);

    #undef SET_KNOB

    if (g_ui.hwnd_main) {
        RECT rc;
        GetClientRect(g_ui.hwnd_main, &rc);
        ui_update_knob_layout(rc.right - rc.left, rc.bottom - rc.top);
    }
}

static void ui_synthesize(int play) {
    g_ui.sample_count = quadrum_render(&g_ui.params[g_ui.cur_voice], g_ui.sample_buffer, QUADRUM_MAX_SAMPLES);

    if (g_master_volume != 1.0f) {
        for (int i = 0; i < g_ui.sample_count; i++) {
            g_ui.sample_buffer[i] *= g_master_volume;
        }
    }

    int num_pts = 512;
    if (num_pts > g_ui.sample_count) num_pts = g_ui.sample_count;
    g_ui.scope_cache_len = num_pts;

    if (num_pts > 0 && g_ui.sample_count > 0) {
        for (int i = 0; i < num_pts; i++) {
            int s_idx = (int)((int64_t)i * g_ui.sample_count / num_pts);
            g_ui.scope_cache[i] = g_ui.sample_buffer[s_idx];
        }
    }

    if (play && g_ui.sample_count > 0) {
        /* Jitter letter opacities within a readable range [0.45 .. 1.00] */
        for (int i = 0; i < 7; i++) {
            g_ui.title_alpha[i] = 0.45f + ((float)rand() / (float)RAND_MAX) * 0.55f;
        }

        g_ui.pad_flash_time[g_ui.cur_voice] = GetTickCount();
        g_ui.btn_flash_time[0] = GetTickCount();
        g_ui.logo_trigger_time = GetTickCount();

        g_ui.logo_anim_element++;
        if (g_ui.logo_anim_element > 2) g_ui.logo_anim_element = 0;
        g_ui.logo_step = g_ui.logo_anim_element;

        audio_play(g_ui.sample_buffer, g_ui.sample_count);
        if (g_ui.hwnd_main) {
            SetTimer(g_ui.hwnd_main, 101, 130, NULL);
            SetTimer(g_ui.hwnd_main, 102, 30, NULL);
        }
    }

    if (g_ui.hwnd_main) {
        InvalidateRect(g_ui.hwnd_main, NULL, FALSE);
    }
}

static void ui_select_voice(VoiceType voice, int play) {
    if (voice < 0 || voice >= VOICE_COUNT) return;

    g_ui.cur_voice = voice;
    g_ui.pad_flash_time[voice] = GetTickCount();
    ui_init_knobs();

    if (g_ui.active_knob_idx >= 0 && g_ui.active_knob_idx < g_ui.knob_count) {
        g_ui.drag_start_val = *g_ui.knobs[g_ui.active_knob_idx].param_ptr;
        POINT pt;
        if (GetCursorPos(&pt) && g_ui.hwnd_main) {
            ScreenToClient(g_ui.hwnd_main, &pt);
            g_ui.drag_start_y = pt.y;
        }
    }

    ui_synthesize(play);
}

static void ui_trigger_export(HWND hwnd) {
    if (InterlockedCompareExchange(&g_ui.export_in_progress, 0, 0) != 0) return;

    OPENFILENAMEA ofn;
    char szFile[MAX_PATH];
    char voice_clean[32];
    strncpy(voice_clean, VOICE_NAMES[g_ui.cur_voice], sizeof(voice_clean) - 1);
    voice_clean[sizeof(voice_clean) - 1] = '\0';
    for (char* cp = voice_clean; *cp; cp++) {
        if (*cp == ' ') *cp = '_';
    }

    snprintf(szFile, sizeof(szFile), "quadrum_%s_%lu.wav", voice_clean, (unsigned long)time(NULL));

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Wave Audio (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "wav";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameA(&ofn)) {
        ExportJob* job = (ExportJob*)malloc(sizeof(ExportJob));
        if (!job) return;

        strncpy(job->filepath, ofn.lpstrFile, MAX_PATH - 1);
        job->filepath[MAX_PATH - 1] = '\0';

        const char* base_name = strrchr(job->filepath, '\\');
        if (!base_name) base_name = strrchr(job->filepath, '/');
        if (base_name) base_name++;
        else base_name = job->filepath;
        strncpy(g_ui.export_filename, base_name, sizeof(g_ui.export_filename) - 1);
        g_ui.export_filename[sizeof(g_ui.export_filename) - 1] = '\0';

        job->count = g_ui.sample_count;
        memcpy(job->buffer, g_ui.sample_buffer, job->count * sizeof(float));
        job->notify_hwnd = hwnd;

        InterlockedExchange(&g_ui.export_in_progress, 1);
        InterlockedExchange(&g_ui.export_progress, 0);
        g_ui.export_finish_time = 0;

        HANDLE hThread = CreateThread(NULL, 0, async_export_worker, job, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
            SetTimer(hwnd, 103, 30, NULL);
        } else {
            free(job);
            InterlockedExchange(&g_ui.export_in_progress, 0);
        }
    }
}

/* ========================================================================
   High‑DPI Alpha Icon Generator
   ======================================================================== */

static HICON create_app_icon(int size) {
    int ss = 4;
    int hi_w = size * ss;
    int hi_h = size * ss;

    HDC hdcScreen = GetDC(NULL);
    HDC hiDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = hi_w;
    bmi.bmiHeader.biHeight = -hi_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* hiBits = NULL;
    HBITMAP hBmpHi = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &hiBits, NULL, 0);
    HGDIOBJ oldHi = SelectObject(hiDC, hBmpHi);

    /* Clean dark background matching cseq */
    RECT bg_rc = {0, 0, hi_w, hi_h};
    HBRUSH bg_br = CreateSolidBrush(RGB(15, 17, 21));
    FillRect(hiDC, &bg_rc, bg_br);
    DeleteObject(bg_br);

    /* Inset rounded panel */
    int inset = 2 * ss;
    RECT panel_rc = {inset, inset, hi_w - inset, hi_h - inset};
    HBRUSH pad_br = CreateSolidBrush(RGB(22, 26, 33));
    HPEN pad_pen = CreatePen(PS_SOLID, 2 * ss, RGB(56, 194, 224));
    HGDIOBJ old_br = SelectObject(hiDC, pad_br);
    HGDIOBJ old_pen = SelectObject(hiDC, pad_pen);
    RoundRect(hiDC, panel_rc.left, panel_rc.top, panel_rc.right, panel_rc.bottom, 4 * ss, 4 * ss);
    SelectObject(hiDC, old_br);
    SelectObject(hiDC, old_pen);
    DeleteObject(pad_br);
    DeleteObject(pad_pen);

    /* Transient waveform */
    int pen_w = (size <= 16) ? (2 * ss) : (3 * ss);
    HPEN wave_pen = CreatePen(PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, pen_w, UI_COLOR_ACCENT);
    old_pen = SelectObject(hiDC, wave_pen);

    int margin_x = 4 * ss;
    int draw_w = hi_w - 2 * margin_x;
    int mid_y = hi_h / 2;
    int max_amp = (hi_h / 2) - (6 * ss);

    for (int x = 0; x < draw_w; x++) {
        double norm_x = (double)x / (double)draw_w;
        double env = exp(-norm_x * 2.6);
        double osc = sin(norm_x * QUADRUM_TWO_PI * 2.5);
        int y = mid_y - (int)(osc * env * max_amp);

        int px = margin_x + x;
        if (x == 0) MoveToEx(hiDC, px, y, NULL);
        else LineTo(hiDC, px, y);
    }

    SelectObject(hiDC, old_pen);
    DeleteObject(wave_pen);

    /* High-quality 32-bit ARGB downsample */
    HDC finalDC = CreateCompatibleDC(hdcScreen);
    BITMAPINFO finalBmi = bmi;
    finalBmi.bmiHeader.biWidth = size;
    finalBmi.bmiHeader.biHeight = -size;
    void* pFinalBits = NULL;
    HBITMAP hFinalBmp = CreateDIBSection(hdcScreen, &finalBmi, DIB_RGB_COLORS, &pFinalBits, NULL, 0);
    HGDIOBJ oldFinal = SelectObject(finalDC, hFinalBmp);

    SetStretchBltMode(finalDC, HALFTONE);
    SetBrushOrgEx(finalDC, 0, 0, NULL);
    StretchBlt(finalDC, 0, 0, size, size, hiDC, 0, 0, hi_w, hi_h, SRCCOPY);

    BYTE* p = (BYTE*)pFinalBits;
    for (int i = 0; i < size * size; i++) {
        p[i * 4 + 3] = 255;
    }

    HBITMAP hMask = CreateBitmap(size, size, 1, 1, NULL);
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = hMask;
    ii.hbmColor = hFinalBmp;
    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(finalDC, oldFinal);
    DeleteDC(finalDC);
    DeleteObject(hFinalBmp);
    DeleteObject(hMask);

    SelectObject(hiDC, oldHi);
    DeleteDC(hiDC);
    DeleteObject(hBmpHi);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

/* ========================================================================
   Window Procedure
   ======================================================================== */

static LRESULT CALLBACK QuadrumWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

        case WM_CREATE: {

            srand((unsigned)time(NULL)); /* Seed PRNG once for title animation jitter */

            for (int i = 0; i < 7; i++) {
            g_ui.title_alpha[i] = 1.0f;
            }

            g_ui.hwnd_main = hwnd;
            g_ui.active_knob_idx = -1;
            g_ui.logo_step = -1;
            g_ui.logo_anim_element = 0;
            g_ui.master_dragging = 0;

            int sm_icon_sz = GetSystemMetrics(SM_CXSMICON);
            int lg_icon_sz = GetSystemMetrics(SM_CXICON);
            HICON hIconSm = create_app_icon(sm_icon_sz ? sm_icon_sz : 16);
            HICON hIconLg = create_app_icon(lg_icon_sz ? lg_icon_sz : 32);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
            SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconLg);

            for (int i = 0; i < VOICE_COUNT; i++) {
                quadrum_get_preset((VoiceType)i, &g_ui.params[i]);
            }

            g_ui.cur_voice = VOICE_KICK;
            ui_init_knobs();

            // Font baselines
            g_ui.font_regular = create_ui_font(14);
            g_ui.font_header  = create_ui_font(13);
            g_ui.font_title   = create_ui_font(24);
            g_ui.font_small   = create_ui_font(12); /* Bumped from 10 to 12 */

            ui_synthesize(0);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            RECT rc = { 0, 0, 960, 460 };
            AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX, FALSE, WS_EX_APPWINDOW);
            int fixed_h = rc.bottom - rc.top;
            int min_w   = rc.right - rc.left;

            mmi->ptMinTrackSize.x = min_w;
            mmi->ptMinTrackSize.y = fixed_h;
            mmi->ptMaxTrackSize.y = fixed_h; /* Locks vertical height */
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcA(hwnd, msg, wParam, lParam);
            if (hit == HTTOP || hit == HTBOTTOM) return HTBORDER;
            if (hit == HTTOPLEFT || hit == HTBOTTOMLEFT) return HTLEFT;
            if (hit == HTTOPRIGHT || hit == HTBOTTOMRIGHT) return HTRIGHT;
            return hit;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ui_update_knob_layout(rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            ui_paint(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 104) {
                KillTimer(hwnd, 104);
                if (g_ui.synth_dirty) {
                    g_ui.synth_dirty = 0;
                    ui_synthesize(0);
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    RECT scope_r = {UI_MARGIN, 108, rc.right - UI_MARGIN, 190};
                    InvalidateRect(hwnd, &scope_r, FALSE);
                }
            }
            if (wParam == 101) {
                KillTimer(hwnd, 101);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (wParam == 102) {
                DWORD elapsed = GetTickCount() - g_ui.logo_trigger_time;
                if (elapsed >= 1000) {
                    KillTimer(hwnd, 102);
                    g_ui.logo_step = -1;
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            if (wParam == 103) {
                if (InterlockedCompareExchange(&g_ui.export_in_progress, 0, 0) != 0) {
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    DWORD done_elapsed = GetTickCount() - g_ui.export_finish_time;
                    if (done_elapsed >= 3000) {
                        KillTimer(hwnd, 103);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right;

            SIZE title_sz = {0};
            HDC hdcScr = GetDC(hwnd);
            SelectObject(hdcScr, g_ui.font_title);
            GetTextExtentPoint32A(hdcScr, "quadrum", 7, &title_sz);
            ReleaseDC(hwnd, hdcScr);
            int logo_x = UI_MARGIN + title_sz.cx + 10;
            RECT kb_btn = {logo_x + 36, 12, logo_x + 36 + 88, 38};

            if (PtInRect(&kb_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[3] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                open_keybinds_dialog(hwnd);
                return 0;
            }

            RECT play_btn = {w - 290, 12, w - 204, 38};
            if (PtInRect(&play_btn, (POINT){x, y})) {
                g_ui.pad_flash_time[g_ui.cur_voice] = GetTickCount();
                g_ui.btn_flash_time[0] = GetTickCount();
                ui_synthesize(1);
                return 0;
            }

            RECT exp_btn = {w - 196, 12, w - 110, 38};
            if (PtInRect(&exp_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[1] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                ui_trigger_export(hwnd);
                return 0;
            }

            RECT rst_btn = {w - 102, 12, w - UI_MARGIN, 38};
            if (PtInRect(&rst_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[2] = GetTickCount();
                quadrum_get_preset(g_ui.cur_voice, &g_ui.params[g_ui.cur_voice]);
                ui_init_knobs();
                ui_synthesize(1);
                return 0;
            }

            RECT master_rect;
            get_master_knob_rect(w, &master_rect);
            if (PtInRect(&master_rect, (POINT){x, y})) {
                g_ui.master_dragging = 1;
                g_ui.master_drag_start_y = y;
                g_ui.master_drag_start_val = g_master_volume;
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            int pad_w = (w - UI_MARGIN_DOUBLE - (VOICE_COUNT - 1) * 8) / VOICE_COUNT;
            int pad_y = 48;
            int pad_h = 50;
            for (int i = 0; i < VOICE_COUNT; i++) {
                RECT pr = {UI_MARGIN + i * (pad_w + 8), pad_y, UI_MARGIN + i * (pad_w + 8) + pad_w, pad_y + pad_h};
                if (PtInRect(&pr, (POINT){x, y})) {
                    ui_select_voice((VoiceType)i, 1);
                    return 0;
                }
            }

            for (int i = 0; i < g_ui.knob_count; i++) {
                if (is_knob_disabled(i)) continue;
                if (PtInRect(&g_ui.knobs[i].rect, (POINT){x, y})) {
                    g_ui.active_knob_idx = i;
                    g_ui.drag_start_y = y;
                    g_ui.drag_start_val = *g_ui.knobs[i].param_ptr;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_ui.hovered_btn != 0) {
                g_ui.hovered_btn = 0;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            double notches = zDelta / 120.0;

            RECT rc;
            GetClientRect(hwnd, &rc);
            RECT master_rect;
            get_master_knob_rect(rc.right, &master_rect);
            
            if (PtInRect(&master_rect, pt)) {
                float sens = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.015f : 0.06f;
                float new_val = g_master_volume + (float)(notches * sens);
                if (new_val < 0.0f) new_val = 0.0f;
                if (new_val > 1.0f) new_val = 1.0f;
                g_master_volume = new_val;
                InvalidateRect(hwnd, &master_rect, FALSE);
                return 0;
            }

            for (int i = 0; i < g_ui.knob_count; i++) {
                if (is_knob_disabled(i)) continue;
                if (!PtInRect(&g_ui.knobs[i].rect, pt)) continue;

                KnobCtrl* k = &g_ui.knobs[i];
                double new_val;

                if (k->is_int) {
                    double step = (notches > 0.0) ? 1.0 : -1.0;
                    new_val = floor(*k->param_ptr + 0.5) + step;
                } else {
                    double range = k->max_val - k->min_val;
                    double sens  = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.015 : 0.06;
                    new_val = *k->param_ptr + notches * range * sens;
                }

                if (new_val < k->min_val) new_val = k->min_val;
                if (new_val > k->max_val) new_val = k->max_val;

                *k->param_ptr = new_val;

                /* Only redraw the hovered knob and queue throttled waveform sync */
                InvalidateRect(hwnd, &k->rect, FALSE);
                g_ui.synth_dirty = 1;
                SetTimer(hwnd, 104, 30, NULL);
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right;

            SIZE title_sz = {0};
            HDC hdcScr = GetDC(hwnd);
            SelectObject(hdcScr, g_ui.font_title);
            GetTextExtentPoint32A(hdcScr, "quadrum", 7, &title_sz);
            ReleaseDC(hwnd, hdcScr);
            int logo_x = UI_MARGIN + title_sz.cx + 10;
            RECT kb_btn = {logo_x + 36, 12, logo_x + 36 + 88, 38};

            RECT play_btn = {w - 290, 12, w - 204, 38};
            RECT exp_btn  = {w - 196, 12, w - 110, 38};
            RECT rst_btn  = {w - 102, 12, w - UI_MARGIN, 38};

            int new_hover = 0;
            if (PtInRect(&play_btn, (POINT){x, y})) new_hover = 1;
            else if (PtInRect(&exp_btn, (POINT){x, y})) new_hover = 2;
            else if (PtInRect(&rst_btn, (POINT){x, y})) new_hover = 3;
            else if (PtInRect(&kb_btn, (POINT){x, y})) new_hover = 4;

            if (new_hover != g_ui.hovered_btn) {
                g_ui.hovered_btn = new_hover;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            if ((wParam & MK_LBUTTON) == 0)
                break;

            if (g_ui.master_dragging) {
                int dy = g_ui.master_drag_start_y - y;
                float sensitivity = 0.004f;
                float new_val = g_ui.master_drag_start_val + dy * sensitivity;
                if (new_val < 0.0f) new_val = 0.0f;
                if (new_val > 1.0f) new_val = 1.0f;
                g_master_volume = new_val;
                
                RECT master_rect;
                get_master_knob_rect(w, &master_rect);
                InvalidateRect(hwnd, &master_rect, FALSE);
                return 0;
            }

            if (g_ui.active_knob_idx >= 0) {
                KnobCtrl* k = &g_ui.knobs[g_ui.active_knob_idx];
                int dy = g_ui.drag_start_y - y;

                double range = k->max_val - k->min_val;
                double sensitivity = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.0015 : 0.006;
                double new_val = g_ui.drag_start_val + dy * range * sensitivity;

                if (new_val < k->min_val) new_val = k->min_val;
                if (new_val > k->max_val) new_val = k->max_val;
                if (k->is_int) new_val = floor(new_val + 0.5);

                *k->param_ptr = new_val;

                /* Redraw only the dragged knob immediately to prevent latency */
                InvalidateRect(hwnd, &k->rect, FALSE);

                /* Defer heavy DSP synthesis so it doesn't block the UI thread */
                g_ui.synth_dirty = 1;
                SetTimer(hwnd, 104, 30, NULL);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (g_ui.master_dragging) {
                g_ui.master_dragging = 0;
                ReleaseCapture();
            }
            if (g_ui.active_knob_idx >= 0) {
                g_ui.active_knob_idx = -1;
                ReleaseCapture();
            }

            /* Immediately flush waveform preview when mouse is released */
            if (g_ui.synth_dirty) {
                g_ui.synth_dirty = 0;
                KillTimer(hwnd, 104);
                ui_synthesize(0);
                RECT rc;
                GetClientRect(hwnd, &rc);
                RECT scope_r = {UI_MARGIN, 108, rc.right - UI_MARGIN, 190};
                InvalidateRect(hwnd, &scope_r, FALSE);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_SPACE || wParam == VK_RETURN) {
                g_ui.pad_flash_time[g_ui.cur_voice] = GetTickCount();
                g_ui.btn_flash_time[0] = GetTickCount();
                ui_synthesize(1);
                return 0;
            }
            if (wParam == 'E' || wParam == 'e') {
                g_ui.btn_flash_time[1] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                ui_trigger_export(hwnd);
                return 0;
            }
            if (wParam == 'K' || wParam == 'k') {
                g_ui.btn_flash_time[3] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                open_keybinds_dialog(hwnd);
                return 0;
            }
            if (wParam >= '1' && wParam <= '8') {
                int voice = (int)(wParam - '1');
                ui_select_voice((VoiceType)voice, 1);
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            HICON hIconSm = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
            HICON hIconLg = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
            if (hIconSm) DestroyIcon(hIconSm);
            if (hIconLg) DestroyIcon(hIconLg);

            if (g_ui.font_regular) DeleteObject(g_ui.font_regular);
            if (g_ui.font_header)  DeleteObject(g_ui.font_header);
            if (g_ui.font_title)   DeleteObject(g_ui.font_title);
            if (g_ui.font_small)   DeleteObject(g_ui.font_small);
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

#endif /* QUADRUM_UI_H */