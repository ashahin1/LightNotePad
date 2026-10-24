#include "pch.hpp"
#include "app_window.hpp"
#include "shortcuts_dialog.hpp"
#include "supported_formats.hpp"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Undocumented Win32 Menu Bar Dark Mode Messages & Structs
#define WM_UAHDRAWMENU          0x0091
#define WM_UAHDRAWMENUITEM      0x0092

struct UAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
};

struct UAHMENUITEM {
    int iPosition;
    UAHMENU uahMenu;
};

struct UAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU uahMenu;
    int iPosition;
    ULONG cx;
    ULONG cy;
};

enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
using fnFlushMenuThemes = void(WINAPI*)();

namespace {
const wchar_t* const WINDOW_CLASS_NAME = L"LightNotePad_WindowClass";
}

AppWindow::AppWindow() {
    m_hRichEdit = LoadLibraryW(L"msftedit.dll");
    m_uFindMsgString = RegisterWindowMessageW(FINDMSGSTRINGW);
}

AppWindow::~AppWindow() {
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    if (m_hMenu) {
        DestroyMenu(m_hMenu);
        m_hMenu = nullptr;
    }
    if (m_hEditorFont) {
        DeleteObject(m_hEditorFont);
        m_hEditorFont = nullptr;
    }
    if (m_hMenuFont) {
        DeleteObject(m_hMenuFont);
        m_hMenuFont = nullptr;
    }
    if (m_hBrBg) {
        DeleteObject(m_hBrBg);
        m_hBrBg = nullptr;
    }
    if (m_hRichEdit) {
        FreeLibrary(m_hRichEdit);
        m_hRichEdit = nullptr;
    }
}

void AppWindow::InitDarkMode() {
    bool userPrefFound = false;

    // 1. Check user preference in LightNotePad settings
    HKEY hAppKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hAppKey, L"DarkMode", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            m_darkMode = (val != 0);
            userPrefFound = true;
        }
        RegCloseKey(hAppKey);
    }

    // 2. If no explicit user preference, follow Windows system theme
    if (!userPrefFound) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 1;
            DWORD size = sizeof(val);
            if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
                m_darkMode = (val == 0);
            }
            RegCloseKey(hKey);
        }
    }

    ApplyTheme();
}

void AppWindow::ApplyTheme() {
    if (m_hBrBg) {
        DeleteObject(m_hBrBg);
    }
    m_hBrBg = CreateSolidBrush(m_darkMode ? RGB(30, 30, 30) : RGB(255, 255, 255));

    // Update Win10/11 popup and context menus via UxTheme
    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxTheme) hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        auto SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
        auto FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));
        if (SetPreferredAppMode) {
            SetPreferredAppMode(m_darkMode ? ForceDark : ForceLight);
        }
        if (FlushMenuThemes) {
            FlushMenuThemes();
        }
    }

    if (m_hWnd) {
        BOOL useDark = m_darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

        m_tabBar.ApplyTheme(m_darkMode);
        m_statusBar.ApplyTheme(m_darkMode);

        for (auto& tab : m_tabs) {
            tab->ApplyTheme(m_darkMode);
        }

        CheckMenuItem(m_hMenu, IDM_VIEW_DARKMODE, m_darkMode ? MF_CHECKED : MF_UNCHECKED);
        DrawMenuBar(m_hWnd);
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }
}

void AppWindow::UpdateDpi(UINT dpi) {
    m_dpi = (dpi == 0) ? 96 : dpi;
    CreateEditorFont();

    if (m_hMenuFont) {
        DeleteObject(m_hMenuFont);
    }
    int hMenu = -MulDiv(9, m_dpi, 72);
    m_hMenuFont = CreateFontW(
        hMenu, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    m_tabBar.SetDpi(m_dpi);
    m_statusBar.SetDpi(m_dpi);
    UpdateLayout();
}

void AppWindow::CreateEditorFont() {
    if (m_hEditorFont) {
        DeleteObject(m_hEditorFont);
        m_hEditorFont = nullptr;
    }

    int fontHeight = -MulDiv(m_editorFontSizePt, m_dpi, 72);

    m_hEditorFont = CreateFontW(
        fontHeight, 0, 0, 0, m_editorFontWeight, m_editorFontItalic ? TRUE : FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, m_fontFamily.c_str()
    );

    for (auto& tab : m_tabs) {
        tab->SetFont(m_hEditorFont);
    }
}

void AppWindow::CreateAppMenu() {
    m_hMenu = CreateMenu();

    // File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"New Tab\tCtrl+T");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW_WINDOW, L"New Window\tCtrl+Shift+N");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"Open...\tCtrl+O");

    m_hRecentMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_hRecentMenu), L"Open &Recent");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save As...\tCtrl+Shift+S");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_PRINT, L"Print...\tCtrl+P");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_CLOSETAB, L"Close Tab\tCtrl+W");
    AppendMenuW(hFileMenu, MF_STRING, IDM_TAB_REOPEN_CLOSED, L"Reopen Closed Tab\tCtrl+Shift+T");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"Exit\tAlt+F4");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), L"&File");

    // Edit Menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_UNDO, L"Undo\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REDO, L"Redo\tCtrl+Y");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CUT, L"Cut\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY, L"Copy\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE, L"Paste\tCtrl+V");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_DELETE, L"Delete\tDel");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_SELECTALL, L"Select All\tCtrl+A");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_TIMEDATE, L"Time/Date\tF5");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hEditMenu), L"&Edit");

    // Search Menu
    HMENU hSearchMenu = CreatePopupMenu();
    AppendMenuW(hSearchMenu, MF_STRING, IDM_SEARCH_FIND, L"Find...\tCtrl+F");
    AppendMenuW(hSearchMenu, MF_STRING, IDM_SEARCH_FINDNEXT, L"Find Next\tF3");
    AppendMenuW(hSearchMenu, MF_STRING, IDM_SEARCH_FINDPREV, L"Find Previous\tShift+F3");
    AppendMenuW(hSearchMenu, MF_STRING, IDM_SEARCH_REPLACE, L"Replace...\tCtrl+H");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSearchMenu), L"&Search");

    // View Menu
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING | MF_CHECKED, IDM_VIEW_WORDWRAP, L"Word Wrap\tAlt+Z");
    AppendMenuW(hViewMenu, MF_STRING | MF_CHECKED, IDM_VIEW_SYNTAX, L"Syntax Highlighting");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_FONT, L"Font...");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_FONTRESET, L"Restore Default Font");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMIN, L"Zoom In\tCtrl++");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMOUT, L"Zoom Out\tCtrl+-");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMRESET, L"Restore Default Zoom\tCtrl+0");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING | MF_CHECKED, IDM_VIEW_STATUSBAR, L"Status Bar");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_DARKMODE, L"Dark Mode");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hViewMenu), L"&View");

    // Help Menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_SHORTCUTS, L"Keyboard Shortcuts\tF1");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"About LightNotePad");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), L"&Help");

    SetMenu(m_hWnd, m_hMenu);
}

void AppWindow::CreateAccelerators() {
    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'N', IDM_FILE_NEW },
        { FCONTROL | FVIRTKEY, 'T', IDM_FILE_NEW },
        { FCONTROL | FSHIFT | FVIRTKEY, 'N', IDM_FILE_NEW_WINDOW },
        { FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN },
        { FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE },
        { FCONTROL | FSHIFT | FVIRTKEY, 'S', IDM_FILE_SAVEAS },
        { FCONTROL | FVIRTKEY, 'P', IDM_FILE_PRINT },
        { FCONTROL | FVIRTKEY, 'W', IDM_FILE_CLOSETAB },
        { FCONTROL | FSHIFT | FVIRTKEY, 'T', IDM_TAB_REOPEN_CLOSED },

        { FCONTROL | FVIRTKEY, 'Z', IDM_EDIT_UNDO },
        { FCONTROL | FVIRTKEY, 'Y', IDM_EDIT_REDO },
        { FCONTROL | FVIRTKEY, 'A', IDM_EDIT_SELECTALL },
        { FVIRTKEY, VK_F5, IDM_EDIT_TIMEDATE },

        { FCONTROL | FVIRTKEY, 'F', IDM_SEARCH_FIND },
        { FVIRTKEY, VK_F3, IDM_SEARCH_FINDNEXT },
        { FSHIFT | FVIRTKEY, VK_F3, IDM_SEARCH_FINDPREV },
        { FCONTROL | FVIRTKEY, 'H', IDM_SEARCH_REPLACE },

        { FALT | FVIRTKEY, 'Z', IDM_VIEW_WORDWRAP },
        { FCONTROL | FVIRTKEY, VK_OEM_PLUS, IDM_VIEW_ZOOMIN },
        { FCONTROL | FVIRTKEY, VK_ADD, IDM_VIEW_ZOOMIN },
        { FCONTROL | FVIRTKEY, VK_OEM_MINUS, IDM_VIEW_ZOOMOUT },
        { FCONTROL | FVIRTKEY, VK_SUBTRACT, IDM_VIEW_ZOOMOUT },
        { FCONTROL | FVIRTKEY, '0', IDM_VIEW_ZOOMRESET },

        { FCONTROL | FVIRTKEY, VK_TAB, IDM_TAB_NEXT },
        { FCONTROL | FSHIFT | FVIRTKEY, VK_TAB, IDM_TAB_PREV },

        { FVIRTKEY, VK_F1, IDM_HELP_SHORTCUTS },
    };

    m_hAccel = CreateAcceleratorTableW(accels, sizeof(accels) / sizeof(accels[0]));
}

