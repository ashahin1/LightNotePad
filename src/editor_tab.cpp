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
    if (m_pTextDoc) {
        m_pTextDoc->Release();
        m_pTextDoc = nullptr;
    }
    if (m_hEdit) {
        RemoveWindowSubclass(m_hEdit, EditSubclassProc, 1);
        DestroyWindow(m_hEdit);
        m_hEdit = nullptr;
    }
}

LRESULT CALLBACK EditorTab::EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto* pTab = reinterpret_cast<EditorTab*>(dwRefData);
    if (!pTab) return DefSubclassProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_PASTE: {
        if (pTab->m_language != SyntaxLanguage::None && pTab->m_syntaxEnabled) {
            SendMessageW(hWnd, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (ctrl && !shift && (wParam == 'V' || wParam == 'v')) {
            if (pTab->m_language != SyntaxLanguage::None && pTab->m_syntaxEnabled) {
                SendMessageW(hWnd, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
                return 0;
            }
        }
        if (shift && !ctrl && wParam == VK_INSERT) {
            if (pTab->m_language != SyntaxLanguage::None && pTab->m_syntaxEnabled) {
                SendMessageW(hWnd, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
                return 0;
            }
        }

        // Suppress RichEdit rich-formatting hotkeys
        if (ctrl && !shift) {
            switch (wParam) {
            case 'B': case 'b': // Bold
            case 'I': case 'i': // Italic
            case 'U': case 'u': // Underline
            case 'E': case 'e': // Center
            case 'L': case 'l': // Align left
            case 'R': case 'r': // Align right
            case 'J': case 'j': // Justify
                return 0;
            }
        }

        LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (pTab->m_language != SyntaxLanguage::None && pTab->m_syntaxEnabled) {
            if (wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_UP || wParam == VK_DOWN || wParam == VK_HOME || wParam == VK_END) {
                pTab->ColorizeViewport();
            }
        }
        return res;
    }
    case WM_VSCROLL:
    case WM_MOUSEWHEEL: {
        LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (pTab->m_language != SyntaxLanguage::None && pTab->m_syntaxEnabled) {
            pTab->ColorizeViewport();
        }
        return res;
    }
    case WM_NCDESTROY: {
        RemoveWindowSubclass(hWnd, EditSubclassProc, uIdSubclass);
        break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
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

    // Install subclass for paste sanitization and scroll-triggered syntax recoloring
    SetWindowSubclass(m_hEdit, EditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

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

    ConfigureSyntaxMode();

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
    ConfigureSyntaxMode();
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

    // Reset caret to beginning
    CHARRANGE cr = { 0, 0 };
    SendMessage(m_hEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));

    ApplyTheme(m_darkMode);
    ConfigureSyntaxMode();

    // Reset dirty state & clear undo AFTER theme and syntax configuration
    SendMessage(m_hEdit, EM_SETMODIFY, FALSE, 0);
    SendMessage(m_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);

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
    ConfigureSyntaxMode();
    SetDirty(false);
    SendMessage(m_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
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

    BOOL wasModified = (SendMessageW(m_hEdit, EM_GETMODIFY, 0, 0) != 0);

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

    if (m_language != SyntaxLanguage::None && m_syntaxEnabled) {
        ColorizeViewport();
    }

    if (!wasModified) {
        SendMessage(m_hEdit, EM_SETMODIFY, FALSE, 0);
        SendMessage(m_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
    }
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
void EditorTab::Paste() {
    if (m_hEdit) {
        if (m_language != SyntaxLanguage::None && m_syntaxEnabled) {
            SendMessageW(m_hEdit, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
        } else {
            SendMessageW(m_hEdit, WM_PASTE, 0, 0);
        }
    }
}
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

void EditorTab::ConfigureSyntaxMode() {
    if (!m_hEdit) return;

    SyntaxLanguage newLang = m_syntaxEnabled ? SyntaxHighlighter::DetectLanguage(m_filePath) : SyntaxLanguage::None;
    m_language = newLang;

    if (m_language == SyntaxLanguage::None) {
        SendMessageW(m_hEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        ApplyTheme(m_darkMode);
    } else {
        // Large file safety check (> 1 MB = 1,048,576 chars)
        GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
        LRESULT charCount = SendMessageW(m_hEdit, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gtl), 0);
        if (charCount > 1024 * 1024) {
            m_language = SyntaxLanguage::None;
            SendMessageW(m_hEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
            ApplyTheme(m_darkMode);
            return;
        }

        SendMessageW(m_hEdit, EM_SETTEXTMODE, TM_RICHTEXT, 0);
        if (!m_pTextDoc) {
            IUnknown* pUnk = nullptr;
            if (SendMessageW(m_hEdit, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&pUnk)) && pUnk) {
                pUnk->QueryInterface(__uuidof(ITextDocument), reinterpret_cast<void**>(&m_pTextDoc));
                pUnk->Release();
            }
        }
        ColorizeViewport();
    }
}

void EditorTab::SetSyntaxEnabled(bool enabled) {
    if (m_syntaxEnabled != enabled) {
        m_syntaxEnabled = enabled;
        BOOL wasModified = (SendMessageW(m_hEdit, EM_GETMODIFY, 0, 0) != 0);
        ConfigureSyntaxMode();
        if (!wasModified) {
            SendMessage(m_hEdit, EM_SETMODIFY, FALSE, 0);
            SendMessage(m_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
        }
    }
}

void EditorTab::ColorizeViewport() {
    if (!m_hEdit || !m_pTextDoc || m_language == SyntaxLanguage::None || !m_syntaxEnabled) return;

    LRESULT firstLine = SendMessageW(m_hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    RECT rc = {};
    GetClientRect(m_hEdit, &rc);

    POINT pt = { rc.left + 5, rc.bottom - 5 };
    LRESULT charPos = SendMessageW(m_hEdit, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&pt));
    LRESULT lastLine = SendMessageW(m_hEdit, EM_EXLINEFROMCHAR, 0, charPos);
    if (lastLine < firstLine) {
        lastLine = firstLine + 50;
    }

    long startLine = std::max(0L, static_cast<long>(firstLine) - 50);
    long endLine = static_cast<long>(lastLine) + 100;

    ColorizeRange(startLine, endLine);
}

void EditorTab::ColorizeRange(long startLine, long endLine) {
    if (!m_hEdit || !m_pTextDoc || m_language == SyntaxLanguage::None || !m_syntaxEnabled) return;
    if (m_isColorizing) return;

    LRESULT totalLines = SendMessageW(m_hEdit, EM_GETLINECOUNT, 0, 0);
    if (startLine < 0) startLine = 0;
    if (endLine >= totalLines) endLine = static_cast<long>(totalLines - 1);
    if (startLine > endLine) return;

    LRESULT startCp = SendMessageW(m_hEdit, EM_LINEINDEX, startLine, 0);
    if (startCp < 0) return;

    LRESULT endLineStart = SendMessageW(m_hEdit, EM_LINEINDEX, endLine, 0);
    LRESULT endLineLen = SendMessageW(m_hEdit, EM_LINELENGTH, endLineStart, 0);
    LRESULT endCp = endLineStart + endLineLen;
    if (endCp <= startCp) return;

    size_t rangeLen = static_cast<size_t>(endCp - startCp);
    std::vector<wchar_t> textBuf(rangeLen + 1, L'\0');
    TEXTRANGEW tr = {};
    tr.chrg.cpMin = static_cast<LONG>(startCp);
    tr.chrg.cpMax = static_cast<LONG>(endCp);
    tr.lpstrText = textBuf.data();
    LRESULT copied = SendMessageW(m_hEdit, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
    if (copied <= 0) return;

    std::wstring_view text(textBuf.data(), static_cast<size_t>(copied));
    std::vector<TokenSpan> spans;
    bool endedInComment = false;
    SyntaxHighlighter::Tokenize(m_language, text, static_cast<long>(startCp), false, spans, endedInComment);

    m_isColorizing = true;
    LRESULT oldMask = SendMessageW(m_hEdit, EM_SETEVENTMASK, 0, 0);
    BOOL wasModified = (SendMessageW(m_hEdit, EM_GETMODIFY, 0, 0) != 0);

    long freezeCount = 0;
    m_pTextDoc->Freeze(&freezeCount);
    m_pTextDoc->Undo(tomSuspend, nullptr);

    ITextRange* pRange = nullptr;
    if (SUCCEEDED(m_pTextDoc->Range(static_cast<long>(startCp), static_cast<long>(endCp), &pRange)) && pRange) {
        ITextFont* pFont = nullptr;
        if (SUCCEEDED(pRange->GetFont(&pFont)) && pFont) {
            COLORREF defaultColor = SyntaxHighlighter::GetTokenColor(SyntaxTokenType::Default, m_darkMode);
            pFont->SetForeColor(defaultColor);

            for (const auto& span : spans) {
                if (span.type == SyntaxTokenType::Default) continue;
                COLORREF tokenColor = SyntaxHighlighter::GetTokenColor(span.type, m_darkMode);
                pRange->SetRange(span.startCp, span.endCp);
                pFont->SetForeColor(tokenColor);
            }
            pFont->Release();
        }
        pRange->Release();
    }

    m_pTextDoc->Undo(tomResume, nullptr);
    m_pTextDoc->Unfreeze(&freezeCount);

    InvalidateRect(m_hEdit, nullptr, FALSE);

    SendMessageW(m_hEdit, EM_SETEVENTMASK, 0, oldMask);
    if (!wasModified) {
        SendMessageW(m_hEdit, EM_SETMODIFY, FALSE, 0);
    }

    m_isColorizing = false;
}

void EditorTab::OnTextChanged() {
    if (m_isColorizing) return;
    if (m_language != SyntaxLanguage::None && m_syntaxEnabled) {
        CHARRANGE cr = {};
        SendMessageW(m_hEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));
        LRESULT activeLine = SendMessageW(m_hEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
        ColorizeRange(static_cast<long>(activeLine), static_cast<long>(activeLine));
    }
}
