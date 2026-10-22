#include "pch.hpp"
#include "syntax_highlighter.hpp"

namespace {

// C / C++ keywords (strictly sorted alphabetically for std::binary_search)
constexpr std::wstring_view kCppKeywords[] = {
    L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"auto",
    L"bitand", L"bitor", L"bool", L"break", L"case", L"catch",
    L"char", L"char16_t", L"char32_t", L"char8_t", L"class", L"compl",
    L"concept", L"const", L"consteval", L"constexpr", L"constinit", L"const_cast",
    L"continue", L"co_await", L"co_return", L"co_yield", L"decltype", L"default",
    L"delete", L"do", L"double", L"dynamic_cast", L"else", L"enum",
    L"explicit", L"export", L"extern", L"false", L"final", L"float",
    L"for", L"friend", L"goto", L"if", L"inline", L"int",
    L"long", L"mutable", L"namespace", L"new", L"noexcept", L"not",
    L"not_eq", L"nullptr", L"operator", L"or", L"or_eq", L"override",
    L"private", L"protected", L"public", L"reinterpret_cast", L"requires", L"return",
    L"short", L"signed", L"sizeof", L"static", L"static_assert", L"static_cast",
    L"struct", L"switch", L"template", L"this", L"thread_local", L"throw",
    L"true", L"try", L"typedef", L"typeid", L"typename", L"union",
    L"unsigned", L"using", L"virtual", L"void", L"volatile", L"wchar_t",
    L"while", L"xor", L"xor_eq"
};

constexpr std::wstring_view kCppTypes[] = {
    L"DWORD", L"HANDLE", L"HDC", L"HFONT", L"HKEY", L"HINSTANCE",
    L"HMODULE", L"HWND", L"INT", L"LONG", L"LPARAM", L"LRESULT",
    L"SIZE_T", L"UINT", L"ULONG", L"VOID", L"WCHAR", L"WPARAM",
    L"int16_t", L"int32_t", L"int64_t", L"int8_t", L"intptr_t", L"size_t",
    L"std", L"string", L"string_view", L"uint16_t", L"uint32_t", L"uint64_t",
    L"uint8_t", L"uintptr_t", L"unique_ptr", L"vector", L"wstring", L"wstring_view"
};

// Python keywords (sorted)
constexpr std::wstring_view kPythonKeywords[] = {
    L"False", L"None", L"True", L"and", L"as", L"assert",
    L"async", L"await", L"break", L"class", L"continue", L"def",
    L"del", L"elif", L"else", L"except", L"finally", L"for",
    L"from", L"global", L"if", L"import", L"in", L"is",
    L"lambda", L"nonlocal", L"not", L"or", L"pass", L"raise",
    L"return", L"try", L"while", L"with", L"yield"
};

constexpr std::wstring_view kPythonTypes[] = {
    L"bool", L"bytearray", L"bytes", L"dict", L"float", L"int",
    L"list", L"object", L"self", L"set", L"str", L"tuple"
};

// JS / TS keywords (sorted)
constexpr std::wstring_view kJsKeywords[] = {
    L"async", L"await", L"break", L"case", L"catch", L"class",
    L"const", L"continue", L"debugger", L"default", L"delete", L"do",
    L"else", L"export", L"extends", L"false", L"finally", L"for",
    L"function", L"if", L"import", L"in", L"instanceof", L"let",
    L"new", L"null", L"return", L"super", L"switch", L"this",
    L"throw", L"true", L"try", L"typeof", L"undefined", L"var",
    L"void", L"while", L"with", L"yield"
};

constexpr std::wstring_view kJsTypes[] = {
    L"Array", L"Boolean", L"Function", L"Number", L"Object", L"Promise",
    L"String", L"Symbol", L"any", L"boolean", L"never", L"number",
    L"string", L"unknown", L"void"
};

// JSON keywords (sorted)
constexpr std::wstring_view kJsonKeywords[] = {
    L"false", L"null", L"true"
};

// Script / SQL / Batch keywords (sorted uppercase for case-insensitive binary search)
constexpr std::wstring_view kScriptKeywords[] = {
    L"ADD", L"ALTER", L"AND", L"AS", L"ASC", L"BEGIN",
    L"BY", L"CALL", L"CASE", L"COMMIT", L"CREATE", L"DELETE",
    L"DESC", L"DO", L"DROP", L"ECHO", L"ELSE", L"ELSEIF",
    L"END", L"ERRORACTIONPREFERENCE", L"EXEC", L"EXIT", L"FOR", L"FOREACH",
    L"FROM", L"FUNCTION", L"GOTO", L"GROUP", L"HAVING", L"IF",
    L"IN", L"INDEX", L"INSERT", L"INTO", L"IS", L"JOIN",
    L"LEFT", L"LIKE", L"NOT", L"NULL", L"ON", L"OR",
    L"ORDER", L"PARAM", L"PAUSE", L"PROCESS", L"REM", L"RETURN",
    L"RIGHT", L"ROLLBACK", L"SELECT", L"SET", L"TABLE", L"THEN",
    L"TO", L"TOP", L"TRUNCATE", L"UNION", L"UPDATE", L"VALUES",
    L"VIEW", L"WHERE", L"WHILE"
};

template <size_t N>
bool IsWordInTable(const std::wstring_view (&table)[N], std::wstring_view word) {
    return std::binary_search(table, table + N, word);
}

bool IsScriptKeyword(std::wstring_view word) {
    wchar_t upper[64] = {};
    if (word.length() >= _countof(upper)) return false;
    for (size_t i = 0; i < word.length(); ++i) {
        upper[i] = static_cast<wchar_t>(towupper(word[i]));
    }
    return std::binary_search(kScriptKeywords, kScriptKeywords + _countof(kScriptKeywords), std::wstring_view(upper, word.length()));
}

inline bool IsIdentStart(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || c == L'_';
}

inline bool IsIdentChar(wchar_t c) {
    return IsIdentStart(c) || (c >= L'0' && c <= L'9') || c == L'-' || c == L'$';
}

inline bool IsDigit(wchar_t c) {
    return c >= L'0' && c <= L'9';
}

inline size_t FindEndOfLine(std::wstring_view text, size_t pos) {
    for (size_t i = pos; i < text.size(); ++i) {
        if (text[i] == L'\r' || text[i] == L'\n') {
            return i;
        }
    }
    return std::wstring_view::npos;
}

inline bool IsAtLineStart(std::wstring_view text, size_t i) {
    return (i == 0 || text[i - 1] == L'\r' || text[i - 1] == L'\n');
}

} // namespace

