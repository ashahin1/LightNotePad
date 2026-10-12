#include "pch.hpp"
#include "status_bar.hpp"

namespace {
const wchar_t* const STATUSBAR_CLASS_NAME = L"LightNotePad_StatusBar";
}

StatusBar::StatusBar() = default;

StatusBar::~StatusBar() {
    Destroy();
}

void StatusBar::Destroy() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

int StatusBar::GetDesiredHeight(UINT dpi) const {
    if (!m_visible) return 0;
    return MulDiv(24, dpi, 96);
}

bool StatusBar::Create(HWND hParent, HINSTANCE hInstance) {
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = StatusBar::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = STATUSBAR_CLASS_NAME;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0,
        STATUSBAR_CLASS_NAME,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 100, 24,
        hParent,
        nullptr,
        hInstance,
        this
    );

    SetDpi(GetDpiForWindow(hParent));
    return m_hWnd != nullptr;
}

void StatusBar::SetDpi(UINT dpi) {
    m_dpi = (dpi == 0) ? 96 : dpi;
    if (m_hFont) {
        DeleteObject(m_hFont);
    }

    int fontHeight = -MulDiv(9, m_dpi, 72);
    m_hFont = CreateFontW(
        fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void StatusBar::ApplyTheme(bool darkMode) {
    m_darkMode = darkMode;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void StatusBar::SetVisible(bool visible) {
    m_visible = visible;
    if (m_hWnd) {
        ShowWindow(m_hWnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void StatusBar::SetBounds(int x, int y, int width, int height) {
    if (m_hWnd) {
        if (!m_visible || height <= 0) {
            ShowWindow(m_hWnd, SW_HIDE);
        } else {
            ShowWindow(m_hWnd, SW_SHOW);
            MoveWindow(m_hWnd, x, y, width, height, TRUE);
        }
    }
}

void StatusBar::UpdateCaret(int line, int col, int charCount, int selCount) {
    m_line = line;
    m_col = col;
    m_charCount = charCount;
    m_selCount = selCount;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void StatusBar::UpdateDocumentInfo(FileEncoding enc, LineEnding le, int zoomPercent) {
    m_encoding = enc;
    m_lineEnding = le;
    m_zoomPercent = zoomPercent;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void StatusBar::OnPaint(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    COLORREF bgCol = m_darkMode ? RGB(24, 24, 24) : RGB(243, 243, 243);
    COLORREF textCol = m_darkMode ? RGB(180, 180, 180) : RGB(70, 70, 70);
    COLORREF borderCol = m_darkMode ? RGB(45, 45, 45) : RGB(215, 215, 215);

    HBRUSH hBr = CreateSolidBrush(bgCol);
    FillRect(memDC, &rcClient, hBr);
    DeleteObject(hBr);

    // Top border line
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hPen));
    MoveToEx(memDC, 0, 0, nullptr);
    LineTo(memDC, width, 0);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_hFont));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, textCol);

    // Format fields
    wchar_t szCaret[64];
    if (m_selCount > 0) {
        swprintf_s(szCaret, L"Ln %d, Col %d (%d selected)", m_line, m_col, m_selCount);
    } else {
        swprintf_s(szCaret, L"Ln %d, Col %d", m_line, m_col);
    }

    wchar_t szChars[64];
    swprintf_s(szChars, L"%d chars", m_charCount);

    wchar_t szZoom[32];
    swprintf_s(szZoom, L"%d%%", m_zoomPercent);

    const wchar_t* szLe = FileIO::LineEndingToString(m_lineEnding);
    const wchar_t* szEnc = FileIO::EncodingToString(m_encoding);

    // Layout from right to left for standard items
    int padding = MulDiv(12, m_dpi, 96);
    int encWidth = MulDiv(90, m_dpi, 96);
    int leWidth = MulDiv(110, m_dpi, 96);
    int zoomWidth = MulDiv(60, m_dpi, 96);
    int charsWidth = MulDiv(90, m_dpi, 96);

    int curRight = width - padding;

    // Encoding
    RECT rcEnc = { curRight - encWidth, 1, curRight, height };
    DrawTextW(memDC, szEnc, -1, &rcEnc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    curRight -= encWidth;

    // Line Ending
    RECT rcLe = { curRight - leWidth, 1, curRight, height };
    DrawTextW(memDC, szLe, -1, &rcLe, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    curRight -= leWidth;

    // Zoom
    RECT rcZoom = { curRight - zoomWidth, 1, curRight, height };
    DrawTextW(memDC, szZoom, -1, &rcZoom, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    curRight -= zoomWidth;

    // Chars
    RECT rcChars = { curRight - charsWidth, 1, curRight, height };
    DrawTextW(memDC, szChars, -1, &rcChars, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    curRight -= charsWidth;

    // Caret info on left
    RECT rcCaret = { padding, 1, curRight - padding, height };
    DrawTextW(memDC, szCaret, -1, &rcCaret, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    // Copy to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldPen);
    DeleteObject(hPen);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK StatusBar::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StatusBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<StatusBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<StatusBar*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT StatusBar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hWnd, &ps);
        OnPaint(hdc);
        EndPaint(m_hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}
