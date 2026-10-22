$ErrorActionPreference = "Stop"
$exe = "$PSScriptRoot\..\bin\LightNotePad.exe"

if (-not (Test-Path $exe)) {
    Write-Error "Executable not found at $exe"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Testing Shell Associations (/register and /unregister)  " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Register
Write-Host "1. Testing /register /silent..." -NoNewline
$pReg = Start-Process -FilePath $exe -ArgumentList "/register /silent" -Wait -PassThru

# Assert representative extensions: standard, alias, structured data, script
$txtProp      = Get-ItemProperty -Path "HKCU:\Software\Classes\.txt\OpenWithProgids" -ErrorAction SilentlyContinue
$markdownProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.markdown\OpenWithProgids" -ErrorAction SilentlyContinue
$tomlProp     = Get-ItemProperty -Path "HKCU:\Software\Classes\.toml\OpenWithProgids" -ErrorAction SilentlyContinue
$ps1Prop      = Get-ItemProperty -Path "HKCU:\Software\Classes\.ps1\OpenWithProgids" -ErrorAction SilentlyContinue

$progIdKey    = Test-Path "HKCU:\Software\Classes\LightNotePad.Document\shell\open\command"
$appKey       = Test-Path "HKCU:\Software\Classes\Applications\LightNotePad.exe\shell\open\command"
$capsKey      = Test-Path "HKCU:\Software\LightNotePad\Capabilities\FileAssociations"
$regApp       = Get-ItemProperty -Path "HKCU:\Software\RegisteredApplications" -ErrorAction SilentlyContinue

$isReg = ($null -ne $txtProp) -and ($null -ne $txtProp."LightNotePad.Document") -and
         ($null -ne $markdownProp) -and ($null -ne $markdownProp."LightNotePad.Document") -and
         ($null -ne $tomlProp) -and ($null -ne $tomlProp."LightNotePad.Document") -and
         ($null -ne $ps1Prop) -and ($null -ne $ps1Prop."LightNotePad.Document") -and
         $progIdKey -and $appKey -and $capsKey -and ($null -ne $regApp.LightNotePad)

if ($isReg) {
    Write-Host " OK (All ProgIDs, Applications key, Capabilities, and RegisteredApplications verified)" -ForegroundColor Green
} else {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}

# 2. Unregister
Write-Host "2. Testing /unregister /silent..." -NoNewline
$pUnreg = Start-Process -FilePath $exe -ArgumentList "/unregister /silent" -Wait -PassThru

$unregTxt      = Get-ItemProperty -Path "HKCU:\Software\Classes\.txt\OpenWithProgids" -ErrorAction SilentlyContinue
$unregMarkdown = Get-ItemProperty -Path "HKCU:\Software\Classes\.markdown\OpenWithProgids" -ErrorAction SilentlyContinue
$unregToml     = Get-ItemProperty -Path "HKCU:\Software\Classes\.toml\OpenWithProgids" -ErrorAction SilentlyContinue
$unregPs1      = Get-ItemProperty -Path "HKCU:\Software\Classes\.ps1\OpenWithProgids" -ErrorAction SilentlyContinue

$regApp2       = Get-ItemProperty -Path "HKCU:\Software\RegisteredApplications" -ErrorAction SilentlyContinue

$isUnreg = (($null -eq $unregTxt) -or ($null -eq $unregTxt."LightNotePad.Document")) -and
           (($null -eq $unregMarkdown) -or ($null -eq $unregMarkdown."LightNotePad.Document")) -and
           (($null -eq $unregToml) -or ($null -eq $unregToml."LightNotePad.Document")) -and
           (($null -eq $unregPs1) -or ($null -eq $unregPs1."LightNotePad.Document")) -and
           -not (Test-Path "HKCU:\Software\Classes\LightNotePad.Document") -and
           -not (Test-Path "HKCU:\Software\Classes\Applications\LightNotePad.exe") -and
           -not (Test-Path "HKCU:\Software\LightNotePad\Capabilities") -and
           ($null -eq $regApp2.LightNotePad)

if ($isUnreg) {
    Write-Host " OK (All shell associations, Capabilities, and ProgIDs removed cleanly)" -ForegroundColor Green
} else {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "`nShell Association tests PASSED successfully!" -ForegroundColor Green
exit 0
