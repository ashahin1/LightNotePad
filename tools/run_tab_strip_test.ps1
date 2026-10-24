$ErrorActionPreference = "Stop"

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    . C:\AntiGravity_Projects\LightPDF\tools\msvc\activate.ps1
}

cl.exe /nologo /std:c++20 /EHsc /O2 /W4 /WX- /I src tools\test_tab_strip.cpp /Febin\test_tab_strip.exe

if (Test-Path "bin\test_tab_strip.exe") {
    & "bin\test_tab_strip.exe"
}
