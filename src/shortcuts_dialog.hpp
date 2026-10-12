#pragma once
#include "pch.hpp"

class ShortcutsDialog {
public:
    static void Show(HWND hParent, HINSTANCE hInstance, bool darkMode, UINT dpi);

private:
    static LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnPaint(HWND hWnd, HDC hdc, bool darkMode, UINT dpi, HFONT hFontBold, HFONT hFontReg);
};
