/*
 * PDFReader — minimal native PDF reader for Windows (x64)
 *
 * Scope (per user): open PDF + window reading + F11 presentation. Nothing else.
 * Engine : MuPDF 1.27 (linked statically, PDF handler only, base-14 fonts)
 * UI    : Win32 + GDI (no external dependencies, no WebView)
 * Build : x86_64-w64-mingw32-gcc (cross-compiled on Linux), static, stripped
 *
 * License: engine = AGPL (MuPDF); this shell + build scripts = public domain-ish,
 *          whole binary is AGPL — fine for personal/private USB use.
 */
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
/* SetProcessDPIAware is Vista+; load dynamically so the exe still runs on XP */
typedef BOOL (WINAPI *pfnSetProcessDPIAware)(void);
static void enable_dpi_aware(void) {
    HMODULE h = GetModuleHandleA("user32.dll");
    pfnSetProcessDPIAware fn = h ? (pfnSetProcessDPIAware)GetProcAddress(h, "SetProcessDPIAware") : NULL;
    if (fn) fn();
}

#include <mupdf/fitz.h>
extern const fz_document_handler pdf_document_handler;

/* ------------------------------------------------------------------ */
/* globals                                                            */
/* ------------------------------------------------------------------ */
static fz_context *g_ctx = NULL;
static fz_document *g_doc = NULL;
static int g_pages = 0;
static int g_page = 0;              /* 0-based current page */
static int g_pres = 0;              /* presentation mode */
static int g_fit = 0;               /* 0 = fit page, 1 = fit width */
static float g_zoomMul = 1.0f;      /* manual zoom multiplier */
static wchar_t g_path[1024] = {0};
static HWND g_hwnd = NULL;

/* cached rendered page */
static HBITMAP g_bmp = NULL;
static int g_bmpW = 0, g_bmpH = 0;
static int g_bmpPage = -1;

static const wchar_t *kClassName = L"PDFReaderClass";
static const wchar_t *kTitle     = L"SoloCorePDF";

enum { IDM_OPEN = 1, IDM_PRES, IDM_FITPAGE, IDM_FITWIDTH, IDM_ZOOMIN, IDM_ZOOMOUT, IDM_EXIT };

static void build_menu(HWND hwnd);   /* defined below */

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
static char *utf8_from_w(const wchar_t *ws, char *buf, int n) {
    if (!ws || !*ws) { buf[0] = 0; return buf; }
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf, n, NULL, NULL);
    return buf;
}

static void msg_error(const char *utf8msg) {
    wchar_t wbuf[1024];
    MultiByteToWideChar(CP_UTF8, 0, utf8msg, -1, wbuf, 1024);
    MessageBoxW(g_hwnd, wbuf, kTitle, MB_OK | MB_ICONERROR);
}

/* mupdf exception -> message box */
static void fail(void) {
    msg_error(fz_caught_message(g_ctx));
}

static void set_title(void) {
    wchar_t t[64];
    if (g_doc) {
        if (g_pres)
            wsprintfW(t, L"page %d/%d [Presentation]", g_page + 1, g_pages);
        else
            wsprintfW(t, L"page %d/%d", g_page + 1, g_pages);
    } else {
        wcscpy(t, L"PDF");
    }
    SetWindowTextW(g_hwnd, t);
}

/* render page into a 32bpp top-down DIB; returns bitmap handle (caller owns),
   real bitmap size written to *outW/*outH */
static HBITMAP render_page(int page, int clientW, int clientH, int *outW, int *outH) {
    if (!g_doc || clientW <= 0 || clientH <= 0) return NULL;

    fz_rect r;
    fz_page *pg = fz_load_page(g_ctx, g_doc, page);
    r = fz_bound_page(g_ctx, pg);
    fz_drop_page(g_ctx, pg);
    float pw = r.x1 - r.x0, ph = r.y1 - r.y0;
    if (pw <= 0 || ph <= 0) return NULL;

    /* zoom in device pixels per point */
    float z;
    if (g_fit == 1) { /* fit width */
        z = (float)clientW / (pw * 96.0f / 72.0f);
    } else {          /* fit page */
        z = (float)clientW / (pw * 96.0f / 72.0f);
        float zh = (float)clientH / (ph * 96.0f / 72.0f);
        if (zh < z) z = zh;
    }
    z *= g_zoomMul;

    fz_matrix m = fz_scale(z * 96.0f / 72.0f, z * 96.0f / 72.0f);
    fz_pixmap *pix = fz_new_pixmap_from_page_number(g_ctx, g_doc, page, m, fz_device_rgb(g_ctx), 0);
    int w = pix->w, h = pix->h, n = pix->n;   /* 3 = RGB */
    const unsigned char *src = pix->samples;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;          /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HDC hdc = GetDC(g_hwnd);
    HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bmp && bits) {
        /* RGB -> BGRA, copy rows top-down using pixmap stride */
        for (int y = 0; y < h; y++) {
            const unsigned char *s = src + (size_t)y * (size_t)pix->stride;
            unsigned char *d = (unsigned char *)bits + (size_t)y * (size_t)w * 4;
            for (int x = 0; x < w; x++) {
                d[x*4+0] = s[x*n+2]; /* B */
                d[x*4+1] = s[x*n+1]; /* G */
                d[x*4+2] = s[x*n+0]; /* R */
                d[x*4+3] = 0xFF;
            }
        }
    }
    ReleaseDC(g_hwnd, hdc);
    fz_drop_pixmap(g_ctx, pix);
    if (outW) *outW = w;
    if (outH) *outH = h;
    return bmp;
}