EditorTab* AppWindow::CreateNewTab(const std::wstring& filePath) {
    int tabId = m_nextTabId++;
    auto tab = std::make_unique<EditorTab>(m_hWnd, m_hInstance, tabId);
    if (!tab->Create()) {
        return nullptr;
    }

    tab->SetFont(m_hEditorFont);
    tab->ApplyTheme(m_darkMode);
    tab->SetWordWrap(m_wordWrap);
    tab->SetSyntaxEnabled(m_syntaxHighlighting);

    if (!filePath.empty()) {
        tab->OpenFile(filePath);
    } else {
        tab->SetFilePath(L"");
    }

    m_tabBar.AddTab(tabId, tab->GetFileName());
    EditorTab* pTab = tab.get();
    m_tabs.push_back(std::move(tab));

    SwitchToTab(tabId);
    return pTab;
}

EditorTab* AppWindow::FindTab(int tabId) const {
    for (const auto& tab : m_tabs) {
        if (tab->GetTabId() == tabId) return tab.get();
    }
    return nullptr;
}

EditorTab* AppWindow::GetActiveTab() const {
    return FindTab(m_activeTabId);
}

void AppWindow::SwitchToTab(int tabId) {
    m_activeTabId = tabId;
    m_tabBar.SetActiveTab(tabId);

    for (const auto& tab : m_tabs) {
        bool isActive = (tab->GetTabId() == tabId);
        tab->Show(isActive);
    }

    EditorTab* active = GetActiveTab();
    if (active) {
        active->SetFocus();
        CheckMenuItem(m_hMenu, IDM_VIEW_WORDWRAP, active->IsWordWrap() ? MF_CHECKED : MF_UNCHECKED);
        active->ColorizeViewport();
    }

    UpdateLayout();
    UpdateWindowTitle();
}

bool AppWindow::PromptSaveIfDirty(EditorTab* tab) {
    if (!tab || !tab->IsDirty()) {
        return true;
    }

    std::wstring prompt = L"Do you want to save changes to " + tab->GetFileName() + L"?";
    int res = MessageBoxW(m_hWnd, prompt.c_str(), L"LightNotePad", MB_YESNOCANCEL | MB_ICONWARNING);

    if (res == IDYES) {
        if (tab->GetFilePath().empty()) {
            // Need Save As dialog
            SwitchToTab(tab->GetTabId());
            return SaveCurrentFile(true);
        } else {
            return tab->Save();
        }
    } else if (res == IDNO) {
        return true; // Discard changes
    } else {
        return false; // Cancel operation
    }
}

bool AppWindow::CloseTab(int tabId) {
    EditorTab* tab = FindTab(tabId);
    if (!tab) return false;

    if (!PromptSaveIfDirty(tab)) {
        return false;
    }

    RecordClosedTab(tab);

    m_tabBar.RemoveTab(tabId);

    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [tabId](const auto& t) { return t->GetTabId() == tabId; });
    if (it != m_tabs.end()) {
        m_tabs.erase(it);
    }

    if (m_tabs.empty()) {
        // Closed last tab: create a fresh Untitled tab
        CreateNewTab();
    } else {
        SwitchToTab(m_tabBar.GetActiveTab());
    }

    UpdateLayout();
    return true;
}

void AppWindow::RecordClosedTab(EditorTab* tab) {
    if (!tab || tab->GetFilePath().empty()) {
        return;
    }
    ClosedTabState state;
    state.filePath = tab->GetFilePath();
    state.zoomPercent = tab->GetZoom();
    state.encoding = tab->GetEncoding();
    HWND hEdit = tab->GetEditHwnd();
    if (hEdit) {
        SendMessage(hEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&state.selection));
        SendMessage(hEdit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&state.scrollPos));
    }
    m_closedTabsHistory.push_back(std::move(state));
    if (m_closedTabsHistory.size() > 50) {
        m_closedTabsHistory.erase(m_closedTabsHistory.begin());
    }
}

void AppWindow::ReopenClosedTab() {
    if (m_closedTabsHistory.empty()) {
        return;
    }
    ClosedTabState state = std::move(m_closedTabsHistory.back());
    m_closedTabsHistory.pop_back();

    if (state.filePath.empty()) {
        return;
    }

    EditorTab* active = GetActiveTab();
    EditorTab* openedTab = nullptr;
    if (m_tabs.size() == 1 && active && active->GetFilePath().empty() && !active->IsDirty()) {
        if (active->OpenFile(state.filePath)) {
            openedTab = active;
            m_tabBar.UpdateTab(active->GetTabId(), active->GetFileName(), active->IsDirty());
        }
    } else {
        openedTab = CreateNewTab(state.filePath);
    }

    if (openedTab) {
        AddRecentFile(state.filePath);
        openedTab->SetZoom(state.zoomPercent);
        HWND hEdit = openedTab->GetEditHwnd();
        if (hEdit) {
            SendMessage(hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&state.selection));
            SendMessage(hEdit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&state.scrollPos));
        }
        UpdateWindowTitle();
        UpdateLayout();
    }
}

bool AppWindow::CloseOtherTabs(size_t keepIndex) {
    if (keepIndex >= m_tabs.size() || m_tabs.size() <= 1) {
        return false;
    }

    size_t activeIdx = 0;
    bool res = TabStripHelper::CloseOtherTabs(
        m_tabs,
        activeIdx,
        keepIndex,
        [this](size_t, std::unique_ptr<EditorTab>& tab) {
            return PromptSaveIfDirty(tab.get());
        },
        [this](size_t, std::unique_ptr<EditorTab>& tab) {
            RecordClosedTab(tab.get());
        }
    );
    if (!res) {
        return false;
    }

    int keptId = m_tabs[0]->GetTabId();
    m_tabBar.ClearTabs();
    m_tabBar.AddTab(keptId, m_tabs[0]->GetFileName());
    m_tabBar.UpdateTab(keptId, m_tabs[0]->GetFileName(), m_tabs[0]->IsDirty());

    SwitchToTab(keptId);
    UpdateLayout();
    UpdateWindowTitle();
    return true;
}

bool AppWindow::CloseTabsToTheRight(size_t index) {
    if (index + 1 >= m_tabs.size()) {
        return false;
    }

    size_t activeIdx = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->GetTabId() == m_activeTabId) {
            activeIdx = i;
            break;
        }
    }

    std::vector<int> removeIds;
    for (size_t i = index + 1; i < m_tabs.size(); ++i) {
        removeIds.push_back(m_tabs[i]->GetTabId());
    }

    bool res = TabStripHelper::CloseTabsToTheRight(
        m_tabs,
        activeIdx,
        index,
        [this](size_t, std::unique_ptr<EditorTab>& tab) {
            return PromptSaveIfDirty(tab.get());
        },
        [this](size_t, std::unique_ptr<EditorTab>& tab) {
            RecordClosedTab(tab.get());
        }
    );
    if (!res) {
        return false;
    }

    for (int id : removeIds) {
        m_tabBar.RemoveTab(id);
    }

    int newActiveId = m_tabs[activeIdx]->GetTabId();
    SwitchToTab(newActiveId);
    UpdateLayout();
    UpdateWindowTitle();
    return true;
}

