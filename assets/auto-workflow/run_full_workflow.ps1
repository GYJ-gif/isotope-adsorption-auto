param(
    [string]$InputRoot = "D:\calculate\260706",
    [string[]]$Samples = @(),
    [switch]$PrepareRawInput,
    [string]$PreparedOutputRoot,
    [switch]$SkipCalculate,
    [switch]$SkipOrigin
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$config = Get-Content -LiteralPath (Join-Path $root "workflow_config.json") -Raw -Encoding UTF8 | ConvertFrom-Json

if ($PrepareRawInput) {
    $prepareScript = Join-Path $root "run_prepare_ads_workbooks.ps1"
    $prepareArguments = @("-InputRoot", $InputRoot)
    if ($PreparedOutputRoot) {
        $prepareArguments += @("-OutputRoot", $PreparedOutputRoot)
    }
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $prepareScript @prepareArguments
    if ($LASTEXITCODE -ne 0) { throw "Prepare step failed with exit code: $LASTEXITCODE" }
    if ($PreparedOutputRoot) {
        $InputRoot = $PreparedOutputRoot
    } else {
        $InputRoot = Join-Path (Split-Path -Parent $InputRoot) (Get-Date -Format "yyMMdd")
    }
}

if ($Samples.Count -eq 0) {
    if ($PrepareRawInput) {
        $generatedSamplesPath = Join-Path $InputRoot "_prepare_logs\generated_samples.txt"
        if (Test-Path -LiteralPath $generatedSamplesPath -PathType Leaf) {
            $Samples = @(Get-Content -LiteralPath $generatedSamplesPath -Encoding UTF8 | Where-Object { $_ })
        } else {
            $Samples = @(Get-ChildItem -LiteralPath $InputRoot -Directory | Where-Object { $_.Name -ne "_prepare_logs" } | ForEach-Object { $_.Name })
        }
    } else {
        $Samples = @($config.samplesDefault)
    }
}

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = [System.Collections.Generic.List[string]]::new()

$iastScript = Join-Path $root "IAST_Auto_Template\run_iast_from_excel.ps1"
$iastExe = Join-Path $root "IAST_Auto_Template\iast_calc_gui.exe"
$qstExe = Join-Path $root "Qst_Auto_Template\qst_calc_cli.exe"
$originScript = Join-Path $root "Origin_Auto_Template\run_origin_workflow.ps1"

foreach ($sample in $Samples) {
    $sampleDir = Join-Path $InputRoot $sample
    $xlsx = Join-Path $sampleDir "$sample.xlsx"

    if (-not (Test-Path -LiteralPath $xlsx)) {
        $log.Add("$sample`tERROR`tWorkbook not found: $xlsx")
        continue
    }

    if (-not $SkipCalculate) {
        try {
            & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $iastScript `
                -ExcelPath $xlsx `
                -Mode All `
                -IastExe $iastExe `
                -QstExe $qstExe
            if ($LASTEXITCODE -ne 0) { throw "IAST/Qst calculation failed with exit code $LASTEXITCODE" }
            $log.Add("$sample`tCALC_OK`tIAST/Qst calculation completed")
        }
        catch {
            $log.Add("$sample`tCALC_ERROR`t$($_.Exception.Message)")
            continue
        }
    }

    if (-not $SkipOrigin) {
        try {
            & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $originScript `
                -InputRoot $InputRoot `
                -Samples $sample
            if ($LASTEXITCODE -ne 0) { throw "Origin plotting failed with exit code $LASTEXITCODE" }
            $log.Add("$sample`tORIGIN_OK`tOrigin plotting completed")
        }
        catch {
            $log.Add("$sample`tORIGIN_ERROR`t$($_.Exception.Message)")
        }
    }
}

$report = Join-Path $logDir "full_workflow_report.txt"
$log | Set-Content -LiteralPath $report -Encoding UTF8
Write-Host "Full workflow finished."
Write-Host "Report: $report"
