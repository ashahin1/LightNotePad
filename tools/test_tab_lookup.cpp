#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <windows.h>
#include "../src/path_lookup_helper.hpp"

// Lightweight mock tab representing an editor tab in LightNotePad
struct MockEditorTab {
    int id = 0;
    std::wstring filePath;
    bool isDirty = false;

    const std::wstring& GetFilePath() const { return filePath; }
    int GetTabId() const { return id; }
    bool IsDirty() const { return isDirty; }
};

class MockTabManager {
public:
    std::vector<std::unique_ptr<MockEditorTab>> tabs;
    int activeTabId = -1;
    int nextId = 1;

    MockEditorTab* CreateTab(const std::wstring& path = L"") {
        auto t = std::make_unique<MockEditorTab>();
        t->id = nextId++;
        t->filePath = path;
        t->isDirty = false;
        MockEditorTab* p = t.get();
        tabs.push_back(std::move(t));
        activeTabId = p->id;
        return p;
    }

    MockEditorTab* GetActiveTab() {
        for (auto& t : tabs) {
            if (t->id == activeTabId) return t.get();
        }
        return nullptr;
    }

    MockEditorTab* FindTabByPath(const std::wstring& filePath) {
        if (filePath.empty()) return nullptr;

        for (auto& tab : tabs) {
            if (tab->GetFilePath().empty()) continue;
            if (PathLookupHelper::ArePathsEquivalent(filePath, tab->GetFilePath())) {
                return tab.get();
            }
        }
        return nullptr;
    }

    MockEditorTab* OpenOrActivateFile(const std::wstring& filePath) {
        if (filePath.empty()) return nullptr;

        // 1. Existing tab match
        MockEditorTab* existing = FindTabByPath(filePath);
        if (existing) {
            activeTabId = existing->id;
            return existing;
        }

        // 2. Reuse clean untitled tab
        MockEditorTab* active = GetActiveTab();
        if (active && active->filePath.empty() && !active->isDirty) {
            active->filePath = filePath;
            return active;
        }

        // 3. Create new tab
        return CreateTab(filePath);
    }
};

void TestNormalizePath() {
    std::cout << "[Test] NormalizePath Path Resolution: ";

    // Empty path
    assert(PathLookupHelper::NormalizePath(L"").empty());

    // Absolute path normalization
    std::wstring pathSlash = L"C:/Windows/System32/drivers";
    std::wstring norm = PathLookupHelper::NormalizePath(pathSlash);
    assert(norm.find(L'/') == std::wstring::npos);
    assert(norm.find(L"\\Windows\\System32\\drivers") != std::wstring::npos);

    // Relative path expansion (must produce a full drive path)
    std::wstring rel = L".\\sample.txt";
    std::wstring normRel = PathLookupHelper::NormalizePath(rel);
    assert(normRel.length() > rel.length());
    assert(normRel.substr(normRel.length() - 10) == L"sample.txt");

    std::cout << "PASSED" << std::endl;
}

void TestPathEquivalence() {
    std::cout << "[Test] Path Equivalence (Case & Slash Independence): ";

    // Case insensitivity
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\Docs\\Notes.txt", L"c:\\docs\\notes.txt") == true);
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\DOCS\\NOTES.TXT", L"C:\\docs\\notes.txt") == true);

    // Slash independence
    assert(PathLookupHelper::ArePathsEquivalent(L"C:/Docs/Notes.txt", L"C:\\Docs\\Notes.txt") == true);
    assert(PathLookupHelper::ArePathsEquivalent(L"C:/Docs//Notes.txt", L"C:\\Docs\\Notes.txt") == true);

    // Redundant dot resolution
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\Docs\\.\\Notes.txt", L"C:\\Docs\\Notes.txt") == true);
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\Docs\\Sub\\..\\Notes.txt", L"C:\\Docs\\Notes.txt") == true);

    // Different files must NOT be equivalent
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\Docs\\Notes1.txt", L"C:\\Docs\\Notes2.txt") == false);
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\FolderA\\File.txt", L"C:\\FolderB\\File.txt") == false);

    // Empty paths
    assert(PathLookupHelper::ArePathsEquivalent(L"", L"") == false);
    assert(PathLookupHelper::ArePathsEquivalent(L"", L"C:\\Docs\\Notes.txt") == false);
    assert(PathLookupHelper::ArePathsEquivalent(L"C:\\Docs\\Notes.txt", L"") == false);

    std::cout << "PASSED" << std::endl;
}

