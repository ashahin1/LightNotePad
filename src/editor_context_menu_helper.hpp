#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

struct EditorContextMenuFlags {
    bool canUndo = false;
    bool canRedo = false;
    bool canCut = false;
    bool canCopy = false;
    bool canPaste = false;
    bool canDelete = false;
    bool canSelectAll = false;
    bool canSearchWeb = false;
    bool isWordWrap = false;
};

class EditorContextMenuHelper {
public:
    static EditorContextMenuFlags ComputeMenuFlags(
        bool canUndo,
        bool canRedo,
        bool hasSelection,
        bool hasClipboardText,
        size_t textLength,
        bool isWordWrap
    ) {
        EditorContextMenuFlags flags;
        flags.canUndo = canUndo;
        flags.canRedo = canRedo;
        flags.canCut = hasSelection;
        flags.canCopy = hasSelection;
        flags.canPaste = hasClipboardText;
        flags.canDelete = hasSelection;
        flags.canSelectAll = (textLength > 0);
        flags.canSearchWeb = hasSelection;
        flags.isWordWrap = isWordWrap;
        return flags;
    }

    static std::wstring FormatSearchWebLabel(const std::wstring& selectedText, size_t maxLen = 28) {
        if (selectedText.empty()) {
            return L"Search Web";
        }

        std::wstring cleaned;
        cleaned.reserve(selectedText.size());
        bool lastWasSpace = false;

        for (wchar_t ch : selectedText) {
            if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == L' ') {
                if (!lastWasSpace && !cleaned.empty()) {
                    cleaned.push_back(L' ');
                    lastWasSpace = true;
                }
            } else if (ch >= 32) {
                cleaned.push_back(ch);
                lastWasSpace = false;
            }
        }

        while (!cleaned.empty() && cleaned.back() == L' ') {
            cleaned.pop_back();
        }

        if (cleaned.empty()) {
            return L"Search Web";
        }

        if (cleaned.length() > maxLen) {
            cleaned.resize(maxLen);
            cleaned += L"...";
        }

        return L"Search Web for \"" + cleaned + L"\"";
    }

    static std::wstring BuildSearchWebUrl(const std::wstring& query) {
        if (query.empty()) {
            return L"https://www.google.com";
        }

        size_t start = query.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) {
            return L"https://www.google.com";
        }
        size_t end = query.find_last_not_of(L" \t\r\n");
        std::wstring trimmed = query.substr(start, end - start + 1);

        int len = WideCharToMultiByte(CP_UTF8, 0, trimmed.data(), static_cast<int>(trimmed.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return L"https://www.google.com";
        }

        std::string utf8(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, trimmed.data(), static_cast<int>(trimmed.size()), utf8.data(), len, nullptr, nullptr);

        std::wstring encoded;
        encoded.reserve(utf8.size() * 3);
        const wchar_t hexDigits[] = L"0123456789ABCDEF";

        for (unsigned char c : utf8) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                encoded.push_back(static_cast<wchar_t>(c));
            } else if (c == ' ') {
                encoded.push_back(L'+');
            } else {
                encoded.push_back(L'%');
                encoded.push_back(hexDigits[(c >> 4) & 0x0F]);
                encoded.push_back(hexDigits[c & 0x0F]);
            }
        }

        return L"https://www.google.com/search?q=" + encoded;
    }
};
