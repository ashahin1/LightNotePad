#include "pch.hpp"
#include "editor_tab.hpp"

namespace {
// Stream callback for large text input
struct StreamInCookie {
    const wchar_t* pText;
    size_t length;
    size_t pos;
};

DWORD CALLBACK StreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb) {
    auto* cookie = reinterpret_cast<StreamInCookie*>(dwCookie);
    size_t remainingBytes = (cookie->length - cookie->pos) * sizeof(wchar_t);
    size_t copyBytes = (static_cast<size_t>(cb) < remainingBytes) ? static_cast<size_t>(cb) : remainingBytes;

    if (copyBytes > 0) {
        memcpy(pbBuff, reinterpret_cast<const char*>(cookie->pText) + (cookie->pos * sizeof(wchar_t)), copyBytes);
        cookie->pos += copyBytes / sizeof(wchar_t);
        *pcb = static_cast<LONG>(copyBytes);
    } else {
        *pcb = 0;
    }
    return 0;
}

// Stream callback for large text output
struct StreamOutCookie {
    std::wstring* pText;
};

DWORD CALLBACK StreamOutCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb) {
    auto* cookie = reinterpret_cast<StreamOutCookie*>(dwCookie);
    size_t charCount = static_cast<size_t>(cb) / sizeof(wchar_t);
    if (charCount > 0) {
        cookie->pText->append(reinterpret_cast<const wchar_t*>(pbBuff), charCount);
    }
    *pcb = cb;
    return 0;
}
} // namespace

EditorTab::EditorTab(HWND hParent, HINSTANCE hInstance, int tabId)
    : m_hParent(hParent), m_hInstance(hInstance), m_tabId(tabId) {
}

EditorTab::~EditorTab() {
    Destroy();
}

void EditorTab::Destroy() {
    if (m_hEdit) {
        DestroyWindow(m_hEdit);
        m_hEdit = nullptr;
    }
}

bool EditorTab::Create() {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                  ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN;

    m_hEdit = CreateWindowExW(
        0,
        MSFTEDIT_CLASS,
        L"",
        style,
        0, 0, 100, 100,
        m_hParent,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(1000 + m_tabId)),
        m_hInstance,
        nullptr
    );

    if (!m_hEdit) {
        return false;
    }

    // Configure plain text mode
    SendMessage(m_hEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);

    // Disable rich text URL detection
    SendMessage(m_hEdit, EM_AUTOURLDETECT, FALSE, 0);

    // Set unlimited text limit
    SendMessage(m_hEdit, EM_EXLIMITTEXT, 0, static_cast<LPARAM>(-1));

    // Increase undo buffer limit
    SendMessage(m_hEdit, EM_SETUNDOLIMIT, 1000, 0);

    // Enable change & selection events
    SendMessage(m_hEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);

    // Set default tab stops (4 spaces approx ~ 16 dialog units)
    DWORD tabStops = 16;
    SendMessage(m_hEdit, EM_SETTABSTOPS, 1, reinterpret_cast<LPARAM>(&tabStops));

    ApplyTheme(m_darkMode);
    SetWordWrap(m_wordWrap);

    return true;
}

void EditorTab::SetFilePath(const std::wstring& path) {
    m_filePath = path;
    if (path.empty()) {
        m_fileName = L"Untitled";
    } else {
        std::filesystem::path fsPath(path);
        m_fileName = fsPath.filename().wstring();
    }
}

bool EditorTab::IsDirty() const {
    if (m_hEdit) {
        return SendMessage(m_hEdit, EM_GETMODIFY, 0, 0) != 0;
    }
    return m_isDirty;
}

void EditorTab::SetDirty(bool dirty) {
    m_isDirty = dirty;
    if (m_hEdit) {
        SendMessage(m_hEdit, EM_SETMODIFY, dirty ? TRUE : FALSE, 0);
    }
}

bool EditorTab::OpenFile(const std::wstring& path) {
    LoadedDocument doc;
    if (!FileIO::Load(path, doc)) {
        return false;
    }

    SetFilePath(path);
    m_encoding = doc.encoding;
    m_lineEnding = doc.lineEnding;

    // Stream text into RichEdit control
    StreamInCookie cookie = { doc.text.data(), doc.text.size(), 0 };
    EDITSTREAM es = {};
    es.dwCookie = reinterpret_cast<DWORD_PTR>(&cookie);
    es.pfnCallback = StreamInCallback;

    SendMessage(m_hEdit, EM_STREAMIN, SF_UNICODE | SF_TEXT, reinterpret_cast<LPARAM>(&es));

    // Reset dirty state & clear undo
    SendMessage(m_hEdit, EM_SETMODIFY, FALSE, 0);
    SendMessage(m_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);

    // Reset caret to beginning
    CHARRANGE cr = { 0, 0 };
    SendMessage(m_hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));

    ApplyTheme(m_darkMode);
    return true;
}

