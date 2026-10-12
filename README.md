# LightNotePad

An ultra-fast, minimalist, native Win32 text editor for Windows 10 & 11.
Built with pure C++20 and portable MSVC with zero runtime DLL dependencies.

## Key Highlights

* **Instant Startup**: Launches in milliseconds without XAML/WinUI runtime overhead.
* **Microscopic Footprint**: Single standalone **221 KB** `.exe` with zero external DLLs.
* **Smart Tabs (Auto-Hiding)**:
  * When **1 file** is open: The tab bar completely hides itself, giving you the clean, full-screen canvas of classic Notepad.
  * When **2+ files** are opened: A sleek tab bar appears automatically with active tab accents, close buttons (`×`), and a new tab (`+`) button.
  * Middle-click any tab to close it.
* **Native Windows 10/11 Dark Mode**:
  * Immersive dark title bar (`DWMWA_USE_IMMERSIVE_DARK_MODE`).
  * Seamless dark palette (`#1E1E1E`), dark tab bar, and custom-rendered dark status bar.
  * Dynamically responds to Windows theme changes.
* **Typography & Fonts**:
  * Default font cascade: **Cascadia Mono** $\rightarrow$ **Consolas** $\rightarrow$ **Courier New**.
  * Full `PerMonitorV2` High-DPI awareness (never blurs across monitors).
* **Robust File & Encoding Engine**:
  * Auto-detects and preserves:
    * **UTF-8** (with and without BOM)
    * **UTF-16 LE** & **UTF-16 BE**
    * **ANSI** (Windows system codepage)
  * Auto-detects and preserves **CRLF** vs. **LF** line endings.
  * Safe atomic file saves with temporary file swaps to prevent corruption.
* **Single-Instance Tab Forwarding**:
  * Opening files from Explorer forwards them into the existing window in new tabs via `WM_COPYDATA`.
* **Zero-Admin Shell Registration**:
  * Run `LightNotePad.exe /register` to register as a text viewer in your user profile without admin rights.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + N` / `Ctrl + T` | New Tab |
| `Ctrl + Shift + N` | New Window (independent instance) |
| `Ctrl + O` | Open File |
| `Ctrl + S` | Save |
| `Ctrl + Shift + S` | Save As |
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

---

## Command Line Usage

```cmd
:: Open a blank document
LightNotePad.exe

:: Open a specific file
LightNotePad.exe "C:\path\to\file.txt"

:: Open multiple files into tabs
LightNotePad.exe file1.txt file2.log config.ini

:: Register as default text editor (no admin rights needed)
LightNotePad.exe /register

:: Remove file associations
LightNotePad.exe /unregister
```

---

## Building from Source

Build using the included batch script:
```cmd
build.bat
```
To clean or rebuild:
```cmd
build.bat /clean
build.bat /rebuild
```
The build script automatically detects portable MSVC and compiles with Link-Time Code Generation (`/LTCG /OPT:REF /OPT:ICF`) and embedded `PerMonitorV2` manifest.