SyntaxLanguage SyntaxHighlighter::DetectLanguage(const std::wstring& filePath) {
    if (filePath.empty()) return SyntaxLanguage::None;

    std::filesystem::path p(filePath);
    std::wstring ext = p.extension().wstring();
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));

    if (ext == L".cpp" || ext == L".c" || ext == L".h" || ext == L".hpp" || ext == L".cs") {
        return SyntaxLanguage::Cpp;
    }
    if (ext == L".js" || ext == L".ts" || ext == L".jsx" || ext == L".tsx") {
        return SyntaxLanguage::JsTs;
    }
    if (ext == L".py" || ext == L".pyw") {
        return SyntaxLanguage::Python;
    }
    if (ext == L".json") {
        return SyntaxLanguage::Json;
    }
    if (ext == L".html" || ext == L".htm" || ext == L".xml" || ext == L".svg" || ext == L".css") {
        return SyntaxLanguage::Markup;
    }
    if (ext == L".ini" || ext == L".cfg" || ext == L".conf" || ext == L".config" ||
        ext == L".inf" || ext == L".properties" || ext == L".env" ||
        ext == L".yaml" || ext == L".yml" || ext == L".toml") {
        return SyntaxLanguage::Config;
    }
    if (ext == L".ps1" || ext == L".bat" || ext == L".cmd" || ext == L".sh" || ext == L".sql") {
        return SyntaxLanguage::Script;
    }
    if (ext == L".md" || ext == L".markdown") {
        return SyntaxLanguage::Markdown;
    }

    return SyntaxLanguage::None;
}

