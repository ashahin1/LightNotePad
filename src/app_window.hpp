#pragma once
#include "pch.hpp"
#include "editor_tab.hpp"
#include "tab_bar.hpp"
#include "status_bar.hpp"
#include "resource_ids.hpp"

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow, const std::vector<std::wstring>& initialFiles);
    HWND GetHwnd() const { return m_hWnd; }
    HACCEL GetAccel() const { return m_hAccel; }
    HWND GetFindDialog() const { return m_hFindReplaceDialog; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Initialization
    void InitDarkMode();
    void ApplyTheme();
    void UpdateDpi(UINT dpi);
    void UpdateLayout();
    void CreateEditorFont();
    void CreateAppMenu();
    void CreateAccelerators();

    // Tab Management
    EditorTab* CreateNewTab(const std::wstring& filePath = L"");
    bool CloseTab(int tabId);
    void SwitchToTab(int tabId);
    EditorTab* GetActiveTab() const;
    EditorTab* FindTab(int tabId) const;
    bool PromptSaveIfDirty(EditorTab* tab);

    // File Operations
    void OpenFileDialog();
    bool SaveCurrentFile(bool saveAs = false);
    void UpdateWindowTitle();
    void LoadRecentFiles();
    void SaveRecentFiles();
    void AddRecentFile(const std::wstring& path);
    void ClearRecentFiles();
    void UpdateRecentMenu();
    void OpenRecentFile(int index);

    // Search Operations
    void OpenFindDialog(bool isReplace);
    void HandleFindReplaceMessage();
    bool FindTextInternal(bool forward, bool wrapAround = true);
    void ReplaceInternal(bool all);

    // View & Editing
    void ToggleWordWrap();
    void ToggleDarkMode();
    void AdjustZoom(int delta);
    void ResetZoom();
    void InsertDateTime();
    void ShowAboutDialog();
    void ShowShortcutsDialog();

    // Font Configuration
    void LoadFontSettings();
    void SaveFontSettings();
    void OpenFontDialog();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    HMODULE m_hRichEdit = nullptr;
    HMENU m_hMenu = nullptr;
    HMENU m_hRecentMenu = nullptr;
    HACCEL m_hAccel = nullptr;

    UINT m_dpi = 96;
    bool m_darkMode = false;
    bool m_wordWrap = true;
    HFONT m_hEditorFont = nullptr;
    HFONT m_hMenuFont = nullptr;
    int m_editorFontSizePt = 11;
    int m_editorFontWeight = FW_NORMAL;
    bool m_editorFontItalic = false;
    std::wstring m_fontFamily = L"Cascadia Mono";

    std::vector<std::wstring> m_recentFiles;

    // UI Sub-components
    TabBar m_tabBar;
    StatusBar m_statusBar;
    std::vector<std::unique_ptr<EditorTab>> m_tabs;
    int m_nextTabId = 1;
    int m_activeTabId = -1;

    // Find / Replace
    HWND m_hFindReplaceDialog = nullptr;
    FINDREPLACEW m_fr = {};
    wchar_t m_findWhat[256] = {};
    wchar_t m_replaceWith[256] = {};
    UINT m_uFindMsgString = 0;

    HBRUSH m_hBrBg = nullptr;
};