void AppWindow::CopyTabFilePath(size_t targetIndex) {
    if (targetIndex >= m_tabs.size()) return;
    const std::wstring& path = m_tabs[targetIndex]->GetFilePath();
    if (path.empty()) return;

    wchar_t fullPath[MAX_PATH * 2] = {};
    const wchar_t* pathToCopy = path.c_str();
    if (GetFullPathNameW(path.c_str(), MAX_PATH * 2, fullPath, nullptr) > 0) {
        pathToCopy = fullPath;
    }

    if (!OpenClipboard(m_hWnd)) return;
    EmptyClipboard();

    size_t charCount = wcslen(pathToCopy) + 1;
    size_t bytes = charCount * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* pMem = GlobalLock(hMem);
        if (pMem) {
            memcpy(pMem, pathToCopy, bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

void AppWindow::OpenTabContainingFolder(size_t targetIndex) {
    if (targetIndex >= m_tabs.size()) return;
    const std::wstring& filePath = m_tabs[targetIndex]->GetFilePath();
    if (filePath.empty()) return;

    std::wstring params = L"/select,\"" + filePath + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

void AppWindow::ShowTabContextMenu(size_t targetIndex, POINT ptScreen) {
    if (targetIndex >= m_tabs.size()) return;

    bool hasFile = !m_tabs[targetIndex]->GetFilePath().empty();
    bool hasClosedHistory = !m_closedTabsHistory.empty();

    TabContextMenuFlags flags = TabStripHelper::ComputeMenuFlags(
        targetIndex,
        m_tabs.size(),
        hasFile,
        hasClosedHistory
    );

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    UINT flagClose = flags.closeTab ? MF_ENABLED : MF_GRAYED;
    UINT flagCloseOthers = flags.closeOtherTabs ? MF_ENABLED : MF_GRAYED;
    UINT flagCloseRight = flags.closeTabsToTheRight ? MF_ENABLED : MF_GRAYED;
    UINT flagReopen = flags.reopenClosedTab ? MF_ENABLED : MF_GRAYED;
    UINT flagCopyPath = flags.copyFilePath ? MF_ENABLED : MF_GRAYED;
    UINT flagOpenFolder = flags.openContainingFolder ? MF_ENABLED : MF_GRAYED;

    AppendMenuW(hMenu, MF_STRING | flagClose, IDM_TAB_CLOSE, L"Close Tab\tCtrl+W");
    AppendMenuW(hMenu, MF_STRING | flagCloseOthers, IDM_TAB_CLOSE_OTHERS, L"Close Other Tabs");
    AppendMenuW(hMenu, MF_STRING | flagCloseRight, IDM_TAB_CLOSE_RIGHT, L"Close Tabs to the Right");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | flagReopen, IDM_TAB_REOPEN_CLOSED, L"Reopen Closed Tab\tCtrl+Shift+T");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | flagCopyPath, IDM_TAB_COPY_PATH, L"Copy File Path");
    AppendMenuW(hMenu, MF_STRING | flagOpenFolder, IDM_TAB_OPEN_FOLDER, L"Open Containing Folder");

    SetForegroundWindow(m_hWnd);

    UINT cmd = TrackPopupMenuEx(
        hMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        ptScreen.x,
        ptScreen.y,
        m_hWnd,
        nullptr
    );

    DestroyMenu(hMenu);

    if (cmd == 0) return;

    if (targetIndex >= m_tabs.size()) return;

    switch (cmd) {
    case IDM_TAB_CLOSE:
        CloseTab(m_tabs[targetIndex]->GetTabId());
        break;
    case IDM_TAB_CLOSE_OTHERS:
        CloseOtherTabs(targetIndex);
        break;
    case IDM_TAB_CLOSE_RIGHT:
        CloseTabsToTheRight(targetIndex);
        break;
    case IDM_TAB_REOPEN_CLOSED:
        ReopenClosedTab();
        break;
    case IDM_TAB_COPY_PATH:
        CopyTabFilePath(targetIndex);
        break;
    case IDM_TAB_OPEN_FOLDER:
        OpenTabContainingFolder(targetIndex);
        break;
    default:
        break;
    }
}

void AppWindow::UpdateLayout() {
    if (!m_hWnd) return;

    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;
    int clientHeight = rcClient.bottom - rcClient.top;

    if (clientWidth <= 0 || clientHeight <= 0) return;

    // Smart Tab Bar: height is 0 when 1 tab, 32px when 2+ tabs!
    int tabHeight = m_tabBar.GetDesiredHeight(m_dpi);
    m_tabBar.SetBounds(0, 0, clientWidth, tabHeight);

    int statusHeight = m_statusBar.GetDesiredHeight(m_dpi);
    m_statusBar.SetBounds(0, clientHeight - statusHeight, clientWidth, statusHeight);

    int editTop = tabHeight;
    int editHeight = clientHeight - tabHeight - statusHeight;
    if (editHeight < 0) editHeight = 0;

    EditorTab* active = GetActiveTab();
    if (active) {
        active->SetBounds(0, editTop, clientWidth, editHeight);
    }
}

void AppWindow::UpdateWindowTitle() {
    EditorTab* active = GetActiveTab();
    if (!active) {
        SetWindowTextW(m_hWnd, L"LightNotePad");
        return;
    }

    std::wstring title;
    if (active->IsDirty()) {
        title = L"*" + active->GetFileName() + L" - LightNotePad";
    } else {
        title = active->GetFileName() + L" - LightNotePad";
    }
    SetWindowTextW(m_hWnd, title.c_str());

    m_tabBar.UpdateTab(active->GetTabId(), active->GetFileName(), active->IsDirty());

    // Update status bar
    int line = 1, col = 1, chars = 0, sel = 0;
    active->GetCaretInfo(line, col, chars, sel);
    m_statusBar.UpdateCaret(line, col, chars, sel);
    m_statusBar.UpdateDocumentInfo(active->GetEncoding(), active->GetLineEnding(), active->GetZoom());
}

void AppWindow::OpenFileDialog() {
    // Build the filter string at runtime from the shared extension lists so it never drifts
    // from what the app can actually open. Entries are "description\0pattern\0".
    const std::wstring allPattern = SupportedFormats::BuildAllSupportedPattern();
    const std::wstring textPattern = SupportedFormats::BuildPattern(SupportedFormats::kText);
    const std::wstring mdPattern = SupportedFormats::BuildPattern(SupportedFormats::kMarkdown);
    const std::wstring configPattern = SupportedFormats::BuildPattern(SupportedFormats::kConfig);
    const std::wstring dataPattern = SupportedFormats::BuildPattern(SupportedFormats::kData);
    const std::wstring codePattern = SupportedFormats::BuildPattern(SupportedFormats::kCode);

    std::wstring filter;
    auto addFilter = [&filter](const std::wstring& desc, const std::wstring& pattern) {
        filter += desc;
        filter.push_back(L'\0');
        filter += pattern;
        filter.push_back(L'\0');
    };

    addFilter(L"All Supported Documents", allPattern);
    addFilter(L"Text Documents (" + textPattern + L")", textPattern);
    addFilter(L"Markdown Documents (" + mdPattern + L")", mdPattern);
    addFilter(L"Configuration Files (" + configPattern + L")", configPattern);
    addFilter(L"Structured Data (" + dataPattern + L")", dataPattern);
    addFilter(L"Source Code & Scripts (" + codePattern + L")", codePattern);
    addFilter(L"All Files (*.*)", L"*.*");
    filter.push_back(L'\0'); // double-null terminator

    // Large buffer so multiple selections and long paths fit.
    std::vector<wchar_t> buffer(32768, L'\0');

    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Open Document";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER |
                OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ENABLESIZING;

    if (GetOpenFileNameW(&ofn)) {
        // Single selection: buffer holds the full path. Multi-selection: "dir\0file1\0file2\0\0".
        std::vector<std::wstring> files;
        const wchar_t* first = buffer.data();
        const wchar_t* next = first + wcslen(first) + 1;
        if (*next == L'\0') {
            files.emplace_back(first);
        } else {
            std::wstring dir = first;
            if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
            for (const wchar_t* p = next; *p; p += wcslen(p) + 1) {
                files.push_back(dir + p);
            }
        }

        bool isFirst = true;
        for (const auto& file : files) {
            EditorTab* active = GetActiveTab();
            if (isFirst && active && active->GetFilePath().empty() && !active->IsDirty()) {
                active->OpenFile(file);
                UpdateWindowTitle();
            } else {
                CreateNewTab(file);
            }
            AddRecentFile(file);
            isFirst = false;
        }
    }
}

bool AppWindow::SaveCurrentFile(bool saveAs) {
    EditorTab* active = GetActiveTab();
    if (!active) return false;

    if (saveAs || active->GetFilePath().empty()) {
        std::vector<wchar_t> buffer(32768, L'\0');
        if (!active->GetFilePath().empty()) {
            wcsncpy_s(buffer.data(), buffer.size(), active->GetFilePath().c_str(), _TRUNCATE);
        }

        const std::wstring textPattern = SupportedFormats::BuildPattern(SupportedFormats::kText);
        const std::wstring mdPattern = SupportedFormats::BuildPattern(SupportedFormats::kMarkdown);
        const std::wstring configPattern = SupportedFormats::BuildPattern(SupportedFormats::kConfig);
        const std::wstring dataPattern = SupportedFormats::BuildPattern(SupportedFormats::kData);
        const std::wstring codePattern = SupportedFormats::BuildPattern(SupportedFormats::kCode);

        std::wstring filter;
        auto addFilter = [&filter](const std::wstring& desc, const std::wstring& pattern) {
            filter += desc;
            filter.push_back(L'\0');
            filter += pattern;
            filter.push_back(L'\0');
        };

        addFilter(L"Text Documents (" + textPattern + L")", textPattern);
        addFilter(L"Markdown Documents (" + mdPattern + L")", mdPattern);
        addFilter(L"Configuration Files (" + configPattern + L")", configPattern);
        addFilter(L"Structured Data (" + dataPattern + L")", dataPattern);
        addFilter(L"Source Code & Scripts (" + codePattern + L")", codePattern);
        addFilter(L"All Files (*.*)", L"*.*");
        filter.push_back(L'\0'); // double-null terminator

        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFile = buffer.data();
        ofn.nMaxFile = static_cast<DWORD>(buffer.size());
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = L"Save Document As";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER |
                    OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ENABLESIZING;
        ofn.lpstrDefExt = L"txt";

        if (GetSaveFileNameW(&ofn)) {
            if (active->SaveFile(buffer.data())) {
                UpdateWindowTitle();
                AddRecentFile(buffer.data());
                return true;
            }
        }
        return false;
    } else {
        if (active->Save()) {
            UpdateWindowTitle();
            AddRecentFile(active->GetFilePath());
            return true;
        }
        return false;
    }
}

void AppWindow::PrintDocument() {
    EditorTab* active = GetActiveTab();
    if (!active) return;
    HWND hEdit = active->GetEditHwnd();
    if (!hEdit) return;

    PRINTDLGW pd = { sizeof(PRINTDLGW) };
    pd.hwndOwner = m_hWnd;
    pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies = 1;

    if (!PrintDlgW(&pd)) {
        return; // User clicked Cancel or closed the print dialog
    }

    HDC hPrnDC = pd.hDC;
    if (!hPrnDC) {
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }

    int dpiX = GetDeviceCaps(hPrnDC, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hPrnDC, LOGPIXELSY);
    int physWidth = GetDeviceCaps(hPrnDC, PHYSICALWIDTH);
    int physHeight = GetDeviceCaps(hPrnDC, PHYSICALHEIGHT);

    if (physWidth == 0) physWidth = GetDeviceCaps(hPrnDC, HORZRES);
    if (physHeight == 0) physHeight = GetDeviceCaps(hPrnDC, VERTRES);

    // Convert page dimensions to twips (1 inch = 1440 twips)
    int pageTwipsW = MulDiv(physWidth, 1440, dpiX);
    int pageTwipsH = MulDiv(physHeight, 1440, dpiY);

    // Standard 0.75-inch side margins and 1-inch top/bottom margins (for header and footer)
    int marginSideTwips   = 1080; // 0.75 in
    int marginTopTwips    = 1440; // 1.00 in
    int marginBottomTwips = 1440; // 1.00 in

    RECT rcPage = { 0, 0, pageTwipsW, pageTwipsH };
    RECT rcPrintable = {
        marginSideTwips,
        marginTopTwips,
        pageTwipsW - marginSideTwips,
        pageTwipsH - marginBottomTwips
    };

    // Calculate pixel coordinates for Header & Footer drawing
    int printPixelLeft = MulDiv(marginSideTwips, dpiX, 1440);
    int printPixelRight = MulDiv(pageTwipsW - marginSideTwips, dpiX, 1440);
    int printPixelTopHeader = MulDiv(marginTopTwips / 2, dpiY, 1440);
    int printPixelBottomFooter = MulDiv(pageTwipsH - (marginBottomTwips * 3 / 4), dpiY, 1440);

    DOCINFOW di = { sizeof(DOCINFOW) };
    std::wstring docTitle = active->GetFileName();
    di.lpszDocName = docTitle.c_str();

    if (StartDocW(hPrnDC, &di) > 0) {
        // Create clean 9pt Segoe UI font for Header and Footer
        int fontH = -MulDiv(9, dpiY, 72);
        HFONT hHdrFont = CreateFontW(
            fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
        );

        // Ensure printed text is always crisp black on white paper, regardless of Dark Mode
        CHARFORMAT2W cfPrint = { sizeof(CHARFORMAT2W) };
        cfPrint.dwMask = CFM_COLOR;
        cfPrint.dwEffects = 0;
        cfPrint.crTextColor = RGB(0, 0, 0);

        CHARFORMAT2W cfRestore = { sizeof(CHARFORMAT2W) };
        cfRestore.dwMask = CFM_COLOR;
        cfRestore.dwEffects = 0;
        cfRestore.crTextColor = m_darkMode ? RGB(240, 240, 240) : RGB(20, 20, 20);

        SendMessage(hEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cfPrint));

        GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
        LONG totalChars = static_cast<LONG>(SendMessage(hEdit, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gtl), 0));
        LONG cpMin = 0;
        int pageNum = 1;

        do {
            if (StartPage(hPrnDC) <= 0) break;

            // Draw Header and Footer
            HFONT hOldFont = hHdrFont ? static_cast<HFONT>(SelectObject(hPrnDC, hHdrFont)) : nullptr;
            SetBkMode(hPrnDC, TRANSPARENT);
            SetTextColor(hPrnDC, RGB(80, 80, 80));

            RECT rcHdr = { printPixelLeft, printPixelTopHeader, printPixelRight, printPixelTopHeader + MulDiv(16, dpiY, 72) };
            DrawTextW(hPrnDC, docTitle.c_str(), -1, &rcHdr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

            std::wstring footerText = L"Page " + std::to_wstring(pageNum);
            RECT rcFtr = { printPixelLeft, printPixelBottomFooter, printPixelRight, printPixelBottomFooter + MulDiv(16, dpiY, 72) };
            DrawTextW(hPrnDC, footerText.c_str(), -1, &rcFtr, DT_CENTER | DT_SINGLELINE);

            if (hOldFont) SelectObject(hPrnDC, hOldFont);

            FORMATRANGE fr = {};
            fr.hdc = hPrnDC;
            fr.hdcTarget = hPrnDC;
            fr.rc = rcPrintable;
            fr.rcPage = rcPage;
            fr.chrg.cpMin = cpMin;
            fr.chrg.cpMax = -1;

            LONG cpNext = static_cast<LONG>(SendMessage(hEdit, EM_FORMATRANGE, TRUE, reinterpret_cast<LPARAM>(&fr)));

            EndPage(hPrnDC);

            if (cpNext <= cpMin || cpNext >= totalChars) {
                break;
            }
            cpMin = cpNext;
            pageNum++;
        } while (cpMin < totalChars);

        // Free formatting cache and restore editor colors
        SendMessage(hEdit, EM_FORMATRANGE, FALSE, 0);
        SendMessage(hEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cfRestore));
        if (active->IsSyntaxEnabled() && active->GetLanguage() != SyntaxLanguage::None) {
            active->ColorizeViewport();
        }

        if (hHdrFont) DeleteObject(hHdrFont);
        EndDoc(hPrnDC);
    }

    DeleteDC(hPrnDC);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

void AppWindow::ToggleWordWrap() {
    m_wordWrap = !m_wordWrap;
    for (auto& tab : m_tabs) {
        tab->SetWordWrap(m_wordWrap);
    }
    CheckMenuItem(m_hMenu, IDM_VIEW_WORDWRAP, m_wordWrap ? MF_CHECKED : MF_UNCHECKED);

    HKEY hAppKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, nullptr, 0, KEY_WRITE, nullptr, &hAppKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = m_wordWrap ? 1 : 0;
        RegSetValueExW(hAppKey, L"WordWrap", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hAppKey);
    }
}

