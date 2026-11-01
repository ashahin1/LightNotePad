$ErrorActionPreference = "Stop"

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    . C:\AntiGravity_Projects\LightPDF\tools\msvc\activate.ps1
}

cl.exe /nologo /std:c++20 /EHsc /utf-8 /O2 /W4 /WX- /I src tools\test_editor_context_menu.cpp kernel32.lib /Febin\test_editor_context_menu.exe

if (Test-Path "bin\test_editor_context_menu.exe") {
    & "bin\test_editor_context_menu.exe"
}
