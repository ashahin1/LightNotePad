$ErrorActionPreference = "Stop"

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    . C:\AntiGravity_Projects\LightPDF\tools\msvc\activate.ps1
}

cl.exe /nologo /std:c++20 /EHsc /O2 /W4 /WX- /I src tools\test_dirty.cpp src\editor_tab.cpp src\file_io.cpp src\syntax_highlighter.cpp user32.lib gdi32.lib comctl32.lib ole32.lib oleaut32.lib uxtheme.lib /Febin\test_dirty.exe

if (Test-Path "bin\test_dirty.exe") {
    & "bin\test_dirty.exe"
}
