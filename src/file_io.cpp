#include "pch.hpp"
#include "file_io.hpp"

const wchar_t* FileIO::EncodingToString(FileEncoding enc) {
    switch (enc) {
    case FileEncoding::UTF8:     return L"UTF-8";
    case FileEncoding::UTF8_BOM: return L"UTF-8 (BOM)";
    case FileEncoding::UTF16_LE: return L"UTF-16 LE";
    case FileEncoding::UTF16_BE: return L"UTF-16 BE";
    case FileEncoding::ANSI:     return L"ANSI";
    default:                     return L"UTF-8";
    }
}

const wchar_t* FileIO::LineEndingToString(LineEnding le) {
    switch (le) {
    case LineEnding::CRLF: return L"Windows (CRLF)";
    case LineEnding::LF:   return L"Unix (LF)";
    default:               return L"Windows (CRLF)";
    }
}

bool FileIO::IsValidUtf8(const uint8_t* data, size_t length) {
    size_t i = 0;
    while (i < length) {
        uint8_t c = data[i];
        if (c <= 0x7F) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= length || (data[i + 1] & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false; // Overlong encoding
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) return false;
            if (c == 0xE0 && data[i + 1] < 0xA0) return false; // Overlong
            if (c == 0xED && data[i + 1] >= 0xA0) return false; // UTF-16 surrogates
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) return false;
            if (c == 0xF0 && data[i + 1] < 0x90) return false; // Overlong
            if (c > 0xF4 || (c == 0xF4 && data[i + 1] > 0x8F)) return false; // Exceeds Unicode limit
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool FileIO::Load(const std::wstring& path, LoadedDocument& outDoc) {
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }

    if (fileSize.QuadPart == 0) {
        CloseHandle(hFile);
        outDoc.text.clear();
        outDoc.encoding = FileEncoding::UTF8;
        outDoc.lineEnding = LineEnding::CRLF;
        return true;
    }

    // Safety check for ridiculous files (e.g. > 500 MB)
    if (fileSize.QuadPart > 500 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize.QuadPart));
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) || bytesRead != buffer.size()) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    const uint8_t* pData = buffer.data();
    size_t size = buffer.size();

    // Check BOMs
    if (size >= 3 && pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF) {
        outDoc.encoding = FileEncoding::UTF8_BOM;
        pData += 3;
        size -= 3;
    } else if (size >= 2 && pData[0] == 0xFF && pData[1] == 0xFE) {
        outDoc.encoding = FileEncoding::UTF16_LE;
        pData += 2;
        size -= 2;
    } else if (size >= 2 && pData[0] == 0xFE && pData[1] == 0xFF) {
        outDoc.encoding = FileEncoding::UTF16_BE;
        pData += 2;
        size -= 2;
    } else {
        // No BOM: Check if valid UTF-8
        if (IsValidUtf8(pData, size)) {
            outDoc.encoding = FileEncoding::UTF8;
        } else {
            outDoc.encoding = FileEncoding::ANSI;
        }
    }

    // Decode to std::wstring
    switch (outDoc.encoding) {
    case FileEncoding::UTF16_LE: {
        size_t charCount = size / 2;
        outDoc.text.resize(charCount);
        memcpy(outDoc.text.data(), pData, charCount * 2);
        break;
    }
    case FileEncoding::UTF16_BE: {
        size_t charCount = size / 2;
        outDoc.text.resize(charCount);
        const uint16_t* pSrc = reinterpret_cast<const uint16_t*>(pData);
        for (size_t i = 0; i < charCount; ++i) {
            uint16_t val = pSrc[i];
            outDoc.text[i] = static_cast<wchar_t>((val >> 8) | (val << 8));
        }
        break;
    }
    case FileEncoding::UTF8:
    case FileEncoding::UTF8_BOM: {
        if (size == 0) {
            outDoc.text.clear();
        } else {
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(pData), static_cast<int>(size), nullptr, 0);
            if (wideLen > 0) {
                outDoc.text.resize(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(pData), static_cast<int>(size), outDoc.text.data(), wideLen);
            } else {
                outDoc.text.clear();
            }
        }
        break;
    }
    case FileEncoding::ANSI: {
        if (size == 0) {
            outDoc.text.clear();
        } else {
            int wideLen = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(pData), static_cast<int>(size), nullptr, 0);
            if (wideLen > 0) {
                outDoc.text.resize(wideLen);
                MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(pData), static_cast<int>(size), outDoc.text.data(), wideLen);
            } else {
                outDoc.text.clear();
            }
        }
        break;
    }
    }

    // Detect Line Endings: count CRLF vs lone LF
    size_t crlfCount = 0;
    size_t lfCount = 0;
    for (size_t i = 0; i < outDoc.text.size(); ++i) {
        if (outDoc.text[i] == L'\r') {
            if (i + 1 < outDoc.text.size() && outDoc.text[i + 1] == L'\n') {
                crlfCount++;
                i++; // skip LF
            }
        } else if (outDoc.text[i] == L'\n') {
            lfCount++;
        }
    }

    if (lfCount > crlfCount) {
        outDoc.lineEnding = LineEnding::LF;
    } else {
        outDoc.lineEnding = LineEnding::CRLF;
    }

    return true;
}