void AppWindow::ToggleSyntaxHighlighting() {
    m_syntaxHighlighting = !m_syntaxHighlighting;
    for (auto& tab : m_tabs) {
        tab->SetSyntaxEnabled(m_syntaxHighlighting);
    }
    CheckMenuItem(m_hMenu, IDM_VIEW_SYNTAX, m_syntaxHighlighting ? MF_CHECKED : MF_UNCHECKED);

    HKEY hAppKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, nullptr, 0, KEY_WRITE, nullptr, &hAppKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = m_syntaxHighlighting ? 1 : 0;
        RegSetValueExW(hAppKey, L"SyntaxHighlighting", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hAppKey);
    }
}

void AppWindow::ToggleDarkMode() {
    m_darkMode = !m_darkMode;

    HKEY hAppKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, nullptr, 0, KEY_WRITE, nullptr, &hAppKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = m_darkMode ? 1 : 0;
        RegSetValueExW(hAppKey, L"DarkMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hAppKey);
    }

    ApplyTheme();
}

void AppWindow::AdjustZoom(int delta) {
    EditorTab* active = GetActiveTab();
    if (!active) return;
    int zoom = active->GetZoom() + delta;
    active->SetZoom(zoom);
    UpdateWindowTitle();
}

void AppWindow::ResetZoom() {
    EditorTab* active = GetActiveTab();
    if (!active) return;
    active->SetZoom(100);
    UpdateWindowTitle();
}

