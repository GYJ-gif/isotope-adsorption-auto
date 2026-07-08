param(
    [Parameter(Mandatory = $true)]
    [string]$InputRoot,

    [string]$OutputRoot,

    [string]$PythonPath
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $root "prepare_ads_workbooks.py"

if (-not $PythonPath) {
    $bundledPython = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
    if (Test-Path -LiteralPath $bundledPython -PathType Leaf) {
        $PythonPath = $bundledPython
    } else {
        $PythonPath = "python"
    }
}

$arguments = @("--input-root", $InputRoot)
if ($OutputRoot) {
    $arguments += @("--output-root", $OutputRoot)
}

& $PythonPath $script @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Prepare step failed with exit code: $LASTEXITCODE"
}
