#include <windows.h>
#include <iostream>
#include <vector>
#include <string_view>
#include <cassert>
#include "../src/syntax_highlighter.hpp"

int main() {
    std::cout << "Testing SyntaxHighlighter Tokenize with CR (0x0D) line endings...\n" << std::flush;

    // 1. C++ with \r (RichEdit's native delimiter)
    {
        std::wstring text = L"// This is a comment\rint count = 42;\rreturn count;";
        std::vector<TokenSpan> spans;
        bool endedInComment = false;
        SyntaxHighlighter::Tokenize(SyntaxLanguage::Cpp, text, 0, false, spans, endedInComment);

        std::cout << "C++ spans found: " << spans.size() << "\n" << std::flush;
        for (const auto& s : spans) {
            std::wcout << L"  Span [" << s.startCp << L".." << s.endCp << L"]: "
                       << text.substr(s.startCp, s.endCp - s.startCp)
                       << L" (type: " << (int)s.type << L")\n" << std::flush;
        }

        assert(spans.size() == 4);
        assert(spans[0].type == SyntaxTokenType::Comment);
        assert(spans[1].type == SyntaxTokenType::Keyword); // "int"
        assert(spans[2].type == SyntaxTokenType::Number);  // "42"
        assert(spans[3].type == SyntaxTokenType::Keyword); // "return"
    }

    // 2. Python with \r
    {
        std::wstring text = L"# Python comment\rdef calculate(val: int):\r    return val * 2";
        std::vector<TokenSpan> spans;
        bool endedInComment = false;
        SyntaxHighlighter::Tokenize(SyntaxLanguage::Python, text, 0, false, spans, endedInComment);

        std::cout << "Python spans found: " << spans.size() << "\n" << std::flush;
        for (const auto& s : spans) {
            std::wcout << L"  Span [" << s.startCp << L".." << s.endCp << L"]: "
                       << text.substr(s.startCp, s.endCp - s.startCp)
                       << L" (type: " << (int)s.type << L")\n" << std::flush;
        }

        assert(spans.size() == 5);
        assert(spans[0].type == SyntaxTokenType::Comment);
        assert(spans[1].type == SyntaxTokenType::Keyword); // "def"
        assert(spans[2].type == SyntaxTokenType::Type);    // "int"
        assert(spans[3].type == SyntaxTokenType::Keyword); // "return"
        assert(spans[4].type == SyntaxTokenType::Number);  // "2"
    }

    // 3. Markdown with \r
    {
        std::wstring text = L"# Heading 1\rLine two text\r# Heading 2\rLine four text";
        std::vector<TokenSpan> spans;
        bool endedInComment = false;
        SyntaxHighlighter::Tokenize(SyntaxLanguage::Markdown, text, 0, false, spans, endedInComment);

        std::cout << "Markdown spans found: " << spans.size() << "\n" << std::flush;
        for (const auto& s : spans) {
            std::wcout << L"  Span [" << s.startCp << L".." << s.endCp << L"]: "
                       << text.substr(s.startCp, s.endCp - s.startCp)
                       << L" (type: " << (int)s.type << L")\n" << std::flush;
        }

        assert(spans.size() == 2);
        assert(spans[0].type == SyntaxTokenType::Keyword); // "# Heading 1"
        assert(spans[1].type == SyntaxTokenType::Keyword); // "# Heading 2"
    }

    // 4. SQL with \r
    {
        std::wstring text = L"-- SQL comment\rSELECT count FROM users\rWHERE count > 0;";
        std::vector<TokenSpan> spans;
        bool endedInComment = false;
        SyntaxHighlighter::Tokenize(SyntaxLanguage::Script, text, 0, false, spans, endedInComment);

        std::cout << "SQL spans found: " << spans.size() << "\n" << std::flush;
        for (const auto& s : spans) {
            std::wcout << L"  Span [" << s.startCp << L".." << s.endCp << L"]: "
                       << text.substr(s.startCp, s.endCp - s.startCp)
                       << L" (type: " << (int)s.type << L")\n" << std::flush;
        }

        assert(spans.size() == 5);
        assert(spans[0].type == SyntaxTokenType::Comment); // "-- SQL comment"
        assert(spans[1].type == SyntaxTokenType::Keyword); // "SELECT"
        assert(spans[2].type == SyntaxTokenType::Keyword); // "FROM"
        assert(spans[3].type == SyntaxTokenType::Keyword); // "WHERE"
        assert(spans[4].type == SyntaxTokenType::Number);  // "0"
    }

    // 5. Config with \r
    {
        std::wstring text = L"; Config comment\r[Database]\rhost = localhost\r# another comment\rport = 5432";
        std::vector<TokenSpan> spans;
        bool endedInComment = false;
        SyntaxHighlighter::Tokenize(SyntaxLanguage::Config, text, 0, false, spans, endedInComment);

        std::cout << "Config spans found: " << spans.size() << "\n" << std::flush;
        for (const auto& s : spans) {
            std::wcout << L"  Span [" << s.startCp << L".." << s.endCp << L"]: "
                       << text.substr(s.startCp, s.endCp - s.startCp)
                       << L" (type: " << (int)s.type << L")\n" << std::flush;
        }

        assert(spans.size() == 4);
        assert(spans[0].type == SyntaxTokenType::Comment); // "; Config comment"
        assert(spans[1].type == SyntaxTokenType::Type);    // "[Database]"
        assert(spans[2].type == SyntaxTokenType::Comment); // "# another comment"
        assert(spans[3].type == SyntaxTokenType::Number);  // "5432"
    }

    std::cout << "\nALL 5 TOKENIZER LINE-ENDING TESTS PASSED (100% SUCCESS)!\n" << std::flush;
    return 0;
}
