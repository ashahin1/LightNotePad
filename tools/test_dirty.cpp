#include "pch.hpp"
#include <iostream>
#include <cassert>
#include "editor_tab.hpp"
#include "file_io.hpp"

int main() {
    InitCommonControls();
    LoadLibraryW(L"msftedit.dll");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TestDirtyParent";
    RegisterClassW(&wc);

    HWND hParent = CreateWindowW(L"TestDirtyParent", L"Test", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);

    std::wstring testFile = L"test_dirty_sample.cpp";
    std::wstring content = L"// Sample code header\r\n#include <iostream>\r\n";
    for (int i = 0; i < 200; ++i) {
        content += L"int func_" + std::to_wstring(i) + L"() { return " + std::to_wstring(i) + L"; }\r\n";
    }
    FileIO::Save(testFile, content, FileEncoding::UTF8, LineEnding::CRLF);

    EditorTab tab(hParent, wc.hInstance, 1);
    tab.Create();

    // 1. Newly created tab must not be dirty
    assert(!tab.IsDirty());

    // 2. OpenFile on a code file must NOT be dirty
    bool opened = tab.OpenFile(testFile);
    assert(opened);
    std::cout << "1. After OpenFile: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 3. ColorizeViewport (on initial display) must NOT be dirty
    tab.ColorizeViewport();
    std::cout << "2. After initial ColorizeViewport: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 4. Scrolling through file (recolorizing viewports) must NOT mark dirty
    tab.ColorizeRange(30, 80);
    tab.ColorizeRange(70, 120);
    tab.ColorizeRange(110, 160);
    std::cout << "3. After multiple scroll range colorizations: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 5. Toggling Dark Mode must NOT mark dirty
    tab.ApplyTheme(false);
    assert(!tab.IsDirty());
    tab.ApplyTheme(true);
    std::cout << "4. After Dark Mode toggle: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 6. Toggling Syntax Highlighting must NOT mark dirty
    tab.SetSyntaxEnabled(false);
    assert(!tab.IsDirty());
    tab.SetSyntaxEnabled(true);
    std::cout << "5. After Syntax toggle: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 7. Legitimate text edit MUST mark dirty
    tab.InsertText(L"// user typed this\r\n");
    std::cout << "6. After user edit: IsDirty = " << tab.IsDirty() << "\n";
    assert(tab.IsDirty());

    // 8. Saving file must clear dirty
    bool saved = tab.Save();
    assert(saved);
    std::cout << "7. After Save: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    // 9. Scrolling after save must remain not dirty
    tab.ColorizeRange(50, 100);
    std::cout << "8. After scroll after save: IsDirty = " << tab.IsDirty() << "\n";
    assert(!tab.IsDirty());

    std::cout << "\nALL DIRTY STATE VERIFICATION TESTS PASSED (100% SUCCESS)!\n";

    DeleteFileW(testFile.c_str());
    DestroyWindow(hParent);
    return 0;
}
