#pragma once
#include "pch.hpp"

struct TabItem {
    int id = 0;
    std::wstring title;
    bool isDirty = false;
    RECT rect = {};
    RECT closeRect = {};
};

class TabBar {
public:
    TabBar();
    ~TabBar();

    bool Create(HWND hParent, HINSTANCE hInstance);
    void Destroy();

    HWND GetHwnd() const { return m_hWnd; }

    int GetDesiredHeight(UINT dpi) const;
    void SetBounds(int x, int y, int width, int height);

    void AddTab(int id, const std::wstring& title);
    void RemoveTab(int id);
    void UpdateTab(int id, const std::wstring& title, bool isDirty);
    void SetActiveTab(int id);
    int GetActiveTab() const { return m_activeTabId; }
    int GetActiveTabIndex() const;
    size_t GetTabCount() const { return m_tabs.size(); }
    void ClearTabs();
    int HitTestTab(POINT ptClient) const;

    void ApplyTheme(bool darkMode);
    void SetDpi(UINT dpi);

    // Callbacks to parent window
    std::function<void(int tabId)> onTabSelected;
    std::function<void(int tabId)> onTabCloseRequested;
    std::function<void()> onNewTabRequested;
    std::function<void(size_t tabIndex, int tabId, POINT ptScreen)> onTabContextMenu;

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void RecalculateLayout();
    void OnPaint(HDC hdc);

    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;

    std::vector<TabItem> m_tabs;
    int m_activeTabId = -1;
    int m_hoveredTabId = -1;
    int m_hoveredCloseTabId = -1;
    bool m_hoverNewTab = false;
    RECT m_newTabRect = {};

    UINT m_dpi = 96;
    bool m_darkMode = false;
    HFONT m_hFont = nullptr;
};
