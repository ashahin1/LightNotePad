# Contributing to LightNotePad

Thank you for your interest in contributing to **LightNotePad**!

## Core Principles & Design Constraints

Before submitting contributions, please keep in mind the fundamental design goals of the project:

1. **Extreme Performance**: Sub-200ms cold startup, instantaneous file opening and editing, and minimal memory footprint.
2. **Zero External Runtime Dependencies**: Standalone single `.exe` utilizing only native Windows 10/11 Win32 APIs, Common Controls, and native printing/dialog APIs. No external runtime DLLs.
3. **Strict Size Budget**: Total executable size must strictly remain under **300 KB** (currently ~285 KB).
4. **Clean Code & Toolchain**: Pure C++20 (`/std:c++20`), static runtime library (`/MT`), zero-warning compilation (`/W4`).

---

## Development Setup

### Prerequisites
- Windows 10 (Build 1809+) or Windows 11 (64-bit).
- MSVC compiler supporting C++20 (Visual Studio 2022, VS Build Tools, or portable MSVC).

### Building Locally

The build system automatically locates Visual Studio installations via `vswhere.exe` or portable MSVC tools:

```cmd
:: Build incrementally
build.bat

:: Clean rebuild
build.bat /rebuild

:: Clean artifacts
build.bat /clean
```

---

## Testing & Verification

Always run the automated verification scripts before submitting a pull request:

```powershell
# Run the automated functional and encoding test suite
powershell -ExecutionPolicy Bypass -File tools\test_all.ps1
```

The test suite validates:
- Executable generation and launch.
- Standalone execution without extra arguments.
- Multi-instance spawning via `--new-window`.
- Shell association registration and clean removal (`tools/test_registration.ps1`).
- Loading and handling files across encodings (UTF-8 with BOM, UTF-8 without BOM, UTF-16 LE, ANSI CRLF and LF).
- Process cleanup and graceful termination.

---

## Coding Guidelines

- Format all C++ code using the root `.clang-format` rules.
- Maintain `PerMonitorV2` High-DPI scaling accuracy.
- Support both Light and Dark mode faithfully across all UI components (edit control, status bar, dialogs, menus).
- Preserve file encodings (UTF-8, UTF-16, ANSI) and line endings (CRLF, LF) accurately upon save.
- Ensure all Win32 handles (`HWND`, `HFONT`, `HBRUSH`, `HDC`, `HANDLE`) are properly cleaned up to prevent GDI / resource leaks.
- Do not introduce runtime DLL dependencies or heavy third-party frameworks.