void AppWindow::InsertDateTime() {
    EditorTab* active = GetActiveTab();
    if (!active) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t szDate[128];
    GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, szDate, 64);
    wcscat_s(szDate, L" ");
    wchar_t szDatePart[64];
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, szDatePart, 64);
    wcscat_s(szDate, szDatePart);

    active->InsertText(szDate);
}

void AppWindow::OpenFindDialog(bool isReplace) {
    if (m_hFindReplaceDialog) {
        SetFocus(m_hFindReplaceDialog);
        return;
    }

    ZeroMemory(&m_fr, sizeof(m_fr));
    m_fr.lStructSize = sizeof(m_fr);
    m_fr.hwndOwner = m_hWnd;
    m_fr.lpstrFindWhat = m_findWhat;
    m_fr.wFindWhatLen = sizeof(m_findWhat) / sizeof(wchar_t);
    m_fr.lpstrReplaceWith = m_replaceWith;
    m_fr.wReplaceWithLen = sizeof(m_replaceWith) / sizeof(wchar_t);
    m_fr.Flags = FR_DOWN;

    if (isReplace) {
        m_hFindReplaceDialog = ReplaceTextW(&m_fr);
    } else {
        m_hFindReplaceDialog = FindTextW(&m_fr);
    }
}

void AppWindow::HandleFindReplaceMessage() {
    if (m_fr.Flags & FR_DIALOGTERM) {
        m_hFindReplaceDialog = nullptr;
        return;
    }

    if (m_fr.Flags & FR_FINDNEXT) {
        FindTextInternal((m_fr.Flags & FR_DOWN) != 0);
    } else if (m_fr.Flags & FR_REPLACE) {
        ReplaceInternal(false);
    } else if (m_fr.Flags & FR_REPLACEALL) {
        ReplaceInternal(true);
    }
}

bool AppWindow::FindTextInternal(bool forward, bool wrapAround) {
    EditorTab* active = GetActiveTab();
    if (!active || wcslen(m_findWhat) == 0) return false;

    HWND hEdit = active->GetEditHwnd();
    CHARRANGE cr = {};
    SendMessage(hEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));

    FINDTEXTEXW ft = {};
    ft.lpstrText = m_findWhat;

    if (forward) {
        ft.chrg.cpMin = cr.cpMax;
        ft.chrg.cpMax = -1;
    } else {
        ft.chrg.cpMin = cr.cpMin;
        ft.chrg.cpMax = 0;
    }

    DWORD flags = 0;
    if (forward) flags |= FR_DOWN;
    if (m_fr.Flags & FR_MATCHCASE) flags |= FR_MATCHCASE;
    if (m_fr.Flags & FR_WHOLEWORD) flags |= FR_WHOLEWORD;

    LRESULT pos = SendMessage(hEdit, EM_FINDTEXTEXW, flags, reinterpret_cast<LPARAM>(&ft));
    if (pos == -1 && wrapAround) {
        // Wrap around search
        if (forward) {
            ft.chrg.cpMin = 0;
            ft.chrg.cpMax = -1;
        } else {
            GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
            ft.chrg.cpMin = static_cast<LONG>(SendMessage(hEdit, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gtl), 0));
            ft.chrg.cpMax = 0;
        }
        pos = SendMessage(hEdit, EM_FINDTEXTEXW, flags, reinterpret_cast<LPARAM>(&ft));
    }

    if (pos != -1) {
        SendMessage(hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&ft.chrgText));
        SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
        return true;
    } else {
        std::wstring msg = L"Cannot find \"" + std::wstring(m_findWhat) + L"\"";
        MessageBoxW(m_hWnd, msg.c_str(), L"LightNotePad", MB_OK | MB_ICONINFORMATION);
        return false;
    }
}

void AppWindow::ReplaceInternal(bool all) {
    EditorTab* active = GetActiveTab();
    if (!active || wcslen(m_findWhat) == 0) return;

    HWND hEdit = active->GetEditHwnd();

    if (!all) {
        CHARRANGE cr = {};
        SendMessage(hEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));
        if (cr.cpMax > cr.cpMin) {
            SendMessage(hEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(m_replaceWith));
        }
        FindTextInternal(true);
    } else {
        // Replace all
        CHARRANGE crStart = { 0, 0 };
        SendMessage(hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crStart));

        int replaceCount = 0;
        while (FindTextInternal(true, false)) {
            SendMessage(hEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(m_replaceWith));
            replaceCount++;
        }

        std::wstring msg = L"Replaced " + std::to_wstring(replaceCount) + L" occurrences.";
        MessageBoxW(m_hWnd, msg.c_str(), L"LightNotePad", MB_OK | MB_ICONINFORMATION);
    }
}

void AppWindow::ShowAboutDialog() {
    MessageBoxW(
        m_hWnd,
        L"LightNotePad v1.2.0\n\n"
        L"A lightning-fast, minimalist Windows text editor.\n"
        L"Zero external runtime DLLs · Native Win32 · Pure C++20\n\n"
        L"Developed by Ahmed Amer Shahin",
        L"About LightNotePad",
        MB_OK | MB_ICONINFORMATION
    );
}

void AppWindow::ShowShortcutsDialog() {
    ShortcutsDialog::Show(m_hWnd, m_hInstance, m_darkMode, m_dpi);
}

void AppWindow::LoadFontSettings() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\Font", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t faceBuffer[LF_FACESIZE] = {};
        DWORD faceSize = sizeof(faceBuffer);
        if (RegQueryValueExW(hKey, L"FontFace", nullptr, nullptr, reinterpret_cast<LPBYTE>(faceBuffer), &faceSize) == ERROR_SUCCESS && faceBuffer[0] != L'\0') {
            m_fontFamily = faceBuffer;
        }

        DWORD sizeVal = 0;
        DWORD dwSize = sizeof(sizeVal);
        if (RegQueryValueExW(hKey, L"FontSize", nullptr, nullptr, reinterpret_cast<LPBYTE>(&sizeVal), &dwSize) == ERROR_SUCCESS && sizeVal >= 6 && sizeVal <= 144) {
            m_editorFontSizePt = static_cast<int>(sizeVal);
        }

        DWORD weightVal = 0;
        dwSize = sizeof(weightVal);
        if (RegQueryValueExW(hKey, L"FontWeight", nullptr, nullptr, reinterpret_cast<LPBYTE>(&weightVal), &dwSize) == ERROR_SUCCESS && weightVal > 0) {
            m_editorFontWeight = static_cast<int>(weightVal);
        }

        DWORD italicVal = 0;
        dwSize = sizeof(italicVal);
        if (RegQueryValueExW(hKey, L"FontItalic", nullptr, nullptr, reinterpret_cast<LPBYTE>(&italicVal), &dwSize) == ERROR_SUCCESS) {
            m_editorFontItalic = (italicVal != 0);
        }

        RegCloseKey(hKey);
    }
}

