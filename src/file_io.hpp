#pragma once
#include "pch.hpp"

enum class FileEncoding {
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_BE,
    ANSI
};

enum class LineEnding {
    CRLF,
    LF
};

struct LoadedDocument {
    std::wstring text;
    FileEncoding encoding = FileEncoding::UTF8;
    LineEnding lineEnding = LineEnding::CRLF;
};

class FileIO {
public:
    static bool Load(const std::wstring& path, LoadedDocument& outDoc);
    static bool Save(const std::wstring& path, const std::wstring& text, FileEncoding encoding, LineEnding lineEnding);

    static const wchar_t* EncodingToString(FileEncoding enc);
    static const wchar_t* LineEndingToString(LineEnding le);

private:
    static bool IsValidUtf8(const uint8_t* data, size_t length);
};
