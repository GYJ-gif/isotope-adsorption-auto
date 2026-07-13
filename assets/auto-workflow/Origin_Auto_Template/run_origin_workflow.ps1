param(
    [Parameter(Mandatory = $true)]
    [string]$InputRoot,
    [string]$ConfigPath,
    [string[]]$Samples = @(),
    [switch]$NoPng
)

$ErrorActionPreference = "Stop"
if (-not $ConfigPath) { $ConfigPath = Join-Path $PSScriptRoot "workflow_config.json" }
$InputRoot = [System.IO.Path]::GetFullPath($InputRoot)
$ConfigPath = [System.IO.Path]::GetFullPath($ConfigPath)

function Is-Number($v) {
    if ($null -eq $v) { return $false }
    return ($v -is [byte] -or $v -is [int16] -or $v -is [int32] -or $v -is [int64] -or $v -is [single] -or $v -is [double] -or $v -is [decimal])
}

function New-Table([int]$rows, [int]$cols) {
    $a = New-Object 'object[,]' $rows, $cols
    for ($r = 0; $r -lt $rows; $r++) {
        for ($c = 0; $c -lt $cols; $c++) { $a.SetValue('', $r, $c) }
    }
    return ,$a
}

function Find-TitleRow($ws, [int]$col, [string]$kind, [int]$lastRow) {
    for ($r = 1; $r -le $lastRow; $r++) {
        $txt = [string]$ws.Cells.Item($r, $col).Text
        if ($txt -match $kind) { return $r }
    }
    return 0
}

function Read-Pairs-Between($ws, [int]$c1, [int]$c2, [int]$startRow, [int]$endRow) {
    $pairs = [System.Collections.Generic.List[object[]]]::new()
    if ($startRow -le 0 -or $endRow -lt $startRow) { return $pairs.ToArray() }
    for ($r = $startRow; $r -le $endRow; $r++) {
        $v1 = $ws.Cells.Item($r, $c1).Value2
        $v2 = $ws.Cells.Item($r, $c2).Value2
        if ((Is-Number $v1) -and (Is-Number $v2)) { $pairs.Add(@([double]$v1, [double]$v2)) }
    }
    return $pairs.ToArray()
}

function Read-AdsDes($ws, [int]$c1, [int]$c2, [int]$lastRow) {
    $adsTitle = Find-TitleRow $ws $c1 'Adsorption' $lastRow
    $desTitle = Find-TitleRow $ws $c1 'Desorption' $lastRow
    $adsEnd = if ($desTitle -gt 0) { $desTitle - 1 } else { $lastRow }
    $ads = Read-Pairs-Between $ws $c1 $c2 ($adsTitle + 1) $adsEnd
    $des = if ($desTitle -gt 0) { Read-Pairs-Between $ws $c1 $c2 ($desTitle + 1) $lastRow } else { @() }
    return @($ads, $des)
}

function Read-QstPairs($ws, [int]$c1, [int]$c2) {
    $pairs = [System.Collections.Generic.List[object[]]]::new()
    $hasNegative = $false
    for ($r = 3; $r -le 43; $r++) {
        $x = $ws.Cells.Item($r, $c1).Value2
        $y = $ws.Cells.Item($r, $c2).Value2
        if ((Is-Number $x) -and (Is-Number $y)) {
            if ([double]$y -lt 0) { $hasNegative = $true }
            $pairs.Add(@([double]$x, [double]$y))
        }
    }
    return [pscustomobject]@{ Rows = $pairs.ToArray(); HasNegative = $hasNegative }
}

function Read-MultiCols($ws, [int[]]$cols, [int]$r1, [int]$r2) {
    $rows = [System.Collections.Generic.List[object[]]]::new()
    for ($r = $r1; $r -le $r2; $r++) {
        $vals = [System.Collections.Generic.List[object]]::new()
        $ok = $true
        foreach ($c in $cols) {
            $v = $ws.Cells.Item($r, $c).Value2
            if (-not (Is-Number $v)) { $ok = $false; break }
            $vals.Add([double]$v)
        }
        if ($ok) { $rows.Add($vals.ToArray()) }
    }
    return $rows.ToArray()
}

function Put-Pairs([ref]$tableRef, [int]$startCol, [object[]]$pairs) {
    $table = $tableRef.Value
    for ($r = 0; $r -lt $pairs.Count; $r++) {
        $table.SetValue([double]$pairs[$r][0], $r, $startCol)
        $table.SetValue([double]$pairs[$r][1], $r, $startCol + 1)
    }
}