void AppWindow::SaveFontSettings() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\Font", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"FontFace", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(m_fontFamily.c_str()),
            static_cast<DWORD>((m_fontFamily.size() + 1) * sizeof(wchar_t)));

        DWORD sizeVal = static_cast<DWORD>(m_editorFontSizePt);
        RegSetValueExW(hKey, L"FontSize", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&sizeVal), sizeof(sizeVal));

        DWORD weightVal = static_cast<DWORD>(m_editorFontWeight);
        RegSetValueExW(hKey, L"FontWeight", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&weightVal), sizeof(weightVal));

        DWORD italicVal = m_editorFontItalic ? 1 : 0;
        RegSetValueExW(hKey, L"FontItalic", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&italicVal), sizeof(italicVal));

        RegCloseKey(hKey);
    }
}

void AppWindow::OpenFontDialog() {
    LOGFONTW lf = {};
    int fontHeight = -MulDiv(m_editorFontSizePt, m_dpi, 72);
    lf.lfHeight = fontHeight;
    lf.lfWeight = m_editorFontWeight;
    lf.lfItalic = m_editorFontItalic ? TRUE : FALSE;
    wcsncpy_s(lf.lfFaceName, m_fontFamily.c_str(), LF_FACESIZE - 1);

    CHOOSEFONTW cf = { sizeof(CHOOSEFONTW) };
    cf.hwndOwner = m_hWnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_FORCEFONTEXIST;

    if (ChooseFontW(&cf)) {
        m_fontFamily = lf.lfFaceName;
        m_editorFontWeight = lf.lfWeight;
        m_editorFontItalic = (lf.lfItalic != FALSE);
        if (cf.iPointSize > 0) {
            m_editorFontSizePt = cf.iPointSize / 10;
        } else if (lf.lfHeight != 0) {
            m_editorFontSizePt = MulDiv(std::abs(lf.lfHeight), 72, m_dpi);
        }

        CreateEditorFont();
        SaveFontSettings();
    }
}

void AppWindow::ResetFont() {
    m_fontFamily = L"Cascadia Mono";
    m_editorFontSizePt = 11;
    m_editorFontWeight = FW_NORMAL;
    m_editorFontItalic = false;

    SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\Font");

    CreateEditorFont();
}

void AppWindow::LoadRecentFiles() {
    m_recentFiles.clear();
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\RecentFiles", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        for (int i = 1; i <= 9; ++i) {
            std::wstring valName = L"File" + std::to_wstring(i);
            wchar_t pathBuffer[MAX_PATH] = {};
            DWORD dwSize = sizeof(pathBuffer);
            DWORD dwType = 0;
            if (RegQueryValueExW(hKey, valName.c_str(), nullptr, &dwType, reinterpret_cast<LPBYTE>(pathBuffer), &dwSize) == ERROR_SUCCESS && dwType == REG_SZ && pathBuffer[0] != L'\0') {
                m_recentFiles.push_back(pathBuffer);
            }
        }
        RegCloseKey(hKey);
    }
}

void AppWindow::SaveRecentFiles() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\RecentFiles", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        for (int i = 0; i < 9; ++i) {
            std::wstring valName = L"File" + std::to_wstring(i + 1);
            if (i < static_cast<int>(m_recentFiles.size())) {
                const auto& path = m_recentFiles[i];
                RegSetValueExW(hKey, valName.c_str(), 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(path.c_str()),
                    static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
            } else {
                RegDeleteValueW(hKey, valName.c_str());
            }
        }
        RegCloseKey(hKey);
    }
}

void AppWindow::AddRecentFile(const std::wstring& path) {
    if (path.empty()) return;

    wchar_t fullPath[MAX_PATH] = {};
    if (GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr) > 0) {
        // Windows Jump List notification
        SHAddToRecentDocs(SHARD_PATHW, fullPath);

        // Remove if already present (case-insensitive)
        auto it = std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&fullPath](const std::wstring& item) {
                return _wcsicmp(item.c_str(), fullPath) == 0;
            });
        m_recentFiles.erase(it, m_recentFiles.end());

        // Insert at the beginning
        m_recentFiles.insert(m_recentFiles.begin(), fullPath);

        // Limit to 9 items
        if (m_recentFiles.size() > 9) {
            m_recentFiles.resize(9);
        }

        SaveRecentFiles();
        UpdateRecentMenu();
    }
}

void AppWindow::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
    UpdateRecentMenu();
}

void AppWindow::UpdateRecentMenu() {
    if (!m_hRecentMenu) return;

    while (GetMenuItemCount(m_hRecentMenu) > 0) {
        RemoveMenu(m_hRecentMenu, 0, MF_BYPOSITION);
    }

    if (m_recentFiles.empty()) {
        AppendMenuW(m_hRecentMenu, MF_STRING | MF_GRAYED, 0, L"(No Recent Files)");
    } else {
        for (size_t i = 0; i < m_recentFiles.size(); ++i) {
            wchar_t compactPath[MAX_PATH] = {};
            PathCompactPathExW(compactPath, m_recentFiles[i].c_str(), 60, 0);

            // Double ampersands so Win32 menu doesn't treat file path '&' as mnemonics
            std::wstring safePath;
            safePath.reserve(wcslen(compactPath) + 4);
            for (const wchar_t* p = compactPath; *p; ++p) {
                if (*p == L'&') {
                    safePath += L"&&";
                } else {
                    safePath += *p;
                }
            }

            std::wstring itemText = L"&" + std::to_wstring(i + 1) + L"  " + safePath;
            AppendMenuW(m_hRecentMenu, MF_STRING, IDM_FILE_RECENT_FIRST + static_cast<UINT>(i), itemText.c_str());
        }

        AppendMenuW(m_hRecentMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_hRecentMenu, MF_STRING, IDM_FILE_RECENT_CLEAR, L"&Clear Recent List");
    }
}

void AppWindow::OpenRecentFile(int index) {
    if (index < 0 || index >= static_cast<int>(m_recentFiles.size())) {
        return;
    }

    std::wstring path = m_recentFiles[index];

    // Lazy disk validation: check only on click
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring msg = L"The file:\n" + path + L"\n\ndoes not exist or is no longer accessible.\nWould you like to remove it from the recent files list?";
        if (MessageBoxW(m_hWnd, msg.c_str(), L"LightNotePad", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            m_recentFiles.erase(m_recentFiles.begin() + index);
            SaveRecentFiles();
            UpdateRecentMenu();
        }
        return;
    }

    // Open file into current clean tab or new tab
    EditorTab* active = GetActiveTab();
    if (active && active->GetFilePath().empty() && !active->IsDirty()) {
        active->OpenFile(path);
        UpdateWindowTitle();
    } else {
        CreateNewTab(path);
    }

    // Move to top of MRU
    AddRecentFile(path);
}


