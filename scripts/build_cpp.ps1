# Build IAST/Qst command-line tools, unit tests, and the Windows GUI programs.
param(
    [string]$OutDir = "",
    [switch]$SkipGui
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $root "build" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio 2022 Build Tools with the C++ workload."
}
$vs = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "MSVC C++ tools were not found." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

$iast = Join-Path $root "assets\auto-workflow\IAST_Auto_Template\source"
$qst = Join-Path $root "assets\auto-workflow\Qst_Auto_Template\source"

function Invoke-Cl([string]$WorkDir, [string]$Arguments) {
    $command = "call `"$vcvars`" >nul && cd /d `"$WorkDir`" && cl /nologo /O2 /EHsc /std:c++17 /utf-8 $Arguments"
    cmd /c $command
    if ($LASTEXITCODE -ne 0) { throw "cl failed: $Arguments" }
}

$obj = "/Fo:`"$OutDir\\`""
Invoke-Cl $iast "$obj /Fe:`"$OutDir\iast_auto_fit_test.exe`" iast_auto_fit_test.cpp"
Invoke-Cl $iast "$obj /Fe:`"$OutDir\iast_calc.exe`" iast_cli.cpp"
Invoke-Cl $qst "$obj /Fe:`"$OutDir\qst_auto_fit_test.exe`" qst_auto_fit_test.cpp"
Invoke-Cl $qst "$obj /Fe:`"$OutDir\qst_calc.exe`" qst_cli.cpp"

if (-not $SkipGui) {
    Invoke-Cl $iast "$obj /Fe:`"$OutDir\iast_calc_gui.exe`" iast_gui.cpp /link gdiplus.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib shell32.lib"
    Invoke-Cl $qst "$obj /Fe:`"$OutDir\qst_calc_gui.exe`" qst_gui.cpp /link gdiplus.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib"
    Copy-Item -LiteralPath (Join-Path $OutDir "iast_calc_gui.exe") -Destination (Join-Path $iast "..\iast_calc_gui.exe") -Force
    Copy-Item -LiteralPath (Join-Path $OutDir "qst_calc.exe") -Destination (Join-Path $qst "..\qst_calc_cli.exe") -Force
    Copy-Item -LiteralPath (Join-Path $OutDir "qst_calc_gui.exe") -Destination (Join-Path $qst "..\qst_calc_gui.exe") -Force
}

Write-Host "Built C++ programs in $OutDir"