const wchar_t* SyntaxHighlighter::GetLanguageName(SyntaxLanguage lang) {
    switch (lang) {
    case SyntaxLanguage::Cpp:      return L"C/C++";
    case SyntaxLanguage::JsTs:     return L"JavaScript/TypeScript";
    case SyntaxLanguage::Python:   return L"Python";
    case SyntaxLanguage::Json:     return L"JSON";
    case SyntaxLanguage::Markup:   return L"HTML/XML";
    case SyntaxLanguage::Config:   return L"Config";
    case SyntaxLanguage::Script:   return L"Script/SQL";
    case SyntaxLanguage::Markdown: return L"Markdown";
    default:                       return L"Plain Text";
    }
}

COLORREF SyntaxHighlighter::GetTokenColor(SyntaxTokenType type, bool darkMode) {
    if (darkMode) {
        switch (type) {
        case SyntaxTokenType::Keyword:      return RGB(86, 156, 214);   // #569CD6 VS Blue
        case SyntaxTokenType::Type:         return RGB(78, 201, 176);   // #4EC9B0 VS Teal
        case SyntaxTokenType::String:       return RGB(214, 157, 133);  // #D69D85 VS Warm Orange
        case SyntaxTokenType::Comment:      return RGB(87, 166, 74);    // #57A64A VS Olive Green
        case SyntaxTokenType::Number:       return RGB(181, 206, 168);  // #B5CEA8 VS Sage
        case SyntaxTokenType::Preprocessor: return RGB(197, 134, 192);  // #C586C0 VS Violet
        case SyntaxTokenType::Default:
        default:                            return RGB(220, 220, 220);  // #DCDCDC Default text
        }
    } else {
        switch (type) {
        case SyntaxTokenType::Keyword:      return RGB(0, 0, 255);      // #0000FF Classic Blue
        case SyntaxTokenType::Type:         return RGB(43, 145, 175);   // #2B91AF Classic Teal
        case SyntaxTokenType::String:       return RGB(163, 21, 21);    // #A31515 Crimson
        case SyntaxTokenType::Comment:      return RGB(0, 128, 0);      // #008000 Forest Green
        case SyntaxTokenType::Number:       return RGB(9, 134, 88);     // #098658 Dark Green
        case SyntaxTokenType::Preprocessor: return RGB(128, 0, 128);    // #800080 Purple
        case SyntaxTokenType::Default:
        default:                            return RGB(30, 30, 30);     // #1E1E1E Default text
        }
    }
}