function Put-MultiCols([ref]$tableRef, [int]$startCol, [object[]]$rows) {
    $table = $tableRef.Value
    for ($r = 0; $r -lt $rows.Count; $r++) {
        for ($c = 0; $c -lt $rows[$r].Count; $c++) {
            $table.SetValue([double]$rows[$r][$c], $r, $startCol + $c)
        }
    }
}

function Get-PairBounds([object[]]$PairSets) {
    $xs = [System.Collections.Generic.List[double]]::new()
    $ys = [System.Collections.Generic.List[double]]::new()
    foreach ($pairs in $PairSets) {
        foreach ($pair in @($pairs)) {
            if ($null -ne $pair -and $pair.Count -ge 2) {
                $xs.Add([double]$pair[0]); $ys.Add([double]$pair[1])
            }
        }
    }
    if ($xs.Count -eq 0) { return $null }
    return [pscustomobject]@{
        XMin = ($xs | Measure-Object -Minimum).Minimum; XMax = ($xs | Measure-Object -Maximum).Maximum
        YMin = ($ys | Measure-Object -Minimum).Minimum; YMax = ($ys | Measure-Object -Maximum).Maximum
    }
}

function Get-MultiColumnBounds([object[]]$Rows, [int]$XIndex, [int]$YIndex) {
    $pairs = foreach ($row in @($Rows)) {
        if ($row.Count -gt [Math]::Max($XIndex, $YIndex)) { ,@([double]$row[$XIndex], [double]$row[$YIndex]) }
    }
    return Get-PairBounds @($pairs)
}

function Get-AxisCommand($Bounds, $AxisConfig, [string]$GraphName, $Warnings) {
    if ($null -eq $Bounds) { return 'layer -a;' }
    $conflict = $Bounds.XMin -lt [double]$AxisConfig.xFrom -or $Bounds.XMax -gt [double]$AxisConfig.xTo -or
        $Bounds.YMin -lt [double]$AxisConfig.yFrom -or $Bounds.YMax -gt [double]$AxisConfig.yTo
    if ($conflict) {
        $Warnings.Add("$GraphName data exceed configured axes; autoscale used to show all points.")
        return 'layer -a;'
    }
    return "layer.x.from=$($AxisConfig.xFrom); layer.x.to=$($AxisConfig.xTo); layer.y.from=$($AxisConfig.yFrom); layer.y.to=$($AxisConfig.yTo);"
}

function As-IntArray($v) { return @($v | ForEach-Object { [int]$_ }) }
function Escape-LT([string]$s) { return $s.Replace('"', '\"') }
function Get-WorksheetByAliases($workbook, [string[]]$names) {
    foreach ($name in $names) {
        try { return $workbook.Worksheets.Item($name) } catch {}
    }
    throw "找不到工作表：$($names -join ', ')"
}

function Assert-OriginOutputs([string]$SampleDir, [string]$Sample, [string]$OpjuPath, [bool]$DoPng, [bool]$SkipQst) {
    if (-not (Test-Path -LiteralPath $OpjuPath -PathType Leaf)) {
        throw "Origin 未生成工程文件：$OpjuPath"
    }
    if (-not $DoPng) { return }

    $expectedPngs = @(
        (Join-Path $SampleDir "${Sample}_HD.png"),
        (Join-Path $SampleDir "${Sample}_IAST.png")
    )
    if (-not $SkipQst) {
        $expectedPngs += (Join-Path $SampleDir "${Sample}_Qst.png")
    }
    foreach ($pngPath in $expectedPngs) {
        if (-not (Test-Path -LiteralPath $pngPath -PathType Leaf)) {
            throw "Origin 未生成预期 PNG：$pngPath"
        }
    }
}

$config = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
$moduleRoot = Split-Path -Parent ([System.IO.Path]::GetFullPath($ConfigPath))
$template = if ([System.IO.Path]::IsPathRooted($config.templateOpju)) { $config.templateOpju } else { Join-Path $moduleRoot $config.templateOpju }
if ($Samples.Count -eq 0) { $Samples = @($config.samplesDefault) }

$logDir = Join-Path $moduleRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$report = [System.Collections.Generic.List[object]]::new()

$excel = New-Object -ComObject Excel.Application
$excel.Visible = $false
$excel.DisplayAlerts = $false
$origin = New-Object -ComObject Origin.ApplicationSI
$origin.Visible = 0

