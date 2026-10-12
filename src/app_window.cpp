#include "pch.hpp"
#include "app_window.hpp"
#include "shortcuts_dialog.hpp"

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

    // Try Cascadia Mono first
    m_hEditorFont = CreateFontW(
        fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, m_fontFamily.c_str()
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
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save As...\tCtrl+Shift+S");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_CLOSETAB, L"Close Tab\tCtrl+W");
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
        { FCONTROL | FVIRTKEY, 'W', IDM_FILE_CLOSETAB },

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
    wchar_t szFile[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Documents (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        EditorTab* active = GetActiveTab();
        if (active && active->GetFilePath().empty() && !active->IsDirty()) {
            active->OpenFile(szFile);
            UpdateWindowTitle();
        } else {
            CreateNewTab(szFile);
        }
    }
}

bool AppWindow::SaveCurrentFile(bool saveAs) {
    EditorTab* active = GetActiveTab();
    if (!active) return false;

    if (saveAs || active->GetFilePath().empty()) {
        wchar_t szFile[MAX_PATH] = {};
        if (!active->GetFilePath().empty()) {
            wcscpy_s(szFile, active->GetFilePath().c_str());
        }

        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Text Documents (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
        ofn.lpstrDefExt = L"txt";

        if (GetSaveFileNameW(&ofn)) {
            if (active->SaveFile(szFile)) {
                UpdateWindowTitle();
                return true;
            }
        }
        return false;
    } else {
        if (active->Save()) {
            UpdateWindowTitle();
            return true;
        }
        return false;
    }
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
        L"LightNotePad v1.0\n\n"
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

    CreateAppMenu();
    CreateAccelerators();
    CreateEditorFont();

    m_tabBar.Create(m_hWnd, hInstance);
    m_statusBar.Create(m_hWnd, hInstance);

    // Tab bar callbacks
    m_tabBar.onTabSelected = [this](int id) { SwitchToTab(id); };
    m_tabBar.onTabCloseRequested = [this](int id) { CloseTab(id); };
    m_tabBar.onNewTabRequested = [this]() { CreateNewTab(); };

    InitDarkMode();

    // Load WordWrap setting
    HKEY hAppKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\LightNotePad", 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hAppKey, L"WordWrap", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            m_wordWrap = (val != 0);
        }
        RegCloseKey(hAppKey);
    }
    CheckMenuItem(m_hMenu, IDM_VIEW_WORDWRAP, m_wordWrap ? MF_CHECKED : MF_UNCHECKED);

    // Open initial files or blank tab
    if (initialFiles.empty()) {
        CreateNewTab();
    } else {
        for (const auto& f : initialFiles) {
            CreateNewTab(f);
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
    case WM_SETFOCUS: {
        EditorTab* active = GetActiveTab();
        if (active) active->SetFocus();
        return 0;
    }
    case WM_COMMAND: {
        WORD code = HIWORD(wParam);
        WORD id = LOWORD(wParam);

        // Edit control notifications
        if (code == EN_CHANGE || code == EN_SELCHANGE) {
            UpdateWindowTitle();
            return 0;
        }

        switch (id) {
        case IDM_FILE_NEW:        CreateNewTab(); break;
        case IDM_FILE_NEW_WINDOW: {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            ShellExecuteW(nullptr, L"open", exePath, nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case IDM_FILE_OPEN:       OpenFileDialog(); break;
        case IDM_FILE_SAVE:       SaveCurrentFile(false); break;
        case IDM_FILE_SAVEAS:     SaveCurrentFile(true); break;
        case IDM_FILE_CLOSETAB: {
            EditorTab* active = GetActiveTab();
            if (active) CloseTab(active->GetTabId());
            break;
        }
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
            }
        }
        DragFinish(hDrop);
        return 0;
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
