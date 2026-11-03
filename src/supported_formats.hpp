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

inline constexpr const wchar_t* kAllExtensions[] = {
    // Plain Text & Logs
    L".txt", L".text", L".log", L".out", L".err",
    // Markdown & Documentation
    L".md", L".markdown",
    // Configuration & Settings
    L".ini", L".cfg", L".conf", L".config", L".inf", L".properties", L".env",
    // Structured Data & Serialization
    L".json", L".xml", L".yaml", L".yml", L".csv", L".tsv", L".toml",
    // Source Code & Scripts
    L".c", L".cpp", L".h", L".hpp", L".cs", L".js", L".ts", L".py",
    L".html", L".htm", L".css", L".sql", L".sh", L".bat", L".cmd", L".ps1"
};

// Builds a semicolon-separated wildcard pattern such as "*.txt;*.text;*.log"
inline std::wstring BuildPattern(const wchar_t* const* exts, size_t count) {
    std::wstring out;
    for (size_t i = 0; i < count; ++i) {
        if (!out.empty()) out += L';';
        out += L'*';
        out += exts[i];
    }
    return out;
}

template <size_t N>
inline std::wstring BuildPattern(const wchar_t* const (&exts)[N]) {
    return BuildPattern(exts, N);
}

// Returns all supported extensions across all categories
inline std::vector<std::wstring> GetAllExtensions() {
    std::vector<std::wstring> exts;
    exts.reserve(std::size(kAllExtensions));
    for (const wchar_t* ext : kAllExtensions) {
        exts.emplace_back(ext);
    }
    return exts;
}

// Builds an all-inclusive wildcard pattern covering EVERY supported extension
inline std::wstring BuildAllSupportedPattern() {
    return BuildPattern(kAllExtensions, std::size(kAllExtensions));
}

// Checks if an extension or file path is in the supported formats list
inline bool IsSupported(const std::wstring& extOrPath) {
    size_t dot = extOrPath.find_last_of(L'.');
    size_t slash = extOrPath.find_last_of(L"\\/");
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return false;
    }
    std::wstring ext = extOrPath.substr(dot);
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    for (const wchar_t* e : kAllExtensions) {
        if (ext == e) return true;
    }
    return false;
}

inline std::wstring BuildFilterString(bool includeAllSupported) {
    const std::wstring textPattern = BuildPattern(kText);
    const std::wstring mdPattern = BuildPattern(kMarkdown);
    const std::wstring configPattern = BuildPattern(kConfig);
    const std::wstring dataPattern = BuildPattern(kData);
    const std::wstring codePattern = BuildPattern(kCode);

    std::wstring filter;
    auto addFilter = [&filter](const std::wstring& desc, const std::wstring& pattern) {
        filter += desc;
        filter.push_back(L'\0');
        filter += pattern;
        filter.push_back(L'\0');
    };

    if (includeAllSupported) {
        const std::wstring allSupported = BuildAllSupportedPattern();
        addFilter(L"All Supported Documents", allSupported);
    }

    addFilter(L"Text Documents (" + textPattern + L")", textPattern);
    addFilter(L"Markdown Documents (" + mdPattern + L")", mdPattern);
    addFilter(L"Configuration Files (" + configPattern + L")", configPattern);
    addFilter(L"Structured Data (" + dataPattern + L")", dataPattern);
    addFilter(L"Source Code & Scripts (" + codePattern + L")", codePattern);
    addFilter(L"All Files (*.*)", L"*.*");
    filter.push_back(L'\0');
    return filter;
}

} // namespace SupportedFormats
