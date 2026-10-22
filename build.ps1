param(
    [switch]$Rebuild,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Building LightNotePad (Lightning-Fast Text Editor)      " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Handle Clean
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path "bin\obj") { Remove-Item -Path "bin\obj" -Recurse -Force }
    if (Test-Path "bin\LightNotePad.exe") { Remove-Item -Path "bin\LightNotePad.exe" -Force }
    if (Test-Path "resources\app.res") { Remove-Item -Path "resources\app.res" -Force }
    Write-Host "Clean completed successfully." -ForegroundColor Green
    exit 0
}

# 2. Handle Rebuild
if ($Rebuild) {
    Write-Host "Full rebuild requested. Purging bin\obj..." -ForegroundColor Yellow
    if (Test-Path "bin\obj") { Remove-Item -Path "bin\obj" -Recurse -Force }
    if (Test-Path "resources\app.res") { Remove-Item -Path "resources\app.res" -Force }
}

# 3. Ensure required directories
if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Path "bin" | Out-Null }
if (-not (Test-Path "bin\obj")) { New-Item -ItemType Directory -Path "bin\obj" -Force | Out-Null }

# 4. Activate MSVC
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $msvcCandidate = $null
    if (Test-Path "tools\msvc\activate.ps1") {
        $msvcCandidate = "$PSScriptRoot\tools\msvc\activate.ps1"
    } elseif (Test-Path "$PSScriptRoot\..\LightPDF\tools\msvc\activate.ps1") {
        $msvcCandidate = "$PSScriptRoot\..\LightPDF\tools\msvc\activate.ps1"
    }

    if ($msvcCandidate) {
        Write-Host "Activating portable MSVC from $msvcCandidate..." -ForegroundColor Yellow
        . $msvcCandidate
    } else {
        # Check standard Visual Studio installation via vswhere
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $activated = $false
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) {
                $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
                if (Test-Path $vcvars) {
                    Write-Host "Activating Visual Studio environment via $vcvars..." -ForegroundColor Yellow
                    & cmd.exe /c "call `"$vcvars`" && set" | ForEach-Object {
                        if ($_ -match "=(.*)") {
                            $var = $_.Substring(0, $_.IndexOf('='))
                            $val = $_.Substring($_.IndexOf('=') + 1)
                            [System.Environment]::SetEnvironmentVariable($var, $val)
                        }
                    }
                    $activated = $true
                }
            }
        }
        if (-not $activated -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            Write-Error "cl.exe not found and neither portable MSVC nor Visual Studio installation was detected!"
        }
    }
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

# 5. Compile Resource Script (app.rc)
$rcInputs = @(
    "resources\app.rc",
    "resources\app.ico"
)
$resFile = "resources\app.res"
$needRc = -not (Test-Path $resFile)

if (-not $needRc) {
    $resTime = (Get-Item $resFile).LastWriteTimeUtc
    foreach ($rcIn in $rcInputs) {
        if ((Test-Path $rcIn) -and (Get-Item $rcIn).LastWriteTimeUtc -gt $resTime) {
            $needRc = $true
            break
        }
    }
}

if ($needRc) {
    Write-Host "Compiling Windows resource script (app.rc)..." -ForegroundColor Green
    & rc.exe /nologo /i resources /fo "resources\app.res" "resources\app.rc"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Resource compilation failed with exit code $LASTEXITCODE"
    }
}

# 6. Precompiled Header (PCH) Management
$pchHpp = "src\pch.hpp"
$pchCpp = "src\pch.cpp"
$pchBin = "bin\obj\pch.pch"
$pchObj = "bin\obj\pch.obj"
$pchRebuilt = $false

$needPch = (-not (Test-Path $pchBin)) -or (-not (Test-Path $pchObj))
if (-not $needPch) {
    $pchTime = (Get-Item $pchBin).LastWriteTimeUtc
    if ((Get-Item $pchHpp).LastWriteTimeUtc -gt $pchTime -or (Get-Item $pchCpp).LastWriteTimeUtc -gt $pchTime) {
        $needPch = $true
    }
}

if ($needPch) {
    Write-Host "Generating Precompiled Header (pch.pch)..." -ForegroundColor Yellow
    $pchArgs = @(
        "/nologo",
        "/O1",
        "/MT",
        "/std:c++20",
        "/GL",
        "/Gy",
        "/Gw",
        "/EHsc",
        "/utf-8",
        "/permissive-",
        "/DNOMINMAX",
        "/DUNICODE",
        "/D_UNICODE",
        "/DWIN32_LEAN_AND_MEAN",
        "/W4",
        "/Yc`"pch.hpp`"",
        "/Fp`"$pchBin`"",
        "/c",
        "/Fo`"$pchObj`"",
        "/Fd`"bin\obj\pch.pdb`"",
        "/I", "src"
    )
    $pchArgs += $pchCpp

    & cl.exe $pchArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "PCH generation failed with exit code $LASTEXITCODE"
    }
    $pchRebuilt = $true
}

# 7. Translation Units & Dependency Tracking
$sources = @(
    "src\main.cpp",
    "src\app_window.cpp",
    "src\editor_tab.cpp",
    "src\syntax_highlighter.cpp",
    "src\tab_bar.cpp",
    "src\file_io.cpp",
    "src\status_bar.cpp",
    "src\shortcuts_dialog.cpp"
)

# Filter sources that actually exist
$existingSources = $sources | Where-Object { Test-Path $_ }

$staleSources = [System.Collections.Generic.List[string]]::new()

foreach ($src in $existingSources) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $objPath = "bin\obj\$baseName.obj"

    if ($pchRebuilt -or (-not (Test-Path $objPath))) {
        $staleSources.Add($src)
        continue
    }

    $objTime = (Get-Item $objPath).LastWriteTimeUtc
    if ((Get-Item $src).LastWriteTimeUtc -gt $objTime) {
        $staleSources.Add($src)
        continue
    }
}

# 8. Compile stale source files concurrently using /MP and PCH
if ($staleSources.Count -gt 0) {
    Write-Host "Compiling $($staleSources.Count) source file(s) with /MP..." -ForegroundColor Green
    $clArgs = @(
        "/nologo",
        "/MP",
        "/O1",
        "/MT",
        "/std:c++20",
        "/GL",
        "/Gy",
        "/Gw",
        "/EHsc",
        "/utf-8",
        "/permissive-",
        "/DNOMINMAX",
        "/DUNICODE",
        "/D_UNICODE",
        "/DWIN32_LEAN_AND_MEAN",
        "/W4",
        "/Yu`"pch.hpp`"",
        "/FI`"pch.hpp`"",
        "/Fp`"$pchBin`"",
        "/c",
        "/Fo`"bin\obj/`"",
        "/Fd`"bin\obj/`"",
        "/I", "src"
    )
    $clArgs += $staleSources

    & cl.exe $clArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Host "All translation units up to date." -ForegroundColor DarkGray
}

