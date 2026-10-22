<p align="center">
  <img src="resources/app_icon_preview.png" width="128" height="128" alt="LightNotePad Icon" />
</p>

# LightNotePad

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg)](https://www.microsoft.com/windows)
[![Release](https://img.shields.io/github/v/release/ashahin1/LightNotePad?color=blue)](https://github.com/ashahin1/LightNotePad/releases)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-285%20KB-brightgreen.svg)]()
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
| **Standalone Binary Size** | **285 KB** | ~50+ MB (Packaged App) | **>175× smaller** |
| **Cold Startup Latency** | **~185 ms** | ~5,000 ms | **>25× faster** |
| **Private Working Set RAM** | **3.5 MB** | 17.5 MB | **5× lower RAM** |
| **External Runtime DLLs** | **0** (Native Win32 only) | Multiple WinRT/XAML packages | Zero bloat |

---

## ✨ Core Features

### 🎨 Real-Time Syntax Highlighting
* **Multi-Language Support**: Instant colorization for C/C++, C#, JavaScript/TypeScript, Python, JSON, HTML/XML/CSS, Configuration/TOML/YAML, Shell/Batch/SQL, and Markdown.
* **Visual Studio Palettes**: Faithfully renders official Microsoft Visual Studio Dark+ and Light+ theme palettes.
* **ITextDocument COM Architecture**: Operates via RichEdit's Text Object Model with frozen viewport updates and suspended undo tracking—**never pollutes the undo history (`Ctrl+Z`)** and never moves the caret.
* **Zero-RTF Guarantee**: Files saved to disk remain **100% clean plain text** with zero embedded formatting or RTF markup.
* **Pure Plain Text Mode**: Files like `.txt` and `.log` operate in strict `TM_PLAINTEXT` mode with zero tokenizer overhead.
* **Smart Safety Guard**: Files larger than 1 MB automatically fall back to pure plain text to guarantee instantaneous editing speeds.
* **On-Demand Toggle**: Instantly enable or disable syntax colorization via `View -> Syntax Highlighting` (persisted in registry).

### 📑 Smart Tabs & Minimalist Canvas
* **Dynamic Auto-Hiding Tab Bar**: When only 1 file is open, the tab bar collapses completely (0 px height) for the classic, distraction-free Notepad canvas. When 2+ files are opened, a sleek tab bar automatically appears.
* **Tab Management**: Includes active tab accents, close buttons (`×`), middle-click tab closing, and a quick new tab (`+`) button.
* **Smart Single-Instance & Multi-Window**: Opening files from Windows Explorer forwards them into tabs in the active window (`WM_COPYDATA`). Press `Ctrl+Shift+N` (or pass `--new-window`) to spawn a completely independent window instance.

### 🪟 Windows 10/11 Native Integration
* **Immersive Dark & Light Themes**: Seamless title bar theming (`DWMWA_USE_IMMERSIVE_DARK_MODE`), custom dark menu bar (Win32 UAH rendering), dark popup menus (`uxtheme.dll`), and custom dark status bar. Follows system theme changes automatically.
* **Recent Files (MRU) & Jump Lists**: Quick access to recent files via `File -> Open Recent` with keyboard mnemonics (`&1`–`&9`) and synchronized Windows Taskbar Jump Lists (`SHAddToRecentDocs`). Lazy disk validation ensures zero startup overhead.
* **PerMonitorV2 High-DPI**: Crisp rendering across multiple displays with varying scaling factors.

### 🛡️ Robust File & Encoding Engine
* **Universal Encoding Detection**: Automatically detects and preserves **UTF-8** (with and without BOM), **UTF-16 LE**, **UTF-16 BE**, and **ANSI** (Windows system codepage).
* **Line Ending Preservation**: Detects and retains **CRLF** vs. **LF** line endings.
* **Atomic Safe Saving**: Writes to a temporary swap file before replacing the target, protecting your data against write interruptions or crashes.

### 🔤 Typography & Native Printing
* **System Font Picker**: Customize font family, size, weight, and italic options (`View -> Font...`) with single-click **"Restore Default Font"** (`Cascadia Mono` $\rightarrow$ `Consolas` $\rightarrow$ `Courier New`).
* **Native Document Printing**: Print directly via `File -> Print...` (`Ctrl+P`) using the native Windows Print dialog (`PrintDlgW`) with full pagination, margins, word wrapping, headers (file name), and footers (page numbers). Renders crisp black text on white paper regardless of active theme.

---

## 📂 Supported Formats & Syntax Colorization

LightNotePad uses a single unified source of truth for text file extensions, powering Open/Save dialog filters, multi-file selection, shell context menus, Windows Default Apps registration, and real-time syntax colorization:

| Category | File Extensions | Wildcard Pattern | Syntax Colorization Engine |
| :--- | :--- | :--- | :--- |
| **Plain Text & Logs** | `.txt`, `.text`, `.log`, `.out`, `.err` | `*.txt;*.text;*.log;*.out;*.err` | **None** (Strict `TM_PLAINTEXT` mode, 0% overhead) |
| **C / C++ & C#** | `.c`, `.cpp`, `.h`, `.hpp`, `.cs` | `*.c;*.cpp;*.h;*.hpp;*.cs` | **C-Family** (Keywords, types, strings, comments, `#preprocessor`) |
| **JavaScript & TypeScript** | `.js`, `.ts`, `.jsx`, `.tsx` | `*.js;*.ts` | **JS/TS** (Keywords, types, strings, comments, template literals) |
| **Python** | `.py`, `.pyw` | `*.py` | **Python** (Keywords, types, strings, `#comments`, `@decorators`) |
| **JSON** | `.json` | `*.json` | **JSON** (Keys, values, strings, numbers, booleans, null) |
| **Markup & Web** | `.html`, `.htm`, `.xml`, `.svg`, `.css` | `*.html;*.htm;*.xml;*.css` | **HTML/XML** (Tags, attributes, comments, strings) |
| **Configuration & Data** | `.ini`, `.cfg`, `.conf`, `.config`, `.inf`, `.properties`, `.env`, `.yaml`, `.yml`, `.toml` | `*.ini;*.cfg;*.conf;*.config;*.inf;*.properties;*.env;*.yaml;*.yml;*.toml` | **Config/Data** (`[Sections]`, key-value pairs, comments) |
| **Scripts & SQL** | `.ps1`, `.bat`, `.cmd`, `.sh`, `.sql` | `*.ps1;*.bat;*.cmd;*.sh;*.sql` | **Scripts** (Case-insensitive keywords, `$vars`, comments) |
| **Markdown Documents** | `.md`, `.markdown` | `*.md;*.markdown` | **Markdown** (`# Headings`, inline `` `code` ``, blockquotes) |
| **Tabular Data** | `.csv`, `.tsv` | `*.csv;*.tsv` | **None** (Strict `TM_PLAINTEXT` mode) |
| **All Files** | Any extension (`*.*`) | `*.*` | Auto-detected by extension (fallback to plain text) |

---

## 💻 Windows Shell Integration & Command Line Usage

LightNotePad integrates seamlessly with the Windows shell without requiring administrative privileges:

```cmd
:: Launch LightNotePad
LightNotePad.exe

:: Open a specific file
LightNotePad.exe "C:\path\to\file.txt"

:: Open multiple files simultaneously into tabs
LightNotePad.exe file1.txt file2.log config.ini data.json

:: Open an independent window instance
LightNotePad.exe --new-window

:: Register shell associations in user profile (zero admin rights required)
:: Sets up ProgID (LightNotePad.Document), OpenWithProgids, Applications\LightNotePad.exe,
:: and Windows Default Apps Capabilities (Software\LightNotePad\Capabilities & RegisteredApplications).
LightNotePad.exe /register

:: Register silently (ideal for scripts and unattended setups)
LightNotePad.exe /register /silent

:: Remove all registered shell associations, ProgID, and Capabilities cleanly
LightNotePad.exe /unregister

:: Remove associations silently
LightNotePad.exe /unregister /silent
```

> **Note on Windows Default Apps**: Per Windows 10/11 security architecture, running `/register` publishes the app's capabilities to Windows. To set LightNotePad as your default handler for specific extensions, select it in **Windows Settings > Apps > Default apps**, or right-click any supported file and choose **"Open with" > "Choose another app" > "Always"**.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + N` / `Ctrl + T` | New Tab |
| `Ctrl + Shift + N` | New Window (independent instance) |
| `Ctrl + O` | Open File (supports multi-file selection) |
| `Ctrl + S` | Save |
| `Ctrl + Shift + S` | Save As |
| `Ctrl + P` | Print Document |
| `Ctrl + W` | Close Tab |
| `Ctrl + Tab` / `Ctrl + Shift + Tab` | Next / Previous Tab |
| `Ctrl + F` | Find |
| `F3` / `Shift + F3` | Find Next / Find Previous |
| `Ctrl + H` | Replace |
| `Ctrl + Z` / `Ctrl + Y` | Undo / Redo |
| `Alt + Z` | Toggle Word Wrap |
| `Ctrl + +` / `Ctrl + -` | Zoom In / Zoom Out |
| `Ctrl + 0` | Reset Zoom (100%) |
| `Ctrl + Mouse Wheel` | Smooth Zoom |
| `F5` | Insert Date and Time |
| `F1` | Keyboard Shortcuts Cheat Sheet |

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
