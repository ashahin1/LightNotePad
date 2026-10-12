#include "pch.hpp"
#include "shortcuts_dialog.hpp"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace {
const wchar_t* const SHORTCUTS_CLASS_NAME = L"LightNotePad_ShortcutsDialog";

struct ShortcutItem {
    const wchar_t* keys;
    const wchar_t* desc;
};

struct ShortcutCategory {
    const wchar_t* title;
    std::vector<ShortcutItem> items;
};

const ShortcutCategory g_categories[] = {
    {
        L"File Operations",
        {
            { L"Ctrl + N / T", L"New Tab" },
            { L"Ctrl + Shift + N", L"New Window" },
            { L"Ctrl + O", L"Open File" },
            { L"Ctrl + S", L"Save" },
            { L"Ctrl + Shift + S", L"Save As" },
            { L"Ctrl + W", L"Close Tab" },
        }
    },
    {
        L"Edit & Text",
        {
            { L"Ctrl + Z / Y", L"Undo / Redo" },
            { L"Ctrl + X / C / V", L"Cut / Copy / Paste" },
            { L"Ctrl + A", L"Select All" },
            { L"F5", L"Insert Time & Date" },
        }
    },
    {
        L"Search & Navigation",
        {
            { L"Ctrl + F", L"Find" },
            { L"F3 / Shift + F3", L"Find Next / Previous" },
            { L"Ctrl + H", L"Replace" },
            { L"Ctrl + Tab", L"Next Tab" },
            { L"Ctrl + Shift + Tab", L"Previous Tab" },
        }
    },
    {
        L"View & Zoom",
        {
            { L"Alt + Z", L"Toggle Word Wrap" },
            { L"Ctrl + Mouse Wheel", L"Zoom In / Out" },
            { L"Ctrl + + / -", L"Zoom In / Out" },
            { L"Ctrl + 0", L"Reset Zoom (100%)" },
            { L"F1", L"Keyboard Shortcuts" },
        }
    }
};

struct DialogData {
    bool darkMode = false;
    UINT dpi = 96;
    HFONT hFontTitle = nullptr;
    HFONT hFontHeader = nullptr;
    HFONT hFontReg = nullptr;
    HFONT hFontKey = nullptr;
};
} // namespace

