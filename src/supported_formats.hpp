#pragma once

#include <string>
#include <vector>
#include <cwctype>

// Single source of truth for every file extension LightNotePad can open and register.
// Used by the Open/Save File dialogs, shell (/register) associations, and format validation.
namespace SupportedFormats {

// Plain Text & Logs
inline constexpr const wchar_t* kText[] = {
    L".txt", L".text", L".log", L".out", L".err"
};

// Markdown & Documentation
inline constexpr const wchar_t* kMarkdown[] = {
    L".md", L".markdown"
};

// Configuration & Settings
inline constexpr const wchar_t* kConfig[] = {
    L".ini", L".cfg", L".conf", L".config", L".inf", L".properties", L".env"
};

// Structured Data & Serialization
inline constexpr const wchar_t* kData[] = {
    L".json", L".xml", L".yaml", L".yml", L".csv", L".tsv", L".toml"
};

// Source Code & Scripts
inline constexpr const wchar_t* kCode[] = {
    L".c", L".cpp", L".h", L".hpp", L".cs", L".js", L".ts", L".py",
    L".html", L".htm", L".css", L".sql", L".sh", L".bat", L".cmd", L".ps1"
};

// Builds a semicolon-separated wildcard pattern such as "*.txt;*.text;*.log"
template <size_t N>
inline std::wstring BuildPattern(const wchar_t* const (&exts)[N]) {
    std::wstring out;
    for (size_t i = 0; i < N; ++i) {
        if (!out.empty()) out += L';';
        out += L'*';
        out += exts[i];
    }
    return out;
}

// Returns all supported extensions across all categories
inline std::vector<std::wstring> GetAllExtensions() {
    std::vector<std::wstring> exts;
    auto add = [&](const auto& arr) {
        for (const wchar_t* e : arr) exts.emplace_back(e);
    };
    add(kText);
    add(kMarkdown);
    add(kConfig);
    add(kData);
    add(kCode);
    return exts;
}

// Builds an all-inclusive wildcard pattern covering EVERY supported extension
inline std::wstring BuildAllSupportedPattern() {
    std::wstring out;
    auto exts = GetAllExtensions();
    for (const auto& ext : exts) {
        if (!out.empty()) out += L';';
        out += L'*';
        out += ext;
    }
    return out;
}

// Checks if an extension or file path is in the supported formats list
inline bool IsSupported(const std::wstring& extOrPath) {
    std::wstring ext = extOrPath;
    size_t dotPos = ext.rfind(L'.');
    if (dotPos != std::wstring::npos) {
        ext = ext.substr(dotPos);
    }
    for (auto& ch : ext) ch = static_cast<wchar_t>(towlower(ch));
    auto all = GetAllExtensions();
    for (const auto& e : all) {
        if (ext == e) return true;
    }
    return false;
}

} // namespace SupportedFormats