bool AppWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::vector<std::wstring>& initialFiles) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        WINDOW_CLASS_NAME,
        L"LightNotePad",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        960, 640,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) {
        return false;
    }

    UINT dpi = GetDpiForWindow(m_hWnd);
    m_dpi = (dpi == 0) ? 96 : dpi;

    LoadFontSettings();
    CreateAppMenu();
    CreateAccelerators();
    CreateEditorFont();

    m_tabBar.Create(m_hWnd, hInstance);
    m_statusBar.Create(m_hWnd, hInstance);

    // Tab bar callbacks
    m_tabBar.onTabSelected = [this](int id) { SwitchToTab(id); };
    m_tabBar.onTabCloseRequested = [this](int id) { CloseTab(id); };
    m_tabBar.onNewTabRequested = [this]() { CreateNewTab(); };
    m_tabBar.onTabContextMenu = [this](size_t targetIndex, int targetTabId, POINT ptScreen) {
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i]->GetTabId() == targetTabId) {
                targetIndex = i;
                break;
            }
        }
        ShowTabContextMenu(targetIndex, ptScreen);
    };

    InitDarkMode();

    // Load WordWrap and SyntaxHighlighting settings
    HKEY hAppKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hAppKey, L"WordWrap", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            m_wordWrap = (val != 0);
        }
        val = 1;
        size = sizeof(val);
        if (RegQueryValueExW(hAppKey, L"SyntaxHighlighting", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            m_syntaxHighlighting = (val != 0);
        }
        RegCloseKey(hAppKey);
    }
    CheckMenuItem(m_hMenu, IDM_VIEW_WORDWRAP, m_wordWrap ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m_hMenu, IDM_VIEW_SYNTAX, m_syntaxHighlighting ? MF_CHECKED : MF_UNCHECKED);

    LoadRecentFiles();
    UpdateRecentMenu();

    // Open initial files or blank tab
    if (initialFiles.empty()) {
        CreateNewTab();
    } else {
        for (const auto& f : initialFiles) {
            CreateNewTab(f);
            AddRecentFile(f);
        }
    }

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    return true;
}

