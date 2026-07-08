param(
    [Parameter(Mandatory = $true)]
    [string]$InputRoot,

    [string[]]$Samples = @(),
    [switch]$PrepareRawInput,
    [string]$PreparedOutputRoot,
    [switch]$SkipCalculate,
    [switch]$SkipOrigin,
    [string]$WorkflowRoot
)

$ErrorActionPreference = "Stop"
$skillRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

if (-not $WorkflowRoot) {
    $WorkflowRoot = Join-Path $skillRoot "assets\auto-workflow"
}

$runner = Join-Path $WorkflowRoot "run_full_workflow.ps1"
if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
    throw "Workflow runner not found: $runner. Run Deploy-IsotopeAdsorptionAuto.ps1 or pass -WorkflowRoot."
}

$arguments = @("-InputRoot", $InputRoot)
if ($Samples.Count -gt 0) { $arguments += @("-Samples", $Samples) }
if ($PrepareRawInput) { $arguments += "-PrepareRawInput" }
if ($PreparedOutputRoot) { $arguments += @("-PreparedOutputRoot", $PreparedOutputRoot) }
if ($SkipCalculate) { $arguments += "-SkipCalculate" }
if ($SkipOrigin) { $arguments += "-SkipOrigin" }

& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $runner @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Isotope adsorption workflow failed with exit code $LASTEXITCODE"
}
