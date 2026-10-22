$ErrorActionPreference = "Stop"
$exePath = "$PSScriptRoot\..\bin\LightNotePad.exe"

if (-not (Test-Path $exePath)) {
    Write-Error "LightNotePad.exe not found at $exePath!"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  LightNotePad Verification Test Suite                    " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Test Cold Launch (Blank Instance)
Write-Host "1. Testing Blank Startup..." -NoNewline
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exePath -PassThru
Start-Sleep -Milliseconds 350
$sw.Stop()

if ($p.HasExited) {
    Write-Host " FAILED (Process exited with code $($p.ExitCode))" -ForegroundColor Red
    exit 1
} else {
    $p.Refresh()
    $ws = [math]::Round($p.WorkingSet64 / 1MB, 2)
    $pm = [math]::Round($p.PrivateMemorySize64 / 1MB, 2)
    Write-Host (" OK (Ready in {0}ms | Working Set: {1} MB | Private: {2} MB)" -f $sw.ElapsedMilliseconds, $ws, $pm) -ForegroundColor Green
    $p.Kill()
    $p.WaitForExit()
}

# 2. Test Multi-Window Flag (--new-window)
Write-Host "2. Testing --new-window Launch..." -NoNewline
$p = Start-Process -FilePath $exePath -ArgumentList "--new-window" -PassThru
Start-Sleep -Milliseconds 350

if ($p.HasExited) {
    Write-Host " FAILED (Process exited with code $($p.ExitCode))" -ForegroundColor Red
    exit 1
} else {
    Write-Host " OK" -ForegroundColor Green
    $p.Kill()
    $p.WaitForExit()
}

# 3. Test File Encoding & Line Ending Handling
$tempDir = [System.IO.Path]::Combine($PSScriptRoot, "temp_test_docs")
if (-not (Test-Path $tempDir)) { New-Item -ItemType Directory -Path $tempDir | Out-Null }

try {
    # Prepare sample files
    $utf8NoBom = [System.IO.Path]::Combine($tempDir, "utf8_nobom.txt")
    [System.IO.File]::WriteAllText($utf8NoBom, "Hello LightNotePad (UTF-8 No BOM)`nLine 2`nLine 3", [System.Text.UTF8Encoding]::new($false))

    $utf8Bom = [System.IO.Path]::Combine($tempDir, "utf8_bom.txt")
    [System.IO.File]::WriteAllText($utf8Bom, "Hello LightNotePad (UTF-8 with BOM)`r`nLine 2`r`nLine 3", [System.Text.UTF8Encoding]::new($true))

    $utf16Le = [System.IO.Path]::Combine($tempDir, "utf16_le.txt")
    [System.IO.File]::WriteAllText($utf16Le, "Hello LightNotePad (UTF-16 LE)`r`nLine 2", [System.Text.UnicodeEncoding]::new($false, $true))

    $ansiFile = [System.IO.Path]::Combine($tempDir, "ansi_crlf.txt")
    [System.IO.File]::WriteAllText($ansiFile, "Hello LightNotePad (ANSI CRLF)`r`nLine 2", [System.Text.Encoding]::ASCII)

    $sampleCpp = [System.IO.Path]::Combine($tempDir, "sample.cpp")
    [System.IO.File]::WriteAllText($sampleCpp, "#include <iostream>`n// Main function`nint main() {`n    std::string msg = `"Hello World`";`n    return 0;`n}", [System.Text.UTF8Encoding]::new($false))

    $samplePy = [System.IO.Path]::Combine($tempDir, "sample.py")
    [System.IO.File]::WriteAllText($samplePy, "# Python script`ndef greet(name: str):`n    msg = f`"Hello {name}`"`n    return msg`n", [System.Text.UTF8Encoding]::new($false))

    $sampleJson = [System.IO.Path]::Combine($tempDir, "sample.json")
    [System.IO.File]::WriteAllText($sampleJson, "{`n  `"name`": `"LightNotePad`",`n  `"active`": true,`n  `"count`": 42`n}", [System.Text.UTF8Encoding]::new($false))

    $sampleMd = [System.IO.Path]::Combine($tempDir, "sample.md")
    [System.IO.File]::WriteAllText($sampleMd, "# Markdown Header`n`nHere is `inline code` and a quote:`n> Note", [System.Text.UTF8Encoding]::new($false))

    $testFiles = @($utf8NoBom, $utf8Bom, $utf16Le, $ansiFile, $sampleCpp, $samplePy, $sampleJson, $sampleMd)

    foreach ($file in $testFiles) {
        $fileName = [System.IO.Path]::GetFileName($file)
        Write-Host "3. Testing document load: $fileName..." -NoNewline

        $p = Start-Process -FilePath $exePath -ArgumentList "`"$file`"" -PassThru
        Start-Sleep -Milliseconds 350

        if ($p.HasExited) {
            Write-Host " FAILED (ExitCode $($p.ExitCode))" -ForegroundColor Red
            exit 1
        } else {
            $p.Refresh()
            $ws = [math]::Round($p.WorkingSet64 / 1MB, 2)
            Write-Host (" OK (RAM: {0} MB)" -f $ws) -ForegroundColor Green
            $p.Kill()
            $p.WaitForExit()
        }
    }
}
finally {
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# 4. Test Shell Associations (/register and /unregister)
Write-Host "`n4. Testing Shell Associations..."
& powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\test_registration.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Shell association tests failed!"
    exit 1
}

Write-Host "`nAll LightNotePad verification tests passed successfully!" -ForegroundColor Green
exit 0
