#include "pch.hpp"
#include "app_window.hpp"
#include <shlobj.h>

namespace {
const wchar_t* const SINGLE_INSTANCE_MUTEX = L"LightNotePad_SingleInstance_Mutex_7F8A2C";
const wchar_t* const WINDOW_CLASS_NAME = L"LightNotePad_WindowClass";
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
        AppWindow::RegisterShellAssociations();
        if (!isSilent) {
            MessageBoxW(nullptr,
                L"LightNotePad successfully registered!\n\n"
                L"To make it your default text editor, choose it in Windows Settings > Apps > Default apps, "
                L"or use \"Open with\" > \"Choose another app\" on any text file.\n\n"
                L"Use /unregister to remove the associations.",
                L"LightNotePad", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    if (doUnregister) {
        AppWindow::UnregisterShellAssociations();
        if (!isSilent) {
            MessageBoxW(nullptr,
                L"LightNotePad shell associations successfully removed.",
                L"LightNotePad", MB_OK | MB_ICONINFORMATION);
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
