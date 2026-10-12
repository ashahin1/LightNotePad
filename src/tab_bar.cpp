#include "pch.hpp"
#include "tab_bar.hpp"

namespace {
const wchar_t* const TABBAR_CLASS_NAME = L"LightNotePad_TabBar";
}

TabBar::TabBar() = default;

TabBar::~TabBar() {
    Destroy();
}

void TabBar::Destroy() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

int TabBar::GetDesiredHeight(UINT dpi) const {
    if (m_tabs.size() <= 1) {
        return 0; // Smart tabs: invisible when 0 or 1 tab
    }
    return MulDiv(32, dpi, 96);
}

bool TabBar::Create(HWND hParent, HINSTANCE hInstance) {
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = TabBar::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = TABBAR_CLASS_NAME;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0,
        TABBAR_CLASS_NAME,
        L"",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 100, 32,
        hParent,
        nullptr,
        hInstance,
        this
    );

    SetDpi(GetDpiForWindow(hParent));
    return m_hWnd != nullptr;
}

void TabBar::SetDpi(UINT dpi) {
    m_dpi = (dpi == 0) ? 96 : dpi;
    if (m_hFont) {
        DeleteObject(m_hFont);
    }

    int fontHeight = -MulDiv(10, m_dpi, 72);
    m_hFont = CreateFontW(
        fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    RecalculateLayout();
}

void TabBar::ApplyTheme(bool darkMode) {
    m_darkMode = darkMode;
    if (m_hWnd) {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void TabBar::SetBounds(int x, int y, int width, int height) {
    if (m_hWnd) {
        if (height <= 0) {
            ShowWindow(m_hWnd, SW_HIDE);
        } else {
            ShowWindow(m_hWnd, SW_SHOW);
            MoveWindow(m_hWnd, x, y, width, height, TRUE);
            RecalculateLayout();
        }
    }
}

void TabBar::AddTab(int id, const std::wstring& title) {
    TabItem item;
    item.id = id;
    item.title = title;
    m_tabs.push_back(item);
    SetActiveTab(id);
    RecalculateLayout();
}

void TabBar::RemoveTab(int id) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [id](const TabItem& t) { return t.id == id; });
    if (it != m_tabs.end()) {
        m_tabs.erase(it);
    }
    if (m_activeTabId == id) {
        m_activeTabId = m_tabs.empty() ? -1 : m_tabs.back().id;
    }
    RecalculateLayout();
}

void TabBar::UpdateTab(int id, const std::wstring& title, bool isDirty) {
    for (auto& t : m_tabs) {
        if (t.id == id) {
            t.title = title;
            t.isDirty = isDirty;
            break;
        }
    }
    RecalculateLayout();
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void TabBar::SetActiveTab(int id) {
    m_activeTabId = id;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void TabBar::RecalculateLayout() {
    if (!m_hWnd) return;

    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;
    int clientHeight = rcClient.bottom - rcClient.top;

    if (m_tabs.empty() || clientHeight <= 0) {
        return;
    }

    int tabHeight = clientHeight;
    int closeBtnSize = MulDiv(14, m_dpi, 96);
    int padding = MulDiv(8, m_dpi, 96);

    int maxTabWidth = MulDiv(180, m_dpi, 96);
    int minTabWidth = MulDiv(90, m_dpi, 96);
    int plusBtnWidth = MulDiv(30, m_dpi, 96);

    int availableWidth = clientWidth - plusBtnWidth - padding;
    int tabWidth = maxTabWidth;
    if (static_cast<int>(m_tabs.size()) * tabWidth > availableWidth) {
        tabWidth = availableWidth / static_cast<int>(m_tabs.size());
        if (tabWidth < minTabWidth) tabWidth = minTabWidth;
    }

    int curX = padding;
    for (auto& tab : m_tabs) {
        tab.rect.left = curX;
        tab.rect.top = 2;
        tab.rect.right = curX + tabWidth;
        tab.rect.bottom = tabHeight;

        tab.closeRect.right = tab.rect.right - MulDiv(6, m_dpi, 96);
        tab.closeRect.left = tab.closeRect.right - closeBtnSize;
        tab.closeRect.top = tab.rect.top + (tabHeight - closeBtnSize) / 2;
        tab.closeRect.bottom = tab.closeRect.top + closeBtnSize;

        curX += tabWidth + 2;
    }

    m_newTabRect.left = curX + 2;
    m_newTabRect.top = 4;
    m_newTabRect.right = m_newTabRect.left + plusBtnWidth;
    m_newTabRect.bottom = tabHeight - 4;

    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void TabBar::OnPaint(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Strip background
    COLORREF bgStrip = m_darkMode ? RGB(22, 22, 22) : RGB(235, 235, 235);
    COLORREF bgActive = m_darkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
    COLORREF bgInactive = m_darkMode ? RGB(34, 34, 34) : RGB(225, 225, 225);
    COLORREF bgHover = m_darkMode ? RGB(45, 45, 45) : RGB(242, 242, 242);
    COLORREF textCol = m_darkMode ? RGB(200, 200, 200) : RGB(50, 50, 50);
    COLORREF textActiveCol = m_darkMode ? RGB(255, 255, 255) : RGB(0, 0, 0);
    COLORREF accentCol = m_darkMode ? RGB(6, 182, 212) : RGB(0, 120, 215);

    HBRUSH hBrStrip = CreateSolidBrush(bgStrip);
    FillRect(memDC, &rcClient, hBrStrip);
    DeleteObject(hBrStrip);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_hFont));
    SetBkMode(memDC, TRANSPARENT);

    // Bottom border line of tab bar
    HPEN hPenBorder = CreatePen(PS_SOLID, 1, m_darkMode ? RGB(45, 45, 45) : RGB(210, 210, 210));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hPenBorder));
    MoveToEx(memDC, 0, height - 1, nullptr);
    LineTo(memDC, width, height - 1);

    // Draw tabs
    for (const auto& tab : m_tabs) {
        bool isActive = (tab.id == m_activeTabId);
        bool isHover = (tab.id == m_hoveredTabId);

        COLORREF tabBg = isActive ? bgActive : (isHover ? bgHover : bgInactive);
        HBRUSH hBrTab = CreateSolidBrush(tabBg);
        FillRect(memDC, &tab.rect, hBrTab);
        DeleteObject(hBrTab);

        // Active tab accent bar at the top
        if (isActive) {
            RECT rcAccent = tab.rect;
            rcAccent.bottom = rcAccent.top + MulDiv(2, m_dpi, 96);
            HBRUSH hBrAccent = CreateSolidBrush(accentCol);
            FillRect(memDC, &rcAccent, hBrAccent);
            DeleteObject(hBrAccent);
        }

        // Title text
        std::wstring displayTitle = tab.title;
        if (tab.isDirty) {
            displayTitle = L"● " + displayTitle;
        }

        RECT rcText = tab.rect;
        rcText.left += MulDiv(10, m_dpi, 96);
        rcText.right = tab.closeRect.left - MulDiv(4, m_dpi, 96);

        SetTextColor(memDC, isActive ? textActiveCol : textCol);
        DrawTextW(memDC, displayTitle.c_str(), -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Close button 'x'
        bool isCloseHover = (tab.id == m_hoveredCloseTabId);
        if (isCloseHover) {
            HBRUSH hBrClose = CreateSolidBrush(RGB(239, 68, 68));
            FillRect(memDC, &tab.closeRect, hBrClose);
            DeleteObject(hBrClose);
            SetTextColor(memDC, RGB(255, 255, 255));
        } else {
            SetTextColor(memDC, m_darkMode ? RGB(140, 140, 140) : RGB(120, 120, 120));
        }

        RECT rcX = tab.closeRect;
        DrawTextW(memDC, L"×", -1, &rcX, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // New Tab (+) button
    COLORREF plusBg = m_hoverNewTab ? bgHover : bgInactive;
    HBRUSH hBrPlus = CreateSolidBrush(plusBg);
    FillRect(memDC, &m_newTabRect, hBrPlus);
    DeleteObject(hBrPlus);

    SetTextColor(memDC, m_hoverNewTab ? textActiveCol : textCol);
    RECT rcPlus = m_newTabRect;
    DrawTextW(memDC, L"+", -1, &rcPlus, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Copy to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldPen);
    DeleteObject(hPenBorder);
    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK TabBar::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TabBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<TabBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT TabBar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);

        int oldHoverTab = m_hoveredTabId;
        int oldHoverClose = m_hoveredCloseTabId;
        bool oldHoverNew = m_hoverNewTab;

        m_hoveredTabId = -1;
        m_hoveredCloseTabId = -1;
        m_hoverNewTab = PtInRect(&m_newTabRect, pt) != FALSE;

        for (const auto& tab : m_tabs) {
            if (PtInRect(&tab.closeRect, pt)) {
                m_hoveredCloseTabId = tab.id;
                m_hoveredTabId = tab.id;
                break;
            } else if (PtInRect(&tab.rect, pt)) {
                m_hoveredTabId = tab.id;
                break;
            }
        }

        if (oldHoverTab != m_hoveredTabId || oldHoverClose != m_hoveredCloseTabId || oldHoverNew != m_hoverNewTab) {
            InvalidateRect(m_hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        m_hoveredTabId = -1;
        m_hoveredCloseTabId = -1;
        m_hoverNewTab = false;
        InvalidateRect(m_hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (PtInRect(&m_newTabRect, pt)) {
            if (onNewTabRequested) onNewTabRequested();
            return 0;
        }

        for (const auto& tab : m_tabs) {
            if (PtInRect(&tab.closeRect, pt)) {
                if (onTabCloseRequested) onTabCloseRequested(tab.id);
                return 0;
            } else if (PtInRect(&tab.rect, pt)) {
                if (onTabSelected) onTabSelected(tab.id);
                return 0;
            }
        }
        return 0;
    }
    case WM_MBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (const auto& tab : m_tabs) {
            if (PtInRect(&tab.rect, pt)) {
                if (onTabCloseRequested) onTabCloseRequested(tab.id);
                return 0;
            }
        }
        return 0;
    }
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}