bool FileIO::Save(const std::wstring& path, const std::wstring& text, FileEncoding encoding, LineEnding lineEnding) {
    // 1. Normalize line endings
    std::wstring normalized;
    normalized.reserve(text.size() + (lineEnding == LineEnding::CRLF ? text.size() / 20 : 0));

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                i++; // skip '\n'
            }
            if (lineEnding == LineEnding::CRLF) {
                normalized.push_back(L'\r');
                normalized.push_back(L'\n');
            } else {
                normalized.push_back(L'\n');
            }
        } else if (ch == L'\n') {
            if (lineEnding == LineEnding::CRLF) {
                normalized.push_back(L'\r');
                normalized.push_back(L'\n');
            } else {
                normalized.push_back(L'\n');
            }
        } else {
            normalized.push_back(ch);
        }
    }

    // 2. Encode to target bytes
    std::vector<uint8_t> outBytes;

    switch (encoding) {
    case FileEncoding::UTF8_BOM: {
        // Write UTF-8 BOM
        outBytes.push_back(0xEF);
        outBytes.push_back(0xBB);
        outBytes.push_back(0xBF);
        [[fallthrough]];
    }
    case FileEncoding::UTF8: {
        if (!normalized.empty()) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, normalized.data(), static_cast<int>(normalized.size()), nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                size_t offset = outBytes.size();
                outBytes.resize(offset + utf8Len);
                WideCharToMultiByte(CP_UTF8, 0, normalized.data(), static_cast<int>(normalized.size()), reinterpret_cast<char*>(outBytes.data() + offset), utf8Len, nullptr, nullptr);
            }
        }
        break;
    }
    case FileEncoding::UTF16_LE: {
        outBytes.push_back(0xFF);
        outBytes.push_back(0xFE);
        size_t byteCount = normalized.size() * sizeof(wchar_t);
        size_t offset = outBytes.size();
        outBytes.resize(offset + byteCount);
        memcpy(outBytes.data() + offset, normalized.data(), byteCount);
        break;
    }
    case FileEncoding::UTF16_BE: {
        outBytes.push_back(0xFE);
        outBytes.push_back(0xFF);
        size_t offset = outBytes.size();
        outBytes.resize(offset + normalized.size() * sizeof(wchar_t));
        uint16_t* pDest = reinterpret_cast<uint16_t*>(outBytes.data() + offset);
        for (size_t i = 0; i < normalized.size(); ++i) {
            uint16_t val = static_cast<uint16_t>(normalized[i]);
            pDest[i] = static_cast<uint16_t>((val >> 8) | (val << 8));
        }
        break;
    }
    case FileEncoding::ANSI: {
        if (!normalized.empty()) {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, normalized.data(), static_cast<int>(normalized.size()), nullptr, 0, nullptr, nullptr);
            if (ansiLen > 0) {
                outBytes.resize(ansiLen);
                WideCharToMultiByte(CP_ACP, 0, normalized.data(), static_cast<int>(normalized.size()), reinterpret_cast<char*>(outBytes.data()), ansiLen, nullptr, nullptr);
            }
        }
        break;
    }
    }

    // 3. Atomic write: write to temp file, then rename/replace
    std::wstring tempPath = path + L".tmp_" + std::to_wstring(GetCurrentProcessId());
    HANDLE hFile = CreateFileW(
        tempPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten = 0;
    if (!outBytes.empty()) {
        if (!WriteFile(hFile, outBytes.data(), static_cast<DWORD>(outBytes.size()), &bytesWritten, nullptr) || bytesWritten != outBytes.size()) {
            CloseHandle(hFile);
            DeleteFileW(tempPath.c_str());
            return false;
        }
    }
    CloseHandle(hFile);

    // Replace target file
    if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.c_str());
        return false;
    }

    return true;
}
