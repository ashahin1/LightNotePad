#pragma once
#include "pch.hpp"
#include "file_io.hpp"
#include "syntax_highlighter.hpp"

class EditorTab {
public:
    EditorTab(HWND hParent, HINSTANCE hInstance, int tabId);
    ~EditorTab();

    bool Create();
    void Destroy();

    HWND GetEditHwnd() const { return m_hEdit; }
    int GetTabId() const { return m_tabId; }

    const std::wstring& GetFilePath() const { return m_filePath; }
    const std::wstring& GetFileName() const { return m_fileName; }
    void SetFilePath(const std::wstring& path);

    bool IsDirty() const;
    void SetDirty(bool dirty);

    FileEncoding GetEncoding() const { return m_encoding; }
    void SetEncoding(FileEncoding enc) { m_encoding = enc; }

    LineEnding GetLineEnding() const { return m_lineEnding; }
    void SetLineEnding(LineEnding le) { m_lineEnding = le; }

    bool OpenFile(const std::wstring& path);
    bool SaveFile(const std::wstring& path);
    bool Save();

    void ApplyTheme(bool darkMode);
    void SetFont(HFONT hFont);
    void SetWordWrap(bool wrap);
    bool IsWordWrap() const { return m_wordWrap; }

    void SetZoom(int percent);
    int GetZoom() const { return m_zoomPercent; }

    void GetCaretInfo(int& outLine, int& outCol, int& outCharCount, int& outSelCount);

    void Show(bool show);
    void SetBounds(int x, int y, int width, int height);
    void SetFocus();

    // Syntax Highlighting
    SyntaxLanguage GetLanguage() const { return m_language; }
    void SetSyntaxEnabled(bool enabled);
    bool IsSyntaxEnabled() const { return m_syntaxEnabled; }
    void ConfigureSyntaxMode();
    void ColorizeViewport();
    void ColorizeRange(long startLine, long endLine);
    void OnTextChanged();

    // Text editing commands
    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    void Delete();
    void SelectAll();
    void InsertText(const std::wstring& text);

private:
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hEdit = nullptr;
    int m_tabId = 0;

    std::wstring m_filePath;
    std::wstring m_fileName = L"Untitled";
    bool m_isDirty = false;
    FileEncoding m_encoding = FileEncoding::UTF8;
    LineEnding m_lineEnding = LineEnding::CRLF;
    bool m_wordWrap = true;
    int m_zoomPercent = 100;
    bool m_darkMode = false;
    HFONT m_hFont = nullptr;

    SyntaxLanguage m_language = SyntaxLanguage::None;
    bool m_syntaxEnabled = true;
    bool m_isColorizing = false;
    ITextDocument* m_pTextDoc = nullptr;
};