void SyntaxHighlighter::Tokenize(
    SyntaxLanguage lang,
    std::wstring_view text,
    long baseCp,
    bool initialInBlockComment,
    std::vector<TokenSpan>& outSpans,
    bool& outEndedInBlockComment
) {
    outEndedInBlockComment = initialInBlockComment;
    if (lang == SyntaxLanguage::None || text.empty()) {
        return;
    }

    size_t i = 0;
    const size_t len = text.length();

    // 1. Resume multi-line block comment if entered inside one
    if (outEndedInBlockComment) {
        size_t endComment = std::wstring_view::npos;
        if (lang == SyntaxLanguage::Markup) {
            endComment = text.find(L"-->", i);
            if (endComment != std::wstring_view::npos) {
                endComment += 3;
            }
        } else {
            endComment = text.find(L"*/", i);
            if (endComment != std::wstring_view::npos) {
                endComment += 2;
            }
        }

        if (endComment != std::wstring_view::npos) {
            outSpans.push_back({ baseCp, baseCp + static_cast<long>(endComment), SyntaxTokenType::Comment });
            i = endComment;
            outEndedInBlockComment = false;
        } else {
            outSpans.push_back({ baseCp, baseCp + static_cast<long>(len), SyntaxTokenType::Comment });
            return;
        }
    }

    while (i < len) {
        wchar_t c = text[i];

        // Whitespace: skip
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
            ++i;
            continue;
        }

        // Markdown special parsing
        if (lang == SyntaxLanguage::Markdown) {
            if (c == L'#' && IsAtLineStart(text, i)) {
                size_t eol = FindEndOfLine(text, i);
                if (eol == std::wstring_view::npos) eol = len;
                outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(eol), SyntaxTokenType::Keyword });
                i = eol;
                continue;
            }
            if (c == L'`') {
                size_t endTick = text.find(L'`', i + 1);
                if (endTick != std::wstring_view::npos) {
                    outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(endTick + 1), SyntaxTokenType::String });
                    i = endTick + 1;
                    continue;
                }
            }
            if (c == L'>' && IsAtLineStart(text, i)) {
                size_t eol = FindEndOfLine(text, i);
                if (eol == std::wstring_view::npos) eol = len;
                outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(eol), SyntaxTokenType::Comment });
                i = eol;
                continue;
            }
        }

        // Single-line comment: // in C++/JS, # in Python/Config/Script, ; in Config, -- in SQL, REM/:: in Batch
        if ((c == L'/' && i + 1 < len && text[i + 1] == L'/') ||
            (c == L'#' && (lang == SyntaxLanguage::Python || lang == SyntaxLanguage::Config || lang == SyntaxLanguage::Script)) ||
            (c == L';' && lang == SyntaxLanguage::Config) ||
            (c == L'-' && i + 1 < len && text[i + 1] == L'-' && lang == SyntaxLanguage::Script) ||
            (c == L':' && i + 1 < len && text[i + 1] == L':' && lang == SyntaxLanguage::Script)) {
            size_t eol = FindEndOfLine(text, i);
            if (eol == std::wstring_view::npos) eol = len;
            outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(eol), SyntaxTokenType::Comment });
            i = eol;
            continue;
        }

        // Multi-line block comments: /* ... */ or <!-- ... -->
        if ((c == L'/' && i + 1 < len && text[i + 1] == L'*') ||
            (c == L'<' && i + 3 < len && text[i + 1] == L'!' && text[i + 2] == L'-' && text[i + 3] == L'-')) {
            bool isXmlComment = (c == L'<');
            size_t endComment = std::wstring_view::npos;
            if (isXmlComment) {
                endComment = text.find(L"-->", i + 4);
                if (endComment != std::wstring_view::npos) endComment += 3;
            } else {
                endComment = text.find(L"*/", i + 2);
                if (endComment != std::wstring_view::npos) endComment += 2;
            }

            if (endComment != std::wstring_view::npos) {
                outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(endComment), SyntaxTokenType::Comment });
                i = endComment;
            } else {
                outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(len), SyntaxTokenType::Comment });
                outEndedInBlockComment = true;
                return;
            }
            continue;
        }

        // String literals: "...", '...', `...`
        if (c == L'"' || c == L'\'' || (c == L'`' && lang == SyntaxLanguage::JsTs)) {
            wchar_t quote = c;
            size_t startStr = i;
            ++i;
            while (i < len) {
                if (text[i] == L'\\' && i + 1 < len) {
                    i += 2; // skip escaped character
                    continue;
                }
                if (text[i] == quote) {
                    ++i;
                    break;
                }
                if ((text[i] == L'\r' || text[i] == L'\n') && quote != L'`') {
                    break; // unclosed single-line string
                }
                ++i;
            }
            outSpans.push_back({ baseCp + static_cast<long>(startStr), baseCp + static_cast<long>(i), SyntaxTokenType::String });
            continue;
        }

        // Preprocessor / Directives: #include, #define in C/C++; @decorator in Python; $var in Script
        if (c == L'#' && (lang == SyntaxLanguage::Cpp)) {
            size_t startPre = i;
            ++i;
            while (i < len && (IsIdentChar(text[i]))) {
                ++i;
            }
            outSpans.push_back({ baseCp + static_cast<long>(startPre), baseCp + static_cast<long>(i), SyntaxTokenType::Preprocessor });
            continue;
        }
        if (c == L'@' && lang == SyntaxLanguage::Python) {
            size_t startDec = i;
            ++i;
            while (i < len && IsIdentChar(text[i])) {
                ++i;
            }
            outSpans.push_back({ baseCp + static_cast<long>(startDec), baseCp + static_cast<long>(i), SyntaxTokenType::Preprocessor });
            continue;
        }
        if (c == L'$' && lang == SyntaxLanguage::Script) {
            size_t startVar = i;
            ++i;
            while (i < len && (IsIdentChar(text[i]) || text[i] == L':')) {
                ++i;
            }
            outSpans.push_back({ baseCp + static_cast<long>(startVar), baseCp + static_cast<long>(i), SyntaxTokenType::Preprocessor });
            continue;
        }

        // Config Section: [SectionName]
        if (c == L'[' && lang == SyntaxLanguage::Config) {
            size_t closeBracket = text.find(L']', i + 1);
            if (closeBracket != std::wstring_view::npos) {
                outSpans.push_back({ baseCp + static_cast<long>(i), baseCp + static_cast<long>(closeBracket + 1), SyntaxTokenType::Type });
                i = closeBracket + 1;
                continue;
            }
        }

        // XML / HTML tags: <tag ... > or </tag>
        if (c == L'<' && lang == SyntaxLanguage::Markup) {
            size_t startTag = i;
            ++i;
            if (i < len && text[i] == L'/') ++i;
            size_t nameStart = i;
            while (i < len && IsIdentChar(text[i])) ++i;
            if (i > nameStart) {
                outSpans.push_back({ baseCp + static_cast<long>(startTag), baseCp + static_cast<long>(i), SyntaxTokenType::Type });
            }
            continue;
        }

        // Numbers: 123, 0xABC, 3.14
        if (IsDigit(c) || (c == L'.' && i + 1 < len && IsDigit(text[i + 1]))) {
            size_t startNum = i;
            if (c == L'0' && i + 1 < len && (text[i + 1] == L'x' || text[i + 1] == L'X' || text[i + 1] == L'b' || text[i + 1] == L'B')) {
                i += 2;
                while (i < len && (IsDigit(text[i]) || (text[i] >= L'a' && text[i] <= L'f') || (text[i] >= L'A' && text[i] <= L'F'))) {
                    ++i;
                }
            } else {
                while (i < len && (IsDigit(text[i]) || text[i] == L'.' || text[i] == L'f' || text[i] == L'u' || text[i] == L'l' || text[i] == L'L')) {
                    ++i;
                }
            }
            outSpans.push_back({ baseCp + static_cast<long>(startNum), baseCp + static_cast<long>(i), SyntaxTokenType::Number });
            continue;
        }

        // Words / Identifiers
        if (IsIdentStart(c)) {
            size_t startWord = i;
            while (i < len && IsIdentChar(text[i])) {
                ++i;
            }
            std::wstring_view word = text.substr(startWord, i - startWord);

            switch (lang) {
            case SyntaxLanguage::Cpp:
                if (IsWordInTable(kCppKeywords, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Keyword });
                } else if (IsWordInTable(kCppTypes, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Type });
                }
                break;
            case SyntaxLanguage::Python:
                if (IsWordInTable(kPythonKeywords, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Keyword });
                } else if (IsWordInTable(kPythonTypes, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Type });
                }
                break;
            case SyntaxLanguage::JsTs:
                if (IsWordInTable(kJsKeywords, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Keyword });
                } else if (IsWordInTable(kJsTypes, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Type });
                }
                break;
            case SyntaxLanguage::Json:
                if (IsWordInTable(kJsonKeywords, word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Keyword });
                }
                break;
            case SyntaxLanguage::Script:
                if (IsScriptKeyword(word)) {
                    outSpans.push_back({ baseCp + static_cast<long>(startWord), baseCp + static_cast<long>(i), SyntaxTokenType::Keyword });
                }
                break;
            default:
                break;
            }
            continue;
        }

        // Other characters / punctuation
        ++i;
    }
}