try {
    foreach ($sample in $Samples) {
        $sampleDir = Join-Path $InputRoot $sample
        $xlsx = Join-Path $sampleDir "$sample.xlsx"
        $out = Join-Path $sampleDir "$sample.opju"
        $warnings = [System.Collections.Generic.List[string]]::new()
        $status = "OK"

        try {
            $wb = $excel.Workbooks.Open($xlsx)
            try {
                $adsWs = Get-WorksheetByAliases $wb @($config.excelSheets.adsorption, '吸附数据', 'ads-des', 'ads/des', '簾現方象')
                $qstWs = Get-WorksheetByAliases $wb @($config.excelSheets.qst, 'Qst结果', 'Qst', 'Qst潤惚')
                $iastWs = Get-WorksheetByAliases $wb @($config.excelSheets.iast, 'IAST结果', 'IAST', 'IAST潤惚')
                $last = $adsWs.UsedRange.Rows.Count

                $h277 = Read-AdsDes $adsWs (As-IntArray $config.adsorptionColumns.H2_77K)[0] (As-IntArray $config.adsorptionColumns.H2_77K)[1] $last
                $d277 = Read-AdsDes $adsWs (As-IntArray $config.adsorptionColumns.D2_77K)[0] (As-IntArray $config.adsorptionColumns.D2_77K)[1] $last
                $h287 = Read-AdsDes $adsWs (As-IntArray $config.adsorptionColumns.H2_87K)[0] (As-IntArray $config.adsorptionColumns.H2_87K)[1] $last
                $d287 = Read-AdsDes $adsWs (As-IntArray $config.adsorptionColumns.D2_87K)[0] (As-IntArray $config.adsorptionColumns.D2_87K)[1] $last

                $h2q = Read-QstPairs $qstWs (As-IntArray $config.qstColumns.H2)[0] (As-IntArray $config.qstColumns.H2)[1]
                $d2q = Read-QstPairs $qstWs (As-IntArray $config.qstColumns.D2)[0] (As-IntArray $config.qstColumns.D2)[1]
                $skipQst = $config.qst.skipPlotWhenNegative -and ($h2q.HasNegative -or $d2q.HasNegative)
                if ($skipQst) { $warnings.Add("Qst contains negative values; Qst plot/export skipped.") }

                $iast77 = Read-MultiCols $iastWs (As-IntArray $config.iastColumns."77K") 3 22
                $iast87 = Read-MultiCols $iastWs (As-IntArray $config.iastColumns."87K") 3 22
                $hdBounds = Get-PairBounds @($h277[0],$h277[1],$d277[0],$d277[1],$h287[0],$h287[1],$d287[0],$d287[1])
                $iastBounds77 = Get-MultiColumnBounds $iast77 0 5
                $iastBounds87 = Get-MultiColumnBounds $iast87 0 5
                $iastBounds = Get-PairBounds @(
                    $(if ($iastBounds77) { @(@($iastBounds77.XMin,$iastBounds77.YMin),@($iastBounds77.XMax,$iastBounds77.YMax)) }),
                    $(if ($iastBounds87) { @(@($iastBounds87.XMin,$iastBounds87.YMin),@($iastBounds87.XMax,$iastBounds87.YMax)) })
                )
            }
            finally { $wb.Close($false) }

            $s1Rows = @($h277[0].Count,$h277[1].Count,$d277[0].Count,$d277[1].Count,$h287[0].Count,$h287[1].Count,$d287[0].Count,$d287[1].Count) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
            $s1 = New-Table $s1Rows 16
            Put-Pairs ([ref]$s1) 0 $h277[0]; Put-Pairs ([ref]$s1) 2 $h277[1]
            Put-Pairs ([ref]$s1) 4 $d277[0]; Put-Pairs ([ref]$s1) 6 $d277[1]
            Put-Pairs ([ref]$s1) 8 $h287[0]; Put-Pairs ([ref]$s1) 10 $h287[1]
            Put-Pairs ([ref]$s1) 12 $d287[0]; Put-Pairs ([ref]$s1) 14 $d287[1]

            $s2Rows = @($d2q.Rows.Count,$h2q.Rows.Count,1) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
            $s2 = New-Table $s2Rows 4
            if (-not $skipQst) { Put-Pairs ([ref]$s2) 0 $d2q.Rows; Put-Pairs ([ref]$s2) 2 $h2q.Rows }

            $s3Rows = @($iast77.Count,$iast87.Count,1) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
            $s3 = New-Table $s3Rows 15
            Put-MultiCols ([ref]$s3) 0 $iast77
            Put-MultiCols ([ref]$s3) 7 $iast87

            Copy-Item -LiteralPath $template -Destination $out -Force
            $origin.NewProject() | Out-Null
            $origin.Load($out) | Out-Null
            $origin.Execute('win -a Book1; page.active=1; wks.nrows=1; wks.ncols=16;') | Out-Null
            $origin.PutWorksheet('[Book1]1', $s1, 0, 0) | Out-Null
            if (-not $skipQst) {
                $origin.Execute('win -a Book1; page.active=2; wks.nrows=1; wks.ncols=4;') | Out-Null
                $origin.PutWorksheet('[Book1]QST', $s2, 0, 0) | Out-Null
            }
            $origin.Execute('win -a Book1; page.active=3; wks.nrows=1; wks.ncols=15;') | Out-Null
            $origin.PutWorksheet('[Book1]IAST', $s3, 0, 0) | Out-Null

            $title = Escape-LT $config.titleFormat.Replace('{sample}', $sample)
            $w = [int]$config.pngExport.uniformWidthPx
            $doPng = $config.pngExport.enabled -and -not $NoPng
            $hd = $config.axes.HD; $ia = $config.axes.IAST; $qa = $config.axes.Qst
            $hdAxisCmd = Get-AxisCommand $hdBounds $hd 'HD' $warnings
            $iastAxisCmd = Get-AxisCommand $iastBounds $ia 'IAST' $warnings

            $cmd = "pe_cd $($config.originFolders.hd); win -a HD; Text.text$=`"$title`"; $hdAxisCmd doc -uw;"
            if ($doPng) { $cmd += " expGraph type:=png path:=`"$sampleDir`" filename:=`"${sample}_HD`" tr1.unit:=2 tr1.width:=$w;" }
            $cmd += " pe_cd $($config.originFolders.calc); win -a IAST; Text2.text$=`"$title`"; $iastAxisCmd doc -uw;"
            if ($doPng) { $cmd += " expGraph type:=png path:=`"$sampleDir`" filename:=`"${sample}_IAST`" tr1.unit:=2 tr1.width:=$w;" }
            if (-not $skipQst) {
                $yrProp = $qa.manualYBySample.PSObject.Properties[$sample]
                if ($yrProp) {
                    $yr = @($yrProp.Value)
                    $qstBounds = Get-PairBounds @($h2q.Rows, $d2q.Rows)
                    $qstConflict = $qstBounds -and ($qstBounds.XMin -lt [double]$qa.xFrom -or $qstBounds.XMax -gt [double]$qa.xTo -or $qstBounds.YMin -lt [double]$yr[0] -or $qstBounds.YMax -gt [double]$yr[1])
                    if ($qstConflict) {
                        $ycmd = 'layer -a;'
                        $warnings.Add('Qst data exceed configured axes; autoscale used to show all points.')
                    } else {
                        $ycmd = "layer.x.from=$($qa.xFrom); layer.x.to=$($qa.xTo); layer.y.from=$($yr[0]); layer.y.to=$($yr[1]);"
                    }
                } else {
                    $ycmd = "layer -a;"
                    $warnings.Add("No manual Qst y-axis range configured; used autoscale.")
                }
                $cmd += " pe_cd $($config.originFolders.calc); win -a Qst; Text2.text$=`"$title`"; $ycmd doc -uw;"
                if ($doPng) { $cmd += " expGraph type:=png path:=`"$sampleDir`" filename:=`"${sample}_Qst`" tr1.unit:=2 tr1.width:=$w;" }
            }
            $cmd += " save -dix $out;"
            $origin.Execute($cmd) | Out-Null
            Assert-OriginOutputs -SampleDir $sampleDir -Sample $sample -OpjuPath $out -DoPng $doPng -SkipQst $skipQst
        }
        catch {
            $status = "ERROR"
            $warnings.Add($_.Exception.Message)
        }

        $report.Add([pscustomobject]@{
            Sample = $sample
            Status = $status
            Warnings = ($warnings -join " | ")
            Opju = $out
        })
    }
}
finally {
    try { $excel.Quit() } catch {}
    try { $origin.Exit() } catch {}
}

$csv = Join-Path $moduleRoot "logs\workflow_report.csv"
$txt = Join-Path $moduleRoot "logs\workflow_report.txt"
$report | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
$report | Format-Table -AutoSize | Out-String | Set-Content -LiteralPath $txt -Encoding UTF8
Write-Host "Origin workflow finished."
Write-Host "Report: $csv"
if (@($report | Where-Object { $_.Status -ne 'OK' }).Count -gt 0) {
    exit 1
}
