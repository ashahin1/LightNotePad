#include "pch.hpp"
#include "app_window.hpp"
#include <shlobj.h>

namespace {
const wchar_t* const SINGLE_INSTANCE_MUTEX = L"LightNotePad_SingleInstance_Mutex_7F8A2C";
const wchar_t* const WINDOW_CLASS_NAME = L"LightNotePad_WindowClass";

bool RegisterFileAssociation() {
    wchar_t exePath[MAX_PATH * 2] = { 0 };
    GetModuleFileNameW(nullptr, exePath, _countof(exePath));

    std::wstring openCmd = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
    std::wstring iconCmd = L"\"" + std::wstring(exePath) + L"\",0";

    HKEY hKey = nullptr;
    // 1. HKCU\Software\Classes\LightNotePad.Document
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"Text Document", (DWORD)(sizeof(L"Text Document")));
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document\\DefaultIcon", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)iconCmd.c_str(), (DWORD)((iconCmd.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document\\shell\\open\\command", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)openCmd.c_str(), (DWORD)((openCmd.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // 2. Register for common text file extensions
    const wchar_t* extensions[] = { L".txt", L".log", L".ini", L".cfg", L".md" };
    for (const wchar_t* ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + std::wstring(ext) + L"\\OpenWithProgids";
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"LightNotePad.Document", 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
            RegCloseKey(hKey);
        }
    }

    // 3. Register as an Application
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\LightNotePad.exe\\SupportedTypes", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        for (const wchar_t* ext : extensions) {
            RegSetValueExW(hKey, ext, 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
        }
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\LightNotePad.exe\\shell\\open\\command", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)openCmd.c_str(), (DWORD)((openCmd.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool UnregisterFileAssociation() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\LightNotePad.Document");
    const wchar_t* extensions[] = { L".txt", L".log", L".ini", L".cfg", L".md" };
    for (const wchar_t* ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + std::wstring(ext) + L"\\OpenWithProgids";
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"LightNotePad.Document");
            RegCloseKey(hKey);
        }
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\LightNotePad.exe");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX) };
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // Parse command line arguments
    std::vector<std::wstring> initialFiles;
    bool isSilent = false;
    bool doRegister = false;
    bool doUnregister = false;
    bool forceNewWindow = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (_wcsicmp(arg.c_str(), L"/silent") == 0 || _wcsicmp(arg.c_str(), L"-silent") == 0) {
                isSilent = true;
            } else if (_wcsicmp(arg.c_str(), L"/register") == 0 || _wcsicmp(arg.c_str(), L"-register") == 0) {
                doRegister = true;
            } else if (_wcsicmp(arg.c_str(), L"/unregister") == 0 || _wcsicmp(arg.c_str(), L"-unregister") == 0) {
                doUnregister = true;
            } else if (_wcsicmp(arg.c_str(), L"--new-window") == 0 ||
                       _wcsicmp(arg.c_str(), L"-new-window") == 0 ||
                       _wcsicmp(arg.c_str(), L"/new-window") == 0 ||
                       _wcsicmp(arg.c_str(), L"-n") == 0) {
                forceNewWindow = true;
            } else if (!arg.empty() && arg[0] != L'-' && arg[0] != L'/') {
                initialFiles.push_back(arg);
            }
        }
        LocalFree(argv);
    }

    if (doRegister) {
        RegisterFileAssociation();
        if (!isSilent) {
            MessageBoxW(nullptr,
                L"LightNotePad has been registered in your user profile.\nYou can now select it in Windows 'Open with' or Default Apps.",
                L"LightNotePad Registration", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    if (doUnregister) {
        UnregisterFileAssociation();
        if (!isSilent) {
            MessageBoxW(nullptr, L"LightNotePad file associations removed.", L"LightNotePad Registration", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    // Single-instance forwarding with smart tabs
    HANDLE hMutex = nullptr;
    if (!forceNewWindow) {
        hMutex = CreateMutexW(nullptr, FALSE, SINGLE_INSTANCE_MUTEX);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND hExisting = FindWindowW(WINDOW_CLASS_NAME, nullptr);
            if (hExisting) {
                for (const auto& file : initialFiles) {
                    COPYDATASTRUCT cds = {};
                    cds.dwData = 1;
                    cds.cbData = static_cast<DWORD>((file.length() + 1) * sizeof(wchar_t));
                    cds.lpData = const_cast<wchar_t*>(file.c_str());
                    SendMessageW(hExisting, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
                }
                if (IsIconic(hExisting)) {
                    ShowWindow(hExisting, SW_RESTORE);
                }
                SetForegroundWindow(hExisting);
            }
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    AppWindow app;
    if (!app.Create(hInstance, nCmdShow, initialFiles)) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    HWND hWnd = app.GetHwnd();
    HACCEL hAccel = app.GetAccel();

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        HWND hFindDlg = app.GetFindDialog();
        if (hFindDlg && IsDialogMessageW(hFindDlg, &msg)) {
            continue;
        }

        if (hAccel && TranslateAcceleratorW(hWnd, hAccel, &msg)) {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
