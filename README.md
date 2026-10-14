<p align="center">
  <img src="resources/app_icon_preview.png" width="128" height="128" alt="LightNotePad Icon" />
</p>

# LightNotePad

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg)](https://www.microsoft.com/windows)
[![Release](https://img.shields.io/github/v/release/ashahin1/LightNotePad?color=blue)](https://github.com/ashahin1/LightNotePad/releases)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-255%20KB-brightgreen.svg)]()
[![RAM Usage](https://img.shields.io/badge/Private%20RAM-%3C%204%20MB-success.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An ultra-fast, minimalist, native Win32 text editor for Windows 10 & 11.
Built with pure C++20 and portable MSVC with **zero runtime DLL dependencies**.

[⬇️ **Download Latest Release (LightNotePad.exe)**](https://github.com/ashahin1/LightNotePad/releases/latest)

---

## ⚡ Performance Benchmarks

Measured on Windows 11 (64-bit):

| Metric | LightNotePad | Windows 11 Notepad (WinUI) | Difference |
| :--- | :--- | :--- | :--- |
| **Standalone Binary Size** | **255 KB** | ~50+ MB (Packaged App) | **>200× smaller** |
| **Cold Startup Latency** | **~200 ms** | ~5,000 ms | **>20× faster** |
| **Private Working Set RAM** | **3.5 MB** | 17.5 MB | **5× lower RAM** |
| **External Runtime DLLs** | **0** (Native Win32 only) | Multiple WinRT/XAML packages | Zero bloat |

---

## 🚀 Key Highlights

* **Instant Cold Launch**: Launches immediately without XAML, WinUI, or Electron runtime baggage.
* **Microscopic Footprint**: Single standalone **255 KB** `.exe` with zero installer needed.
* **Smart Tabs (Auto-Hiding)**:
  * When **1 file** is open: The tab bar completely hides itself (0 px height), providing the clean, full-screen canvas of classic Notepad.
  * When **2+ files** are opened: A sleek tab bar appears automatically with active tab accents, close buttons (`×`), and a new tab (`+`) button.
  * Middle-click any tab to close it.
* **Native Windows 10/11 Dark Mode**:
  * Immersive dark title bar (`DWMWA_USE_IMMERSIVE_DARK_MODE`).
  * Dark menu bar (via custom Win32 UAH menu rendering) and dark popup menus (via `uxtheme.dll`).
  * Seamless dark palette (`#1E1E1E`), dark tab bar, and custom-rendered dark status bar.
  * Automatically follows the Windows system theme and remembers user preferences.
* **Font Selection & Typography**:
  * Native font picker (`View -> Font...`) supporting all installed system fonts with custom size, weight, and italic options.
  * Single-click **"Restore Default Font"** to instantly revert to the clean default.
  * Persistent font preferences stored in `HKCU\Software\LightNotePad\Font`.
  * Default font cascade: **Cascadia Mono** $\rightarrow$ **Consolas** $\rightarrow$ **Courier New**.
  * Full `PerMonitorV2` High-DPI awareness (never blurs across monitors with different scaling factors).
* **Recent Files (MRU List)**:
  * Quick access to recently opened files via `File -> Open Recent` with keyboard mnemonics (`&1` ... `&9`).
  * Windows Taskbar Jump List sync (`SHAddToRecentDocs`).
  * Zero startup penalty: lazy disk validation only when an entry is clicked.
* **Robust File & Encoding Engine**:
  * Auto-detects and preserves:
    * **UTF-8** (with and without BOM)
    * **UTF-16 LE** & **UTF-16 BE**
    * **ANSI** (Windows system codepage)
  * Auto-detects and preserves **CRLF** vs. **LF** line endings.
  * Safe atomic file saving with temporary file swaps to prevent data loss or file corruption.
* **Smart Single-Instance & Independent Multi-Window**:
  * Opening files from Explorer forwards them as new tabs into the active window (`WM_COPYDATA`).
  * Pressing `Ctrl+Shift+N` (or passing `--new-window`) spawns a completely independent window instance.
* **Native Document Printing**:
  * Print documents directly via `File -> Print...` (`Ctrl+P`) using the native Windows Print dialog (`PrintDlgW`).
  * Full pagination, margins, word wrapping, and automatic headers (file name) & footers (page numbers).
  * Automatically renders crisp black text on white paper even when editing in Dark Mode.
* **Zero-Admin Shell Registration**:
  * Run `LightNotePad.exe /register` to register as a text viewer in your user profile without admin rights.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + N` / `Ctrl + T` | New Tab |
| `Ctrl + Shift + N` | New Window (independent instance) |
| `Ctrl + O` | Open File |
| `Ctrl + S` | Save |
| `Ctrl + Shift + S` | Save As |
| `Ctrl + P` | Print Document |
| `Ctrl + W` | Close Tab |
| `Ctrl + Tab` / `Ctrl + Shift + Tab` | Next / Previous Tab |
| `Ctrl + F` | Find |
| `F3` / `Shift + F3` | Find Next / Find Previous |
| `Ctrl + H` | Replace |
| `Alt + Z` | Toggle Word Wrap |
| `Ctrl + +` / `Ctrl + -` | Zoom In / Zoom Out |
| `Ctrl + 0` | Reset Zoom (100%) |
| `Ctrl + Mouse Wheel` | Smooth Zoom |
| `F5` | Insert Date and Time |
| `F1` | Keyboard Shortcuts Cheat Sheet |

---

## 💻 Command Line Usage

```cmd
:: Open a blank document
LightNotePad.exe

:: Open a specific file
LightNotePad.exe "C:\path\to\file.txt"

:: Open multiple files into tabs
LightNotePad.exe file1.txt file2.log config.ini

:: Open an independent window instance
LightNotePad.exe --new-window

:: Register as default text editor in user profile (no admin rights required)
LightNotePad.exe /register

:: Remove file associations
LightNotePad.exe /unregister
```

---

## 🛠️ Building from Source

### Prerequisites
* Windows 10 (Build 1809+) or Windows 11
* MSVC toolchain (Visual Studio 2022 / Build Tools or standalone portable MSVC) with C++20 support

### Build Commands

```cmd
:: Build the project incrementally
build.bat

:: Perform a clean rebuild
build.bat /rebuild

:: Clean build outputs
build.bat /clean
```

The build script compiles with:
- C++20 (`/std:c++20`)
- Link-Time Code Generation (`/GL`, `/LTCG`)
- Dead-code stripping (`/OPT:REF`, `/OPT:ICF`)
- Embedded High-DPI manifest (`PerMonitorV2`)
- Precompiled headers (`pch.hpp`) for sub-second rebuilds.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.

Developed with care by **Ahmed Amer Shahin**.