/* drop cached bitmap */
static void drop_bmp(void) {
    if (g_bmp) { DeleteObject(g_bmp); g_bmp = NULL; }
    g_bmpW = g_bmpH = 0; g_bmpPage = -1;
}

/* ------------------------------------------------------------------ */
/* document open                                                       */
/* ------------------------------------------------------------------ */
static void open_file(const wchar_t *wpath) {
    char utf8[2048];
    utf8_from_w(wpath, utf8, sizeof utf8);

    fz_document *ndoc = NULL;
    fz_try(g_ctx) {
        ndoc = fz_open_document(g_ctx, utf8);
        int n = fz_count_pages(g_ctx, ndoc);
        if (n < 1)
            fz_throw(g_ctx, FZ_ERROR_GENERIC, "document has no pages");
        if (g_doc) { fz_drop_document(g_ctx, g_doc); g_doc = NULL; }
        g_doc = ndoc;
        g_pages = n;
        g_page = 0;
        g_zoomMul = 1.0f;
        wcscpy(g_path, wpath);
        drop_bmp();
        InvalidateRect(g_hwnd, NULL, TRUE);
        set_title();
    }
    fz_catch(g_ctx) {
        fail();
        if (ndoc) fz_drop_document(g_ctx, ndoc);
    }
}

static void open_dialog(void) {
    OPENFILENAMEW ofn;
    wchar_t file[2048] = {0};
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"PDF files (*.pdf)\0*.pdf\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 2048;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn))
        open_file(file);
}

