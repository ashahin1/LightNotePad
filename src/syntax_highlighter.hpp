#pragma once
#include "pch.hpp"

enum class SyntaxLanguage : uint8_t {
    None = 0,
    Cpp,
    JsTs,
    Python,
    Json,
    Markup,
    Config,
    Script,
    Markdown
};

enum class SyntaxTokenType : uint8_t {
    Default = 0,
    Keyword,
    Type,
    String,
    Comment,
    Number,
    Preprocessor
};

struct TokenSpan {
    long startCp;
    long endCp;
    SyntaxTokenType type;
};

class SyntaxHighlighter {
public:
    static SyntaxLanguage DetectLanguage(const std::wstring& filePath);
    static const wchar_t* GetLanguageName(SyntaxLanguage lang);
    static COLORREF GetTokenColor(SyntaxTokenType type, bool darkMode);

    static void Tokenize(
        SyntaxLanguage lang,
        std::wstring_view text,
        long baseCp,
        bool initialInBlockComment,
        std::vector<TokenSpan>& outSpans,
        bool& outEndedInBlockComment
    );
};
