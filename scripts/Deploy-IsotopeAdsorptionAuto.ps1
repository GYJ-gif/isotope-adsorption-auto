param(
    [string]$DestinationRoot = "D:\calculate\Auto",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$skillRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$sourceRoot = Join-Path $skillRoot "assets\auto-workflow"

if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
    throw "Packaged workflow assets were not found: $sourceRoot"
}

if ((Test-Path -LiteralPath $DestinationRoot) -and -not $Force) {
    Write-Host "Workflow already exists: $DestinationRoot"
    Write-Host "Use -Force to overwrite files from the packaged skill."
    exit 0
}

New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
robocopy $sourceRoot $DestinationRoot /E /XD ".tmp" "__pycache__" "logs" /XF "*.pyc" | Out-Host
if ($LASTEXITCODE -gt 7) {
    throw "Robocopy failed with exit code $LASTEXITCODE"
}

Write-Host "Deployed isotope adsorption workflow to: $DestinationRoot"
