#include <iostream>
#include <string>
#include <cassert>
#include <windows.h>
#include "../src/editor_context_menu_helper.hpp"

void TestMenuFlags() {
    std::cout << "[Test] Editor Context Menu Flags: ";

    // Case 1: Empty document, no selection, empty clipboard
    {
        auto f = EditorContextMenuHelper::ComputeMenuFlags(false, false, false, false, 0, false);
        assert(f.canUndo == false);
        assert(f.canRedo == false);
        assert(f.canCut == false);
        assert(f.canCopy == false);
        assert(f.canPaste == false);
        assert(f.canDelete == false);
        assert(f.canSelectAll == false);
        assert(f.canSearchWeb == false);
        assert(f.isWordWrap == false);
    }

    // Case 2: Document with text, no selection, clipboard has text, can undo, word wrap true
    {
        auto f = EditorContextMenuHelper::ComputeMenuFlags(true, false, false, true, 150, true);
        assert(f.canUndo == true);
        assert(f.canRedo == false);
        assert(f.canCut == false);
        assert(f.canCopy == false);
        assert(f.canPaste == true);
        assert(f.canDelete == false);
        assert(f.canSelectAll == true);
        assert(f.canSearchWeb == false);
        assert(f.isWordWrap == true);
    }

    // Case 3: Document with active selection, empty clipboard, can redo
    {
        auto f = EditorContextMenuHelper::ComputeMenuFlags(false, true, true, false, 150, false);
        assert(f.canUndo == false);
        assert(f.canRedo == true);
        assert(f.canCut == true);
        assert(f.canCopy == true);
        assert(f.canPaste == false);
        assert(f.canDelete == true);
        assert(f.canSelectAll == true);
        assert(f.canSearchWeb == true);
        assert(f.isWordWrap == false);
    }

    std::cout << "PASSED" << std::endl;
}

void TestSearchWebLabel() {
    std::cout << "[Test] Search Web Label Formatting: ";

    // Empty query
    assert(EditorContextMenuHelper::FormatSearchWebLabel(L"") == L"Search Web");

    // All whitespace
    assert(EditorContextMenuHelper::FormatSearchWebLabel(L"   \r\n\t  ") == L"Search Web");

    // Simple single word
    assert(EditorContextMenuHelper::FormatSearchWebLabel(L"error404") == L"Search Web for \"error404\"");

    // Multiple words with internal tabs and newlines
    assert(EditorContextMenuHelper::FormatSearchWebLabel(L" \t hello \r\n world \t ") == L"Search Web for \"hello world\"");

    // Long text exceeding 28 characters
    std::wstring longText = L"This is a long search query that exceeds the maximum limit";
    std::wstring label = EditorContextMenuHelper::FormatSearchWebLabel(longText, 28);
    assert(label == L"Search Web for \"This is a long search query ...\"");

    std::cout << "PASSED" << std::endl;
}

void TestSearchWebUrl() {
    std::cout << "[Test] Search Web URL Generation: ";

    // Empty query
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"") == L"https://www.google.com");
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"   \t\r\n  ") == L"https://www.google.com");

    // Alphanumeric query
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"LightNotePad") == L"https://www.google.com/search?q=LightNotePad");

    // Query with spaces
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"hello world") == L"https://www.google.com/search?q=hello+world");

    // Leading and trailing spaces trimmed
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"   foo bar   ") == L"https://www.google.com/search?q=foo+bar");

    // Symbols encoding
    assert(EditorContextMenuHelper::BuildSearchWebUrl(L"C++ & Win32") == L"https://www.google.com/search?q=C%2B%2B+%26+Win32");

    // Unicode / UTF-8 encoding (Café -> C a f %C3%A9)
    auto resUrl = EditorContextMenuHelper::BuildSearchWebUrl(L"Caf\u00E9");
    if (resUrl != L"https://www.google.com/search?q=Caf%C3%A9") {
        std::wcout << L"Actual resUrl: " << resUrl << std::endl;
    }
    assert(resUrl == L"https://www.google.com/search?q=Caf%C3%A9");

    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  LightNotePad Editor Context Menu Unit Tests             " << std::endl;
    std::cout << "==========================================================" << std::endl;

    TestMenuFlags();
    TestSearchWebLabel();
    TestSearchWebUrl();

    std::cout << "All Editor Context Menu unit tests completed successfully!" << std::endl;
    return 0;
}