bool EditorTab::SaveFile(const std::wstring& path) {
    std::wstring text;
    StreamOutCookie cookie = { &text };
    EDITSTREAM es = {};
    es.dwCookie = reinterpret_cast<DWORD_PTR>(&cookie);
    es.pfnCallback = StreamOutCallback;

    SendMessage(m_hEdit, EM_STREAMOUT, SF_UNICODE | SF_TEXT, reinterpret_cast<LPARAM>(&es));

    if (!FileIO::Save(path, text, m_encoding, m_lineEnding)) {
        return false;
    }

    SetFilePath(path);
    SetDirty(false);
    return true;
}

bool EditorTab::Save() {
    if (m_filePath.empty()) {
        return false;
    }
    return SaveFile(m_filePath);
}

void EditorTab::ApplyTheme(bool darkMode) {
    m_darkMode = darkMode;
    if (!m_hEdit) return;

    COLORREF bgColor = darkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
    COLORREF fgColor = darkMode ? RGB(220, 220, 220) : RGB(0, 0, 0);

    SendMessage(m_hEdit, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(bgColor));

    CHARFORMAT2W cf = { sizeof(CHARFORMAT2W) };
    cf.dwMask = CFM_COLOR;
    cf.dwEffects = 0; // Crucial: clear CFE_AUTOCOLOR so crTextColor is always honored
    cf.crTextColor = fgColor;

    // 1. Set default format for newly typed/inserted characters
    SendMessage(m_hEdit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

    // 2. Set format for current selection / caret insertion point
    SendMessage(m_hEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

    // 3. Set format for all existing text in buffer
    SendMessage(m_hEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

    // Refresh display
    InvalidateRect(m_hEdit, nullptr, TRUE);
}

void EditorTab::SetFont(HFONT hFont) {
    m_hFont = hFont;
    if (m_hEdit) {
        SendMessage(m_hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        ApplyTheme(m_darkMode);
    }
}

void EditorTab::SetWordWrap(bool wrap) {
    m_wordWrap = wrap;
    if (m_hEdit) {
        // In RichEdit, EM_SETTARGETDEVICE with NULL, 0 = wrap to window; NULL, 1 = no wrap
        SendMessage(m_hEdit, EM_SETTARGETDEVICE, 0, wrap ? 0 : 1);
    }
}

void EditorTab::SetZoom(int percent) {
    if (percent < 10) percent = 10;
    if (percent > 500) percent = 500;
    m_zoomPercent = percent;
    if (m_hEdit) {
        SendMessage(m_hEdit, EM_SETZOOM, static_cast<WPARAM>(m_zoomPercent), 100);
    }
}

void EditorTab::GetCaretInfo(int& outLine, int& outCol, int& outCharCount, int& outSelCount) {
    outLine = 1;
    outCol = 1;
    outCharCount = 0;
    outSelCount = 0;

    if (!m_hEdit) return;

    CHARRANGE cr = {};
    SendMessage(m_hEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));
    outSelCount = std::abs(cr.cpMax - cr.cpMin);

    LRESULT lineIndex = SendMessage(m_hEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
    outLine = static_cast<int>(lineIndex) + 1;

    LRESULT lineStartChar = SendMessage(m_hEdit, EM_LINEINDEX, lineIndex, 0);
    outCol = static_cast<int>(cr.cpMin - lineStartChar) + 1;

    GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
    outCharCount = static_cast<int>(SendMessage(m_hEdit, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gtl), 0));
}

void EditorTab::Show(bool show) {
    if (m_hEdit) {
        ShowWindow(m_hEdit, show ? SW_SHOW : SW_HIDE);
    }
}

void EditorTab::SetBounds(int x, int y, int width, int height) {
    if (m_hEdit) {
        MoveWindow(m_hEdit, x, y, width, height, TRUE);
    }
}

void EditorTab::SetFocus() {
    if (m_hEdit) {
        ::SetFocus(m_hEdit);
    }
}

void EditorTab::Undo()      { if (m_hEdit) SendMessage(m_hEdit, WM_UNDO, 0, 0); }
void EditorTab::Redo()      { if (m_hEdit) SendMessage(m_hEdit, EM_REDO, 0, 0); }
void EditorTab::Cut()       { if (m_hEdit) SendMessage(m_hEdit, WM_CUT, 0, 0); }
void EditorTab::Copy()      { if (m_hEdit) SendMessage(m_hEdit, WM_COPY, 0, 0); }
void EditorTab::Paste()     { if (m_hEdit) SendMessage(m_hEdit, WM_PASTE, 0, 0); }
void EditorTab::Delete()    { if (m_hEdit) SendMessage(m_hEdit, WM_CLEAR, 0, 0); }
void EditorTab::SelectAll() {
    if (m_hEdit) {
        CHARRANGE cr = { 0, -1 };
        SendMessage(m_hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));
    }
}
void EditorTab::InsertText(const std::wstring& text) {
    if (m_hEdit) {
        SendMessage(m_hEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    }
}
