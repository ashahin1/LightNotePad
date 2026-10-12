#pragma once
#include "pch.hpp"
#include "file_io.hpp"

class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    bool Create(HWND hParent, HINSTANCE hInstance);
    void Destroy();

    HWND GetHwnd() const { return m_hWnd; }

    int GetDesiredHeight(UINT dpi) const;
    void SetBounds(int x, int y, int width, int height);

    void UpdateCaret(int line, int col, int charCount, int selCount);
    void UpdateDocumentInfo(FileEncoding enc, LineEnding le, int zoomPercent);

    void ApplyTheme(bool darkMode);
    void SetDpi(UINT dpi);
    void SetVisible(bool visible);
    bool IsVisible() const { return m_visible; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint(HDC hdc);

    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;

    bool m_visible = true;
    int m_line = 1;
    int m_col = 1;
    int m_charCount = 0;
    int m_selCount = 0;
    FileEncoding m_encoding = FileEncoding::UTF8;
    LineEnding m_lineEnding = LineEnding::CRLF;
    int m_zoomPercent = 100;

    UINT m_dpi = 96;
    bool m_darkMode = false;
    HFONT m_hFont = nullptr;
};
