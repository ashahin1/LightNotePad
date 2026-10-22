<p align="center">
  <img src="resources/app_icon_preview.png" width="128" height="128" alt="LightNotePad Icon" />
</p>

# LightNotePad

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg)](https://www.microsoft.com/windows)
[![Release](https://img.shields.io/github/v/release/ashahin1/LightNotePad?color=blue)](https://github.com/ashahin1/LightNotePad/releases)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-272%20KB-brightgreen.svg)]()
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
| **Standalone Binary Size** | **272 KB** | ~50+ MB (Packaged App) | **>180× smaller** |
| **Cold Startup Latency** | **~180 ms** | ~5,000 ms | **>25× faster** |
| **Private Working Set RAM** | **3.5 MB** | 17.5 MB | **5× lower RAM** |
| **External Runtime DLLs** | **0** (Native Win32 only) | Multiple WinRT/XAML packages | Zero bloat |

---

## 🚀 Key Highlights

* **Instant Cold Launch**: Launches immediately without XAML, WinUI, or Electron runtime baggage.
* **Microscopic Footprint**: Single standalone **272 KB** `.exe` with zero installer needed.
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
* **Zero-Admin Shell Registration & Default Apps Integration**:
  * Run `LightNotePad.exe /register` to register as a text viewer across all supported formats in your user profile without admin rights.
  * Fully integrates with Windows Settings > Apps > Default apps via `Capabilities` and `RegisteredApplications`.
  * Fully supports `/silent` flag for script-driven and automated deployments.

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

## 📂 Supported Formats

LightNotePad includes a single unified source of truth for text file extensions, powering the Open/Save dialog filters, multi-file selection, shell context menus, and Windows Default Apps registration:

| Category | File Extensions | Wildcard Pattern |
| :--- | :--- | :--- |
| **Plain Text & Logs** | `.txt`, `.text`, `.log`, `.out`, `.err` | `*.txt;*.text;*.log;*.out;*.err` |
| **Markdown Documents** | `.md`, `.markdown` | `*.md;*.markdown` |
| **Configuration & Settings** | `.ini`, `.cfg`, `.conf`, `.config`, `.inf`, `.properties`, `.env` | `*.ini;*.cfg;*.conf;*.config;*.inf;*.properties;*.env` |
| **Structured Data** | `.json`, `.xml`, `.yaml`, `.yml`, `.csv`, `.tsv`, `.toml` | `*.json;*.xml;*.yaml;*.yml;*.csv;*.tsv;*.toml` |
| **Source Code & Scripts** | `.c`, `.cpp`, `.h`, `.hpp`, `.cs`, `.js`, `.ts`, `.py`, `.html`, `.htm`, `.css`, `.sql`, `.sh`, `.bat`, `.cmd`, `.ps1` | `*.c;*.cpp;*.h;*.hpp;*.cs;*.js;*.ts;*.py;*.html;*.htm;*.css;*.sql;*.sh;*.bat;*.cmd;*.ps1` |
| **All Files** | Any extension (`*.*`) via dialog filter, CLI argument, or Drag & Drop | `*.*` |

---

## 💻 Command Line Usage

```cmd
:: Open a blank document
LightNotePad.exe

:: Open a specific file
LightNotePad.exe "C:\path\to\file.txt"

:: Open multiple files simultaneously into tabs
LightNotePad.exe file1.txt file2.log config.ini data.json

:: Open an independent window instance
LightNotePad.exe --new-window

:: Register shell associations in user profile (no admin rights required)
:: Sets up ProgID (LightNotePad.Document), OpenWithProgids, Applications\LightNotePad.exe,
:: and Windows Default Apps Capabilities (Software\LightNotePad\Capabilities & RegisteredApplications).
LightNotePad.exe /register

:: Register silently (no graphical message box)
LightNotePad.exe /register /silent

:: Remove all registered shell associations, ProgID, and Capabilities cleanly
LightNotePad.exe /unregister

:: Remove associations silently
LightNotePad.exe /unregister /silent
```

> **Note on Windows Default Apps**: Per Windows 10/11 security architecture, running `/register` publishes the app's capabilities to Windows. To set LightNotePad as your default handler for specific extensions, select it in **Windows Settings > Apps > Default apps**, or right-click any supported file and choose **"Open with" > "Choose another app" > "Always"**.

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
