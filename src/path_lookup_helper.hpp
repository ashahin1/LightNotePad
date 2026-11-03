#pragma once
#include <string>
#include <windows.h>

class PathLookupHelper {
public:
    static std::wstring NormalizePath(const std::wstring& path) {
        if (path.empty()) return L"";
        wchar_t buf[MAX_PATH];
        DWORD len = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
        if (len > 0 && len < MAX_PATH) {
            wchar_t longBuf[MAX_PATH];
            DWORD longLen = GetLongPathNameW(buf, longBuf, MAX_PATH);
            if (longLen > 0 && longLen < MAX_PATH) {
                return std::wstring(longBuf, longLen);
            }
            return std::wstring(buf, len);
        }
        return path;
    }

    static bool ArePathsEquivalent(const std::wstring& path1, const std::wstring& path2) {
        if (path1.empty() || path2.empty()) return false;

        const std::wstring norm1 = NormalizePath(path1);
        const std::wstring norm2 = NormalizePath(path2);

        // Fast string comparison (handles casing, relative resolution, slash normalization)
        if (_wcsicmp(norm1.c_str(), norm2.c_str()) == 0) {
            return true;
        }

        // Secondary check: physical filesystem file identifier (handles hard links, junctions, subst)
        HANDLE h1 = CreateFileW(norm1.c_str(), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (h1 == INVALID_HANDLE_VALUE) return false;

        HANDLE h2 = CreateFileW(norm2.c_str(), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (h2 == INVALID_HANDLE_VALUE) {
            CloseHandle(h1);
            return false;
        }

        BY_HANDLE_FILE_INFORMATION fi1 = {}, fi2 = {};
        bool match = false;
        if (GetFileInformationByHandle(h1, &fi1) && GetFileInformationByHandle(h2, &fi2)) {
            match = (fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber &&
                     fi1.nFileIndexHigh == fi2.nFileIndexHigh &&
                     fi1.nFileIndexLow == fi2.nFileIndexLow);
        }

        CloseHandle(h1);
        CloseHandle(h2);
        return match;
    }
};