LRESULT CALLBACK AppWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == m_uFindMsgString && m_uFindMsgString != 0) {
        HandleFindReplaceMessage();
        return 0;
    }

    switch (msg) {
    case WM_UAHDRAWMENU: {
        if (!m_darkMode) break;
        auto* pUDM = reinterpret_cast<UAHMENU*>(lParam);
        MENUBARINFO mbi = { sizeof(MENUBARINFO) };
        GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi);
        RECT rcWindow;
        GetWindowRect(m_hWnd, &rcWindow);
        RECT rc = mbi.rcBar;
        OffsetRect(&rc, -rcWindow.left, -rcWindow.top);
        HBRUSH hBr = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(pUDM->hdc, &rc, hBr);
        DeleteObject(hBr);
        return TRUE;
    }
    case WM_UAHDRAWMENUITEM: {
        if (!m_darkMode) break;
        auto* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
        bool isHot = (pUDMI->dis.itemState & (ODS_HOTLIGHT | ODS_SELECTED)) != 0;
        COLORREF bg = isHot ? RGB(55, 55, 55) : RGB(30, 30, 30);
        COLORREF fg = isHot ? RGB(255, 255, 255) : RGB(220, 220, 220);

        HBRUSH hBr = CreateSolidBrush(bg);
        FillRect(pUDMI->dis.hDC, &pUDMI->dis.rcItem, hBr);
        DeleteObject(hBr);

        wchar_t text[128] = {};
        MENUITEMINFOW mii = { sizeof(MENUITEMINFOW), MIIM_STRING };
        mii.dwTypeData = text;
        mii.cch = _countof(text);
        GetMenuItemInfoW(pUDMI->uahMenu.hmenu, pUDMI->iPosition, TRUE, &mii);

        SetBkMode(pUDMI->dis.hDC, TRANSPARENT);
        SetTextColor(pUDMI->dis.hDC, fg);
        HFONT oldFont = m_hMenuFont ? static_cast<HFONT>(SelectObject(pUDMI->dis.hDC, m_hMenuFont)) : nullptr;
        DrawTextW(pUDMI->dis.hDC, text, -1, &pUDMI->dis.rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (oldFont) SelectObject(pUDMI->dis.hDC, oldFont);
        return TRUE;
    }
    case WM_SIZE: {
        UpdateLayout();
        return 0;
    }
    case WM_DPICHANGED: {
        auto* prc = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(m_hWnd, nullptr, prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateDpi(HIWORD(wParam));
        return 0;
    }
    case WM_SETTINGCHANGE: {
        if (lParam && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) {
            InitDarkMode();
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == 101) {
            KillTimer(m_hWnd, 101);
            EditorTab* active = GetActiveTab();
            if (active) {
                active->ColorizeViewport();
            }
            return 0;
        }
        break;
    }
    case WM_SETFOCUS: {
        EditorTab* active = GetActiveTab();
        if (active) active->SetFocus();
        return 0;
    }
    case WM_INITMENUPOPUP: {
        HMENU hPopup = reinterpret_cast<HMENU>(wParam);
        EnableMenuItem(hPopup, IDM_TAB_REOPEN_CLOSED, m_closedTabsHistory.empty() ? (MF_BYCOMMAND | MF_GRAYED) : (MF_BYCOMMAND | MF_ENABLED));
        break;
    }
    case WM_COMMAND: {
        WORD code = HIWORD(wParam);
        WORD id = LOWORD(wParam);

        // Edit control notifications
        if (code == EN_CHANGE) {
            EditorTab* active = GetActiveTab();
            if (active) {
                active->OnTextChanged();
            }
            UpdateWindowTitle();
            SetTimer(m_hWnd, 101, 30, nullptr);
            return 0;
        }
        if (code == EN_SELCHANGE) {
            UpdateWindowTitle();
            return 0;
        }

        if (id >= IDM_FILE_RECENT_FIRST && id <= IDM_FILE_RECENT_LAST) {
            OpenRecentFile(id - IDM_FILE_RECENT_FIRST);
            return 0;
        }

        switch (id) {
        case IDM_FILE_NEW:        CreateNewTab(); break;
        case IDM_FILE_NEW_WINDOW: {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            ShellExecuteW(nullptr, L"open", exePath, L"--new-window", nullptr, SW_SHOWNORMAL);
            break;
        }
        case IDM_FILE_OPEN:       OpenFileDialog(); break;
        case IDM_FILE_RECENT_CLEAR: ClearRecentFiles(); break;
        case IDM_FILE_SAVE:       SaveCurrentFile(false); break;
        case IDM_FILE_SAVEAS:     SaveCurrentFile(true); break;
        case IDM_FILE_PRINT:      PrintDocument(); break;
        case IDM_FILE_CLOSETAB: {
            EditorTab* active = GetActiveTab();
            if (active) CloseTab(active->GetTabId());
            break;
        }
        case IDM_TAB_REOPEN_CLOSED:
            ReopenClosedTab();
            break;
        case IDM_FILE_EXIT:       SendMessage(m_hWnd, WM_CLOSE, 0, 0); break;

        case IDM_EDIT_UNDO:       { EditorTab* a = GetActiveTab(); if (a) a->Undo(); break; }
        case IDM_EDIT_REDO:       { EditorTab* a = GetActiveTab(); if (a) a->Redo(); break; }
        case IDM_EDIT_CUT:        { EditorTab* a = GetActiveTab(); if (a) a->Cut(); break; }
        case IDM_EDIT_COPY:       { EditorTab* a = GetActiveTab(); if (a) a->Copy(); break; }
        case IDM_EDIT_PASTE:      { EditorTab* a = GetActiveTab(); if (a) a->Paste(); break; }
        case IDM_EDIT_DELETE:     { EditorTab* a = GetActiveTab(); if (a) a->Delete(); break; }
        case IDM_EDIT_SELECTALL:  { EditorTab* a = GetActiveTab(); if (a) a->SelectAll(); break; }
        case IDM_EDIT_TIMEDATE:   InsertDateTime(); break;

        case IDM_SEARCH_FIND:     OpenFindDialog(false); break;
        case IDM_SEARCH_FINDNEXT: FindTextInternal(true); break;
        case IDM_SEARCH_FINDPREV: FindTextInternal(false); break;
        case IDM_SEARCH_REPLACE:  OpenFindDialog(true); break;

        case IDM_VIEW_WORDWRAP:   ToggleWordWrap(); break;
        case IDM_VIEW_FONT:       OpenFontDialog(); break;
        case IDM_VIEW_FONTRESET:  ResetFont(); break;
        case IDM_VIEW_ZOOMIN:     AdjustZoom(10); break;
        case IDM_VIEW_ZOOMOUT:    AdjustZoom(-10); break;
        case IDM_VIEW_ZOOMRESET:  ResetZoom(); break;
        case IDM_VIEW_STATUSBAR: {
            bool vis = !m_statusBar.IsVisible();
            m_statusBar.SetVisible(vis);
            CheckMenuItem(m_hMenu, IDM_VIEW_STATUSBAR, vis ? MF_CHECKED : MF_UNCHECKED);
            UpdateLayout();
            break;
        }
        case IDM_VIEW_DARKMODE:   ToggleDarkMode(); break;
        case IDM_VIEW_SYNTAX:     ToggleSyntaxHighlighting(); break;

        case IDM_TAB_NEXT: {
            if (m_tabs.size() > 1) {
                int activeId = m_activeTabId;
                for (size_t i = 0; i < m_tabs.size(); ++i) {
                    if (m_tabs[i]->GetTabId() == activeId) {
                        size_t nextIdx = (i + 1) % m_tabs.size();
                        SwitchToTab(m_tabs[nextIdx]->GetTabId());
                        break;
                    }
                }
            }
            break;
        }
        case IDM_TAB_PREV: {
            if (m_tabs.size() > 1) {
                int activeId = m_activeTabId;
                for (size_t i = 0; i < m_tabs.size(); ++i) {
                    if (m_tabs[i]->GetTabId() == activeId) {
                        size_t prevIdx = (i + m_tabs.size() - 1) % m_tabs.size();
                        SwitchToTab(m_tabs[prevIdx]->GetTabId());
                        break;
                    }
                }
            }
            break;
        }

        case IDM_HELP_SHORTCUTS:  ShowShortcutsDialog(); break;
        case IDM_HELP_ABOUT:      ShowAboutDialog(); break;
        }
        return 0;
    }
    case WM_COPYDATA: {
        auto* pCds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (pCds && pCds->cbData > 0 && pCds->lpData) {
            const wchar_t* pFilePath = reinterpret_cast<const wchar_t*>(pCds->lpData);
            if (pFilePath && *pFilePath) {
                CreateNewTab(pFilePath);
                AddRecentFile(pFilePath);
            }
        }
        return TRUE;
    }
    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < fileCount; ++i) {
            wchar_t szFile[MAX_PATH];
            if (DragQueryFileW(hDrop, i, szFile, MAX_PATH) > 0) {
                EditorTab* active = GetActiveTab();
                if (i == 0 && active && active->GetFilePath().empty() && !active->IsDirty()) {
                    active->OpenFile(szFile);
                    UpdateWindowTitle();
                } else {
                    CreateNewTab(szFile);
                }
                AddRecentFile(szFile);
            }
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_CONTEXTMENU: {
        POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (ptScreen.x != -1 || ptScreen.y != -1) {
            POINT ptClient = ptScreen;
            ScreenToClient(m_hWnd, &ptClient);
            int tabHeight = m_tabBar.GetDesiredHeight(m_dpi);
            if (ptClient.y >= 0 && ptClient.y <= tabHeight) {
                POINT ptTabClient = ptScreen;
                ScreenToClient(m_tabBar.GetHwnd(), &ptTabClient);
                int hit = m_tabBar.HitTestTab(ptTabClient);
                if (hit >= 0 && hit < static_cast<int>(m_tabs.size())) {
                    ShowTabContextMenu(static_cast<size_t>(hit), ptScreen);
                    return 0;
                }
            }
        }
        break;
    }
    case WM_CLOSE: {
        // Prompt save for any dirty tabs
        for (auto& tab : m_tabs) {
            if (!PromptSaveIfDirty(tab.get())) {
                return 0; // User cancelled
            }
        }
        DestroyWindow(m_hWnd);
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

void AppWindow::RegisterShellAssociations() {
    wchar_t exePath[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, exePath, _countof(exePath));

    auto setString = [](HKEY key, const wchar_t* name, const std::wstring& value) {
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                       static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));
    };

    const std::vector<std::wstring> extensions = SupportedFormats::GetAllExtensions();
    const wchar_t* progId = L"LightNotePad.Document";

    // 1. ProgID under HKCU\Software\Classes\LightNotePad.Document
    HKEY hKeyProgId = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyProgId, nullptr) == ERROR_SUCCESS) {

        const wchar_t desc[] = L"Text Document (LightNotePad)";
        RegSetValueExW(hKeyProgId, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(desc),
                       static_cast<DWORD>((wcslen(desc) + 1) * sizeof(wchar_t)));

        // Default Icon: exePath,0
        HKEY hKeyIcon = nullptr;
        if (RegCreateKeyExW(hKeyProgId, L"DefaultIcon", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyIcon, nullptr) == ERROR_SUCCESS) {
            std::wstring iconVal = std::wstring(exePath) + L",0";
            RegSetValueExW(hKeyIcon, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()),
                           static_cast<DWORD>((iconVal.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKeyIcon);
        }

        // shell\open\command: "exePath" "%1"
        HKEY hKeyCmd = nullptr;
        if (RegCreateKeyExW(hKeyProgId, L"shell\\open\\command", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCmd, nullptr) == ERROR_SUCCESS) {
            std::wstring cmdVal = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
            RegSetValueExW(hKeyCmd, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmdVal.c_str()),
                           static_cast<DWORD>((cmdVal.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKeyCmd);
        }

        RegCloseKey(hKeyProgId);
    }

    // 2. OpenWithProgids for every supported extension (REG_NONE)
    for (const auto& ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + ext + L"\\OpenWithProgids";
        HKEY hKeyExt = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyExt, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKeyExt, progId, 0, REG_NONE, nullptr, 0);
            RegCloseKey(hKeyExt);
        }
    }

    // 3. Applications\LightNotePad.exe: Open with lists and Explorer integration
    const std::wstring exeName = std::filesystem::path(exePath).filename().wstring();
    const std::wstring appKeyPath = L"Software\\Classes\\Applications\\" + exeName;
    HKEY hKeyApp = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, appKeyPath.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyApp, nullptr) == ERROR_SUCCESS) {
        setString(hKeyApp, L"FriendlyAppName", L"LightNotePad");

        HKEY hKeyCmd = nullptr;
        if (RegCreateKeyExW(hKeyApp, L"shell\\open\\command", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCmd, nullptr) == ERROR_SUCCESS) {
            setString(hKeyCmd, nullptr, L"\"" + std::wstring(exePath) + L"\" \"%1\"");
            RegCloseKey(hKeyCmd);
        }

        HKEY hKeyTypes = nullptr;
        if (RegCreateKeyExW(hKeyApp, L"SupportedTypes", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyTypes, nullptr) == ERROR_SUCCESS) {
            for (const auto& ext : extensions) {
                setString(hKeyTypes, ext.c_str(), L"");
            }
            RegCloseKey(hKeyTypes);
        }
        RegCloseKey(hKeyApp);
    }

    // 4. Capabilities + RegisteredApplications: lists LightNotePad in
    // Windows Settings > Apps > Default apps so users can choose it as the default editor.
    HKEY hKeyCaps = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\Capabilities", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCaps, nullptr) == ERROR_SUCCESS) {
        setString(hKeyCaps, L"ApplicationName", L"LightNotePad");
        setString(hKeyCaps, L"ApplicationDescription", L"Ultra-fast, minimalist text editor");

        HKEY hKeyAssoc = nullptr;
        if (RegCreateKeyExW(hKeyCaps, L"FileAssociations", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyAssoc, nullptr) == ERROR_SUCCESS) {
            for (const auto& ext : extensions) {
                setString(hKeyAssoc, ext.c_str(), progId);
            }
            RegCloseKey(hKeyAssoc);
        }
        RegCloseKey(hKeyCaps);
    }

    HKEY hKeyRegApps = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyRegApps, nullptr) == ERROR_SUCCESS) {
        setString(hKeyRegApps, L"LightNotePad", L"Software\\LightNotePad\\Capabilities");
        RegCloseKey(hKeyRegApps);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void AppWindow::UnregisterShellAssociations() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\LightNotePad.exe");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\LightNotePad\\Capabilities");

    HKEY hKeyRegApps = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, KEY_WRITE, &hKeyRegApps) == ERROR_SUCCESS) {
        RegDeleteValueW(hKeyRegApps, L"LightNotePad");
        RegCloseKey(hKeyRegApps);
    }

    const std::vector<std::wstring> extensions = SupportedFormats::GetAllExtensions();
    for (const auto& ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + ext + L"\\OpenWithProgids";
        HKEY hKeyExt = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_WRITE, &hKeyExt) == ERROR_SUCCESS) {
            RegDeleteValueW(hKeyExt, L"LightNotePad.Document");
            RegCloseKey(hKeyExt);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