# 9. Link target binary incrementally
$targetExe = "bin\LightNotePad.exe"
$needLink = -not (Test-Path $targetExe)

$allObjFiles = @($pchObj) + ($existingSources | ForEach-Object {
    "bin\obj\$([System.IO.Path]::GetFileNameWithoutExtension($_)).obj"
})
$allLinkInputs = $allObjFiles + @($resFile)

if (-not $needLink) {
    $exeTime = (Get-Item $targetExe).LastWriteTimeUtc
    foreach ($linkIn in $allLinkInputs) {
        if ((Test-Path $linkIn) -and (Get-Item $linkIn).LastWriteTimeUtc -gt $exeTime) {
            $needLink = $true
            break
        }
    }
}

if ($needLink) {
    Write-Host "Linking $targetExe with /LTCG..." -ForegroundColor Yellow
    $linkArgs = @(
        "/nologo",
        "/LTCG",
        "/OPT:REF",
        "/OPT:ICF",
        "/SUBSYSTEM:WINDOWS",
        "/MANIFEST:EMBED",
        "/MANIFESTINPUT:resources\app.manifest"
    ) + $allLinkInputs + @(
        "user32.lib",
        "kernel32.lib",
        "gdi32.lib",
        "comctl32.lib",
        "comdlg32.lib",
        "shell32.lib",
        "shlwapi.lib",
        "dwmapi.lib",
        "uxtheme.lib",
        "advapi32.lib",
        "ole32.lib",
        "/OUT:$targetExe"
    )

    & link.exe $linkArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Linking failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Host "$targetExe is up to date." -ForegroundColor DarkGray
}

$sw.Stop()
$exeSize = (Get-Item $targetExe).Length / 1KB
Write-Host ("Build finished successfully in {0:N2}s (Binary Size: {1:N1} KB)" -f ($sw.ElapsedMilliseconds / 1000.0), $exeSize) -ForegroundColor Green