/* ------------------------------------------------------------------ */
/* presentation                                                        */
/* ------------------------------------------------------------------ */
static void enter_presentation(void) {
    g_pres = 1;
    SetMenu(g_hwnd, NULL);           /* hide menu bar in presentation */
    /* remember and go borderless fullscreen on the window's monitor */
    HMONITOR mon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    SetWindowLongW(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(g_hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    drop_bmp();
    InvalidateRect(g_hwnd, NULL, TRUE);
    set_title();
}

static void leave_presentation(void) {
    g_pres = 0;
    SetWindowLongW(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(g_hwnd, NULL, 100, 100, 900, 700, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    build_menu(g_hwnd);              /* restore menu bar */
    drop_bmp();
    InvalidateRect(g_hwnd, NULL, TRUE);
    set_title();
}

static void toggle_presentation(void) {
    if (g_pres) leave_presentation();
    else enter_presentation();
}

/* ------------------------------------------------------------------ */
/* navigation                                                          */
/* ------------------------------------------------------------------ */
static void goto_page(int p) {
    if (!g_doc) return;
    if (p < 0) p = 0;
    if (p >= g_pages) p = g_pages - 1;
    if (p == g_page) return;
    g_page = p;
    drop_bmp();
    InvalidateRect(g_hwnd, NULL, TRUE);
    set_title();
}

static void next_page(void) { goto_page(g_page + 1); }
static void prev_page(void) { goto_page(g_page - 1); }

/* ------------------------------------------------------------------ */
/* window proc                                                         */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp;
        wchar_t f[2048];
        if (DragQueryFileW(hd, 0, f, 2048) > 0)
            open_file(f);
        DragFinish(hd);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_OPEN:  open_dialog(); return 0;
        case IDM_PRES:  toggle_presentation(); return 0;
        case IDM_FITPAGE:  g_fit = 0; g_zoomMul = 1.0f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case IDM_FITWIDTH: g_fit = 1; g_zoomMul = 1.0f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case IDM_ZOOMIN:  g_zoomMul *= 1.25f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case IDM_ZOOMOUT: g_zoomMul /= 1.25f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case IDM_EXIT:  PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
        }
        return 0;

    case WM_KEYDOWN:
        switch (wp) {
        case VK_PRIOR: case VK_LEFT:  case VK_UP:    prev_page(); return 0;
        case VK_NEXT:  case VK_RIGHT: case VK_DOWN:
        case VK_SPACE: case VK_RETURN:                next_page(); return 0;
        case VK_HOME:  goto_page(0); return 0;
        case VK_END:   goto_page(g_pages - 1); return 0;
        case VK_ADD: case VK_OEM_PLUS: g_zoomMul *= 1.25f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case VK_SUBTRACT: case VK_OEM_MINUS: g_zoomMul /= 1.25f; drop_bmp(); InvalidateRect(hwnd, NULL, TRUE); return 0;
        case VK_F11: toggle_presentation(); return 0;
        case VK_ESCAPE: if (g_pres) { leave_presentation(); return 0; } break;
        case 0x4F: /* 'O' */ if (GetKeyState(VK_CONTROL) < 0) { open_dialog(); return 0; } break;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_pres) { next_page(); return 0; }
        break;

    case WM_RBUTTONDOWN:
        if (g_pres) { prev_page(); return 0; }
        break;

    case WM_MOUSEWHEEL:
        if ((short)HIWORD(wp) < 0) next_page();
        else prev_page();
        return 0;

    case WM_SETCURSOR:
        if (g_pres) { SetCursor(NULL); return TRUE; }   /* hide cursor in presentation */
        break;

    case WM_SIZE:
        drop_bmp();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;

        /* background: black in presentation, white otherwise */
        HBRUSH bg = CreateSolidBrush(g_pres ? RGB(0,0,0) : RGB(255,255,255));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        if (g_doc) {
            if (!g_bmp || g_bmpPage != g_page || g_bmpW != cw || g_bmpH != ch) {
                drop_bmp();
                int rw = 0, rh = 0;
                g_bmp = render_page(g_page, cw, ch, &rw, &rh);
                g_bmpPage = g_page;
                g_bmpW = rw; g_bmpH = rh;
                if (!g_bmp) goto done_paint;
            }
            /* center the page bitmap */
            HDC mdc = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mdc, g_bmp);
            int x = (cw - g_bmpW) / 2, y = (ch - g_bmpH) / 2;
            BitBlt(hdc, x, y, g_bmpW, g_bmpH, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            DeleteDC(mdc);
        }
    done_paint:
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        drop_bmp();
        if (g_doc) fz_drop_document(g_ctx, g_doc);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/* entry                                                               */
/* ------------------------------------------------------------------ */
static void build_menu(HWND hwnd) {
    HMENU mb = CreateMenu();
    HMENU mf = CreateMenu();
    AppendMenuW(mf, MF_STRING, IDM_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(mf, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mf, MF_STRING, IDM_EXIT, L"E&xit");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mf, L"&File");

    HMENU mv = CreateMenu();
    AppendMenuW(mv, MF_STRING, IDM_PRES, L"&Presentation\tF11");
    AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mv, MF_STRING, IDM_FITPAGE, L"Fit &Page");
    AppendMenuW(mv, MF_STRING, IDM_FITWIDTH, L"Fit &Width");
    AppendMenuW(mv, MF_STRING, IDM_ZOOMIN, L"Zoom &In\t+");
    AppendMenuW(mv, MF_STRING, IDM_ZOOMOUT, L"Zoom &Out\t-");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)mv, L"&View");

    SetMenu(hwnd, mb);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, wchar_t *cmd, int nShow) {
    (void)hPrev;
    enable_dpi_aware();

    g_ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!g_ctx) { MessageBoxW(NULL, L"failed to init MuPDF", kTitle, MB_OK | MB_ICONERROR); return 1; }
    fz_register_document_handler(g_ctx, &pdf_document_handler);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize        = sizeof wc;
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCE(1));
    wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCE(1));
    wc.lpszClassName = kClassName;
    wc.hbrBackground = NULL;
    if (!RegisterClassExW(&wc)) { MessageBoxW(NULL, L"RegisterClass failed", kTitle, MB_OK); return 1; }

    g_hwnd = CreateWindowW(kClassName, kTitle, WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
                           NULL, NULL, hInst, NULL);
    if (!g_hwnd) { MessageBoxW(NULL, L"CreateWindow failed", kTitle, MB_OK); return 1; }
    build_menu(g_hwnd);
    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    /* command line arg -> open */
    if (cmd && *cmd) {
        /* strip quotes */
        wchar_t buf[2048];
        wcscpy(buf, cmd);
        int n = (int)wcslen(buf);
        if (n >= 2 && buf[0] == L'"' && buf[n-1] == L'"') { buf[n-1] = 0; memmove(buf, buf+1, (n-1)*sizeof(wchar_t)); }
        open_file(buf);
    }

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    fz_drop_context(g_ctx);
    return (int)m.wParam;
}