void TestPhysicalFileEquivalence() {
    std::cout << "[Test] Physical File Equivalence & Hard Links: ";

    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);

    wchar_t tempFile1[MAX_PATH];
    wchar_t tempFile2[MAX_PATH];
    wchar_t tempLink[MAX_PATH];

    GetTempFileNameW(tempDir, L"LNP", 0, tempFile1);
    GetTempFileNameW(tempDir, L"LNP", 0, tempFile2);

    // Write a byte to tempFile1
    HANDLE h1 = CreateFileW(tempFile1, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h1 != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h1, "A", 1, &written, nullptr);
        CloseHandle(h1);
    }

    // Write a byte to tempFile2
    HANDLE h2 = CreateFileW(tempFile2, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h2 != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h2, "B", 1, &written, nullptr);
        CloseHandle(h2);
    }

    // Create hard link to tempFile1
    lstrcpyW(tempLink, tempFile1);
    lstrcatW(tempLink, L"_link.tmp");
    DeleteFileW(tempLink);
    BOOL linkCreated = CreateHardLinkW(tempLink, tempFile1, nullptr);

    // 1. Same file path matches
    assert(PathLookupHelper::ArePathsEquivalent(tempFile1, tempFile1) == true);

    // 2. Different files do not match
    assert(PathLookupHelper::ArePathsEquivalent(tempFile1, tempFile2) == false);

    // 3. Hard link to the same file matches via volume serial & file index
    if (linkCreated) {
        assert(PathLookupHelper::ArePathsEquivalent(tempFile1, tempLink) == true);
        DeleteFileW(tempLink);
    }

    DeleteFileW(tempFile1);
    DeleteFileW(tempFile2);

    std::cout << "PASSED" << std::endl;
}

void TestTabDeduplication() {
    std::cout << "[Test] Tab Deduplication & Untitled Tab Reuse: ";

    MockTabManager mgr;

    // 1. Starts with a single untitled clean tab
    MockEditorTab* initialTab = mgr.CreateTab(L"");
    assert(mgr.tabs.size() == 1);
    assert(initialTab->id == 1);
    assert(initialTab->filePath.empty());

    // 2. Opening first file reuses the untitled clean tab
    MockEditorTab* openedTab1 = mgr.OpenOrActivateFile(L"C:\\Docs\\Report.txt");
    assert(openedTab1 == initialTab);
    assert(mgr.tabs.size() == 1);
    assert(openedTab1->filePath == L"C:\\Docs\\Report.txt");

    // 3. Opening the exact same file does NOT duplicate; activates existing tab
    MockEditorTab* reOpenedTab1 = mgr.OpenOrActivateFile(L"C:\\Docs\\Report.txt");
    assert(reOpenedTab1 == openedTab1);
    assert(mgr.tabs.size() == 1);

    // 4. Opening same file with different casing activates existing tab
    MockEditorTab* caseOpenedTab1 = mgr.OpenOrActivateFile(L"c:\\DOCS\\REPORT.TXT");
    assert(caseOpenedTab1 == openedTab1);
    assert(mgr.tabs.size() == 1);

    // 5. Opening same file with forward slashes activates existing tab
    MockEditorTab* slashOpenedTab1 = mgr.OpenOrActivateFile(L"C:/Docs/Report.txt");
    assert(slashOpenedTab1 == openedTab1);
    assert(mgr.tabs.size() == 1);

    // 6. Opening a DIFFERENT file creates Tab 2
    MockEditorTab* openedTab2 = mgr.OpenOrActivateFile(L"C:\\Code\\Main.cpp");
    assert(openedTab2 != openedTab1);
    assert(mgr.tabs.size() == 2);
    assert(openedTab2->id == 2);
    assert(mgr.activeTabId == 2);

    // 7. Opening file 1 again switches active tab back to Tab 1
    MockEditorTab* switchTab1 = mgr.OpenOrActivateFile(L"C:\\Docs\\Report.txt");
    assert(switchTab1 == openedTab1);
    assert(mgr.activeTabId == 1);
    assert(mgr.tabs.size() == 2);

    // 8. Opening file 2 switches active tab back to Tab 2
    MockEditorTab* switchTab2 = mgr.OpenOrActivateFile(L"c:\\code\\main.cpp");
    assert(switchTab2 == openedTab2);
    assert(mgr.activeTabId == 2);
    assert(mgr.tabs.size() == 2);

    // 9. Untitled tab is never falsely matched
    assert(mgr.FindTabByPath(L"") == nullptr);

    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  LightNotePad Tab Lookup & Deduplication Unit Tests       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    TestNormalizePath();
    TestPathEquivalence();
    TestPhysicalFileEquivalence();
    TestTabDeduplication();

    std::cout << "All Tab Lookup & Deduplication tests completed successfully!" << std::endl;
    return 0;
}
