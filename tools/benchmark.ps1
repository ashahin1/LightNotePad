# =====================================================================
# LightNotePad Benchmark Suite
# Measures: Cold Launch Time (ms) & Memory Footprint (Working Set / Private)
# =====================================================================

$ErrorActionPreference = "SilentlyContinue"

$LightNotePadExe = "$PSScriptRoot\..\bin\LightNotePad.exe"
$WinNotepadExe   = "C:\Windows\System32\notepad.exe"

function Measure-AppOnce {
    param(
        [string]$Name,
        [string]$ExePath,
        [string]$FilePath = ""
    )

    if (-not (Test-Path $ExePath)) {
        return [PSCustomObject]@{
            Name = $Name
            StartupMs = 0
            WorkingSetMB = 0
            PrivateMB = 0
        }
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $p = if ([string]::IsNullOrEmpty($FilePath)) {
        [System.Diagnostics.Process]::Start($ExePath)
    } else {
        [System.Diagnostics.Process]::Start($ExePath, "`"$FilePath`"")
    }

    if (-not $p) {
        return [PSCustomObject]@{
            Name = $Name
            StartupMs = 0
            WorkingSetMB = 0
            PrivateMB = 0
        }
    }

    # Wait until main window is ready
    while ($p.MainWindowHandle -eq 0 -and $sw.ElapsedMilliseconds -lt 5000) {
        Start-Sleep -Milliseconds 5
        $p.Refresh()
    }
    $sw.Stop()
    $readyMs = $sw.ElapsedMilliseconds

    Start-Sleep -Milliseconds 600
    $p.Refresh()
    $wsMB = [math]::Round($p.WorkingSet64 / 1MB, 2)
    $privMB = [math]::Round($p.PrivateMemorySize64 / 1MB, 2)

    $p.Kill()
    Start-Sleep -Milliseconds 200

    return [PSCustomObject]@{
        Name = $Name
        StartupMs = $readyMs
        WorkingSetMB = $wsMB
        PrivateMB = $privMB
    }
}

Write-Host "Running 3 Benchmark Runs..." -ForegroundColor Cyan

$lightResults = @()
$msResults = @()

1..3 | ForEach-Object {
    Write-Host "Run $_..." -ForegroundColor Yellow
    $lightResults += (Measure-AppOnce "LightNotePad" $LightNotePadExe)
    Start-Sleep -Milliseconds 300
    $msResults += (Measure-AppOnce "Windows Notepad" $WinNotepadExe)
    Start-Sleep -Milliseconds 300
}

$lightAvgMs = [math]::Round(($lightResults | Measure-Object -Property StartupMs -Average).Average, 1)
$lightAvgWs = [math]::Round(($lightResults | Measure-Object -Property WorkingSetMB -Average).Average, 2)
$lightAvgPriv = [math]::Round(($lightResults | Measure-Object -Property PrivateMB -Average).Average, 2)

$msAvgMs = [math]::Round(($msResults | Measure-Object -Property StartupMs -Average).Average, 1)
$msAvgWs = [math]::Round(($msResults | Measure-Object -Property WorkingSetMB -Average).Average, 2)
$msAvgPriv = [math]::Round(($msResults | Measure-Object -Property PrivateMB -Average).Average, 2)

$lightExeSize = [math]::Round((Get-Item $LightNotePadExe).Length / 1KB, 1)

$table = @(
    [PSCustomObject]@{
        "Application" = "LightNotePad"
        "Binary Size" = "$lightExeSize KB"
        "Startup Time" = "$lightAvgMs ms"
        "Working Set RAM" = "$lightAvgWs MB"
        "Private Memory" = "$lightAvgPriv MB"
    },
    [PSCustomObject]@{
        "Application" = "Windows Notepad"
        "Binary Size" = "Packaged App (~50+ MB)"
        "Startup Time" = "$msAvgMs ms"
        "Working Set RAM" = "$msAvgWs MB"
        "Private Memory" = "$msAvgPriv MB"
    }
)

Write-Host "`n=== BENCHMARK SUMMARY ===" -ForegroundColor Green
$table | Format-Table -AutoSize