void ShortcutsDialog::Show(HWND hParent, HINSTANCE hInstance, bool darkMode, UINT dpi) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = ShortcutsDialog::DialogProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = SHORTCUTS_CLASS_NAME;
    RegisterClassExW(&wc);

    int width = MulDiv(780, dpi, 96);
    int height = MulDiv(550, dpi, 96);

    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int posX = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int posY = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    DialogData data;
    data.darkMode = darkMode;
    data.dpi = dpi;

    HWND hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        SHORTCUTS_CLASS_NAME,
        L"Keyboard Shortcuts - LightNotePad",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, width, height,
        hParent, nullptr, hInstance, &data
    );

    if (!hWnd) return;

    EnableWindow(hParent, FALSE);

    MSG msg = {};
    while (IsWindow(hWnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && (msg.wParam == VK_ESCAPE || msg.wParam == VK_RETURN)) {
            DestroyWindow(hWnd);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

LRESULT CALLBACK ShortcutsDialog::DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* pData = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pData = reinterpret_cast<DialogData*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pData));

        BOOL useDark = pData->darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

        // Create fonts
        int hTitle = -MulDiv(14, pData->dpi, 72);
        int hHdr = -MulDiv(11, pData->dpi, 72);
        int hReg = -MulDiv(10, pData->dpi, 72);
        int hKey = -MulDiv(9, pData->dpi, 72);

        pData->hFontTitle = CreateFontW(hTitle, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        pData->hFontHeader = CreateFontW(hHdr, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        pData->hFontReg = CreateFontW(hReg, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        pData->hFontKey = CreateFontW(hKey, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Cascadia Mono");
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        int w = rcClient.right - rcClient.left;
        int h = rcClient.bottom - rcClient.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

        COLORREF bg = pData->darkMode ? RGB(26, 26, 26) : RGB(250, 250, 250);
        COLORREF cardBg = pData->darkMode ? RGB(34, 34, 34) : RGB(242, 242, 242);
        COLORREF keyBg = pData->darkMode ? RGB(48, 48, 48) : RGB(225, 225, 225);
        COLORREF textHdr = pData->darkMode ? RGB(6, 182, 212) : RGB(0, 120, 215);
        COLORREF textKey = pData->darkMode ? RGB(240, 240, 240) : RGB(20, 20, 20);
        COLORREF textDesc = pData->darkMode ? RGB(180, 180, 180) : RGB(80, 80, 80);

        HBRUSH hBrBg = CreateSolidBrush(bg);
        FillRect(memDC, &rcClient, hBrBg);
        DeleteObject(hBrBg);

        SetBkMode(memDC, TRANSPARENT);

        // Header Title
        SelectObject(memDC, pData->hFontTitle);
        SetTextColor(memDC, pData->darkMode ? RGB(255, 255, 255) : RGB(0, 0, 0));
        RECT rcTitle = { MulDiv(24, pData->dpi, 96), MulDiv(18, pData->dpi, 96), w, MulDiv(50, pData->dpi, 96) };
        DrawTextW(memDC, L"Keyboard Shortcuts Cheat Sheet", -1, &rcTitle, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);

        // Subtitle
        SelectObject(memDC, pData->hFontReg);
        SetTextColor(memDC, textDesc);
        RECT rcSub = { MulDiv(24, pData->dpi, 96), MulDiv(46, pData->dpi, 96), w, MulDiv(70, pData->dpi, 96) };
        DrawTextW(memDC, L"Press Esc or Enter to dismiss this window.", -1, &rcSub, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);

        // Draw 2 Columns of Categories
        int margin = MulDiv(24, pData->dpi, 96);
        int gap = MulDiv(16, pData->dpi, 96);
        int colWidth = (w - (margin * 2) - gap) / 2;
        int startY = MulDiv(75, pData->dpi, 96);

        for (size_t catIdx = 0; catIdx < 4; ++catIdx) {
            int col = static_cast<int>(catIdx % 2);
            int row = static_cast<int>(catIdx / 2);

            int x = margin + col * (colWidth + gap);
            int y = startY + row * MulDiv(215, pData->dpi, 96);
            int catHeight = MulDiv(205, pData->dpi, 96);

            RECT rcCard = { x, y, x + colWidth, y + catHeight };
            HBRUSH hBrCard = CreateSolidBrush(cardBg);
            FillRect(memDC, &rcCard, hBrCard);
            DeleteObject(hBrCard);

            // Category title
            SelectObject(memDC, pData->hFontHeader);
            SetTextColor(memDC, textHdr);
            RECT rcCatTitle = { x + MulDiv(14, pData->dpi, 96), y + MulDiv(10, pData->dpi, 96), x + colWidth, y + MulDiv(32, pData->dpi, 96) };
            DrawTextW(memDC, g_categories[catIdx].title, -1, &rcCatTitle, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);

            // Items
            int itemY = y + MulDiv(38, pData->dpi, 96);
            int lineH = MulDiv(25, pData->dpi, 96);
            int keyBadgeWidth = MulDiv(168, pData->dpi, 96);

            for (const auto& item : g_categories[catIdx].items) {
                // Key badge
                RECT rcKey = { x + MulDiv(12, pData->dpi, 96), itemY, x + MulDiv(12, pData->dpi, 96) + keyBadgeWidth, itemY + lineH - 4 };
                HBRUSH hBrKey = CreateSolidBrush(keyBg);
                FillRect(memDC, &rcKey, hBrKey);
                DeleteObject(hBrKey);

                SelectObject(memDC, pData->hFontKey);
                SetTextColor(memDC, textKey);
                DrawTextW(memDC, item.keys, -1, &rcKey, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

                // Description
                SelectObject(memDC, pData->hFontReg);
                SetTextColor(memDC, textDesc);
                RECT rcDesc = { rcKey.right + MulDiv(12, pData->dpi, 96), itemY, x + colWidth - MulDiv(10, pData->dpi, 96), itemY + lineH };
                DrawTextW(memDC, item.desc, -1, &rcDesc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

                itemY += lineH;
            }
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY: {
        if (pData) {
            if (pData->hFontTitle) DeleteObject(pData->hFontTitle);
            if (pData->hFontHeader) DeleteObject(pData->hFontHeader);
            if (pData->hFontReg) DeleteObject(pData->hFontReg);
            if (pData->hFontKey) DeleteObject(pData->hFontKey);
        }
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
