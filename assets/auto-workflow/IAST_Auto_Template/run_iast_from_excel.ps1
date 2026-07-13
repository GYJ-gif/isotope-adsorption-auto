[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$ExcelPath,

    [string]$ResultFolder,

    [ValidateSet('Extract', 'Calculate', 'All')]
    [string]$Mode = 'All',

    [string]$IastExe,

    [string]$QstExe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $IastExe) { $IastExe = Join-Path $PSScriptRoot 'iast_calc_gui.exe' }
if (-not $QstExe) {
    $qstRelease = Join-Path (Split-Path -Parent $PSScriptRoot) 'Qst_release'
    $QstExe = Join-Path $qstRelease 'qst_calc_cli.exe'
}

function Get-ZipEntryText {
    param(
        [Parameter(Mandatory)]$Archive,
        [Parameter(Mandatory)][string]$EntryName
    )

    $entry = $Archive.Entries | Where-Object { $_.FullName -ieq $EntryName } | Select-Object -First 1
    if (-not $entry) { return $null }

    $reader = [System.IO.StreamReader]::new($entry.Open(), [System.Text.Encoding]::UTF8, $true)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}

function ConvertTo-ColumnNumber {
    param([Parameter(Mandatory)][string]$CellReference)

    $letters = ([regex]::Match($CellReference, '^[A-Z]+', 'IgnoreCase')).Value.ToUpperInvariant()
    if (-not $letters) { throw "无法识别单元格地址：$CellReference" }
    $number = 0
    foreach ($character in $letters.ToCharArray()) {
        $number = $number * 26 + ([int]$character - [int][char]'A' + 1)
    }
    return $number
}

function ConvertTo-ColumnName {
    param([Parameter(Mandatory)][int]$ColumnNumber)

    if ($ColumnNumber -lt 1) { throw "列号必须大于 0：$ColumnNumber" }
    $name = ''
    while ($ColumnNumber -gt 0) {
        $ColumnNumber--
        $name = [char]([int][char]'A' + ($ColumnNumber % 26)) + $name
        $ColumnNumber = [math]::Floor($ColumnNumber / 26)
    }
    return $name
}

function Get-CellRowNumber {
    param([Parameter(Mandatory)][string]$CellReference)

    $digits = ([regex]::Match($CellReference, '\d+$')).Value
    if (-not $digits) { throw "无法识别单元格地址：$CellReference" }
    return [int]$digits
}

function Get-WorksheetCells {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$SheetName
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $workbookText = Get-ZipEntryText -Archive $archive -EntryName 'xl/workbook.xml'
        $relationshipsText = Get-ZipEntryText -Archive $archive -EntryName 'xl/_rels/workbook.xml.rels'
        if (-not $workbookText -or -not $relationshipsText) { throw '该文件不是可识别的 XLSX 工作簿。' }

        [xml]$workbookXml = $workbookText
        [xml]$relationshipsXml = $relationshipsText
        $workbookNs = [System.Xml.XmlNamespaceManager]::new($workbookXml.NameTable)
        $workbookNs.AddNamespace('x', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')
        $workbookNs.AddNamespace('r', 'http://schemas.openxmlformats.org/officeDocument/2006/relationships')
        $sheetNode = $workbookXml.SelectSingleNode("//x:sheet[@name='$SheetName']", $workbookNs)
        if (-not $sheetNode) { throw "找不到工作表 [$SheetName]。" }

        $relationshipId = $sheetNode.GetAttribute('id', 'http://schemas.openxmlformats.org/officeDocument/2006/relationships')
        $relationshipNs = [System.Xml.XmlNamespaceManager]::new($relationshipsXml.NameTable)
        $relationshipNs.AddNamespace('p', 'http://schemas.openxmlformats.org/package/2006/relationships')
        $relationshipNode = $relationshipsXml.SelectSingleNode("//p:Relationship[@Id='$relationshipId']", $relationshipNs)
        if (-not $relationshipNode) { throw "无法定位工作表 [$SheetName] 的内部文件。" }

        $target = $relationshipNode.GetAttribute('Target')
        $worksheetEntry = if ($target.StartsWith('/')) { $target.TrimStart('/') } else { 'xl/' + $target.TrimStart('./') }
        $worksheetText = Get-ZipEntryText -Archive $archive -EntryName $worksheetEntry
        if (-not $worksheetText) { throw "无法读取工作表 [$SheetName]。" }

        $sharedStrings = @()
        $sharedStringsText = Get-ZipEntryText -Archive $archive -EntryName 'xl/sharedStrings.xml'
        if ($sharedStringsText) {
            [xml]$sharedStringsXml = $sharedStringsText
            $sharedNs = [System.Xml.XmlNamespaceManager]::new($sharedStringsXml.NameTable)
            $sharedNs.AddNamespace('x', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')
            foreach ($item in $sharedStringsXml.SelectNodes('//x:si', $sharedNs)) {
                $parts = $item.SelectNodes('.//x:t', $sharedNs) | ForEach-Object { $_.InnerText }
                $sharedStrings += ($parts -join '')
            }
        }

        [xml]$worksheetXml = $worksheetText
        $worksheetNs = [System.Xml.XmlNamespaceManager]::new($worksheetXml.NameTable)
        $worksheetNs.AddNamespace('x', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')
        $cells = @{}
        foreach ($cell in $worksheetXml.SelectNodes('//x:sheetData/x:row/x:c', $worksheetNs)) {
            $reference = $cell.GetAttribute('r')
            $type = $cell.GetAttribute('t')
            $value = $null
            if ($type -eq 'inlineStr') {
                $parts = $cell.SelectNodes('.//x:is/x:t', $worksheetNs) | ForEach-Object { $_.InnerText }
                $value = $parts -join ''
            } else {
                $valueNode = $cell.SelectSingleNode('./x:v', $worksheetNs)
                if ($valueNode) {
                    if ($type -eq 's') { $value = $sharedStrings[[int]$valueNode.InnerText] }
                    elseif ($type -eq 'b') { $value = if ($valueNode.InnerText -eq '1') { 'TRUE' } else { 'FALSE' } }
                    else { $value = $valueNode.InnerText }
                }
            }
            if ($null -ne $value) { $cells[$reference] = [string]$value }
        }
        return $cells
    } finally {
        $archive.Dispose()
    }
}

function Get-FirstExistingWorksheetCells {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string[]]$SheetNames
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    foreach ($sheetName in $SheetNames) {
        try {
            return Get-WorksheetCells -Path $Path -SheetName $sheetName
        }
        catch {
            $errors.Add($_.Exception.Message)
        }
    }
    throw "找不到可用工作表：$($SheetNames -join ', ')。最后错误：$($errors[-1])"
}

function ConvertTo-Number {
    param([string]$Value, [string]$CellReference)

    $number = 0.0
    if (-not [double]::TryParse($Value, [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$number)) {
        throw "单元格 $CellReference 不是有效数字：$Value"
    }
    return $number
}

function Get-IsothermData {
    param(
        [Parameter(Mandatory)][hashtable]$Cells,
        [Parameter(Mandatory)][ValidateSet('H2', 'D2')][string]$Gas,
        [Parameter(Mandatory)][ValidateSet('77', '87')][string]$TemperatureValue
    )

    $pattern = "(?i)(^|-)${Gas}-${TemperatureValue}K\b.*\bAdsorption\b"
    $headers = @($Cells.GetEnumerator() | Where-Object { $_.Value -match $pattern })
    if ($headers.Count -ne 1) {
        throw "应找到 1 组 ${Gas}-${TemperatureValue}K Adsorption 数据，实际找到 $($headers.Count) 组。"
    }

    $headerReference = [string]$headers[0].Key
    $pressureColumn = ConvertTo-ColumnNumber $headerReference
    $loadingColumn = $pressureColumn + 1
    $headerRow = Get-CellRowNumber $headerReference
    $pressureHeaderReference = "$(ConvertTo-ColumnName $pressureColumn)$($headerRow + 1)"
    $loadingHeaderReference = "$(ConvertTo-ColumnName $loadingColumn)$($headerRow + 1)"
    if ($Cells[$pressureHeaderReference] -notmatch '(?i)Pressure.*kPa') {
        throw "${Gas}-${TemperatureValue}K 压力列标题无法确认：$($Cells[$pressureHeaderReference])"
    }
    if ($Cells[$loadingHeaderReference] -notmatch '(?i)(Adsorbed|Loading).*mmol/g') {
        throw "${Gas}-${TemperatureValue}K 吸附量列标题无法确认：$($Cells[$loadingHeaderReference])"
    }

    $rows = [System.Collections.Generic.List[object]]::new()
    for ($row = $headerRow + 2; $row -le 100000; $row++) {
        $pressureReference = "$(ConvertTo-ColumnName $pressureColumn)$row"
        $loadingReference = "$(ConvertTo-ColumnName $loadingColumn)$row"
        if (-not $Cells.ContainsKey($pressureReference) -and -not $Cells.ContainsKey($loadingReference)) { break }
        if (-not $Cells.ContainsKey($pressureReference) -or -not $Cells.ContainsKey($loadingReference)) {
            throw "${Gas}-${TemperatureValue}K 在第 $row 行出现不完整的数据对。"
        }
        $pressure = ConvertTo-Number $Cells[$pressureReference] $pressureReference
        $loading = ConvertTo-Number $Cells[$loadingReference] $loadingReference
        if ($pressure -lt 0 -or $loading -lt 0) { throw "${Gas}-${TemperatureValue}K 在第 $row 行出现负值。" }
        $rows.Add([pscustomobject]@{ Pressure = $pressure; Loading = $loading })
    }
    if ($rows.Count -lt 10) { throw "${Gas}-${TemperatureValue}K 仅找到 $($rows.Count) 个吸附数据点，少于最低检查阈值 10。" }
    return $rows
}

function Get-IsothermDataByHeader {
    param(
        [Parameter(Mandatory)][hashtable]$Cells,
        [Parameter(Mandatory)][string]$HeaderReference,
        [Parameter(Mandatory)][string]$ExpectedLabel
    )

    $pressureColumn = ConvertTo-ColumnNumber $HeaderReference
    $loadingColumn = $pressureColumn + 1
    $headerRow = Get-CellRowNumber $HeaderReference
    $pressureHeaderReference = "$(ConvertTo-ColumnName $pressureColumn)$($headerRow + 1)"
    $loadingHeaderReference = "$(ConvertTo-ColumnName $loadingColumn)$($headerRow + 1)"
    if ($Cells[$pressureHeaderReference] -notmatch '(?i)Pressure.*kPa') {
        throw "${ExpectedLabel} 压力列标题无法确认：$($Cells[$pressureHeaderReference])"
    }
    if ($Cells[$loadingHeaderReference] -notmatch '(?i)(Adsorbed|Loading).*mmol/g') {
        throw "${ExpectedLabel} 吸附量列标题无法确认：$($Cells[$loadingHeaderReference])"
    }

    $rows = [System.Collections.Generic.List[object]]::new()
    for ($row = $headerRow + 2; $row -le 100000; $row++) {
        $pressureReference = "$(ConvertTo-ColumnName $pressureColumn)$row"
        $loadingReference = "$(ConvertTo-ColumnName $loadingColumn)$row"
        if (-not $Cells.ContainsKey($pressureReference) -and -not $Cells.ContainsKey($loadingReference)) { break }
        if (-not $Cells.ContainsKey($pressureReference) -or -not $Cells.ContainsKey($loadingReference)) {
            throw "${ExpectedLabel} 在第 $row 行出现不完整的数据对。"
        }
        $pressure = ConvertTo-Number $Cells[$pressureReference] $pressureReference
        $loading = ConvertTo-Number $Cells[$loadingReference] $loadingReference
        if ($pressure -lt 0 -or $loading -lt 0) { throw "${ExpectedLabel} 在第 $row 行出现负值。" }
        $rows.Add([pscustomobject]@{ Pressure = $pressure; Loading = $loading })
    }
    if ($rows.Count -lt 10) { throw "${ExpectedLabel} 仅找到 $($rows.Count) 个吸附数据点，少于最低检查阈值 10。" }
    return $rows
}

function Write-IsothermCsv {
    param([Parameter(Mandatory)]$Rows, [Parameter(Mandatory)][string]$Path)

    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('Pressure(kPa),Loading(mmol/g)')
    foreach ($row in $Rows) {
        $lines.Add($row.Pressure.ToString('R', $culture) + ',' + $row.Loading.ToString('R', $culture))
    }
    [System.IO.File]::WriteAllLines($Path, $lines, [System.Text.UTF8Encoding]::new($false))
}

function Quote-NativeArgument {
    param([Parameter(Mandatory)][string]$Value)
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-IastCalculation {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string]$H2Csv,
        [Parameter(Mandatory)][string]$D2Csv,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][string]$Prefix,
        [string]$D2Model = 'Auto',
        [string]$H2Model = 'Auto'
    )

    $arguments = @(
        '--batch', '--gas1', 'D2', '--csv1', $D2Csv, '--model1', $D2Model,
        '--gas2', 'H2', '--csv2', $H2Csv, '--model2', $H2Model,
        '--y1', '0.5', '--y2', '0.5',
        '--pressures', '1,2,3,4,5,6,7,8,9,10,20,30,40,50,60,70,80,90,100,105',
        '--output-dir', $OutputDirectory, '-o', $Prefix
    )
    $argumentLine = ($arguments | ForEach-Object { Quote-NativeArgument ([string]$_) }) -join ' '
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $argumentLine
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "IAST 程序运行失败，退出代码：$($process.ExitCode)" }
}

function Get-IastSelectedModels {
    param([Parameter(Mandatory)][string]$LogPath)
    $text = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
    $selected = @{}
    foreach ($gas in @('D2', 'H2')) {
        $match = [regex]::Match($text, "(?m)^\[\d+\]\s+$gas\s+-\s+requested\s+\S+,\s+selected\s+(SSL|DSL|SSLF|DSLF)\b")
        if (-not $match.Success) { throw "无法从 IAST 日志确认 $gas 的 AUTO 模型：$LogPath" }
        $selected[$gas] = $match.Groups[1].Value
    }
    return $selected
}

function Get-SimplerIastModel {
    param([Parameter(Mandatory)][string]$Model)
    switch ($Model) {
        'DSLF' { return 'DSL' }
        'DSL'  { return 'SSLF' }
        'SSLF' { return 'SSL' }
        default { return $null }
    }
}

function Assert-IastFallbackFit {
    param(
        [Parameter(Mandatory)][string]$LogPath,
        [Parameter(Mandatory)][hashtable]$ExpectedModels
    )
    $text = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
    foreach ($gas in @('D2', 'H2')) {
        $expected = $ExpectedModels[$gas]
        $match = [regex]::Match($text, "(?ms)^\[\d+\]\s+$gas\s+-\s+requested\s+$expected,\s+selected\s+(\S+).*?A1\s+([-+0-9.eE]+).*?B1\s+([-+0-9.eE]+).*?C1\s+([-+0-9.eE]+).*?A2\s+([-+0-9.eE]+).*?B2\s+([-+0-9.eE]+).*?C2\s+([-+0-9.eE]+).*?converged\s+=\s+(yes|no)")
        if (-not $match.Success -or $match.Groups[1].Value -ne $expected -or $match.Groups[8].Value -ne 'yes') {
            throw "$gas 的简化模型 $expected 未正常收敛或被自动替换；请人工检查模型参数。日志：$LogPath"
        }
        $activeGroups = switch ($expected) {
            'SSL'  { @(2,3) }
            'SSLF' { @(2,3,4) }
            'DSL'  { @(2,3,5,6) }
            default { @(2,3,4,5,6,7) }
        }
        foreach ($groupIndex in $activeGroups) {
            $value = 0.0
            if (-not [double]::TryParse($match.Groups[$groupIndex].Value, [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$value) -or
                [double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) {
                throw "$gas 的简化模型 $expected 存在异常参数；请人工检查。日志：$LogPath"
            }
        }
    }
}

function Invoke-IastFallbackWhenNeeded {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string]$H2Csv,
        [Parameter(Mandatory)][string]$D2Csv,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][string]$Prefix
    )
    $resultPath = Join-Path $OutputDirectory "${Prefix}_Selectivity.csv"
    $rows = @(Import-Csv -LiteralPath $resultPath)
    $hasLowSelectivity = $false
    foreach ($row in $rows) {
        $properties = @($row.PSObject.Properties)
        if ($properties.Count -lt 6) { throw "IAST 结果列数不足，无法检查选择性：$resultPath" }
        $value = 0.0
        if (-not [double]::TryParse([string]$properties[5].Value, [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$value)) {
            throw "IAST 选择性不是有效数字：$($properties[5].Value)"
        }
        if ($value -le 1.0) { $hasLowSelectivity = $true; break }
    }
    if (-not $hasLowSelectivity) { return }

    $logPath = Join-Path $OutputDirectory "${Prefix}_log.txt"
    $selected = Get-IastSelectedModels -LogPath $logPath
    $simpler = @{
        D2 = Get-SimplerIastModel $selected.D2
        H2 = Get-SimplerIastModel $selected.H2
    }
    if (-not $simpler.D2 -or -not $simpler.H2) {
        throw "IAST 选择性出现小于或等于 1，但至少一个 AUTO 模型已是最简单的 SSL；请人工检查模型参数。日志：$logPath"
    }
    Write-Warning "IAST 选择性出现小于或等于 1，改用更简单模型重算：D2 $($selected.D2)->$($simpler.D2)，H2 $($selected.H2)->$($simpler.H2)"
    Invoke-IastCalculation -Executable $Executable -H2Csv $H2Csv -D2Csv $D2Csv -OutputDirectory $OutputDirectory -Prefix $Prefix -D2Model $simpler.D2 -H2Model $simpler.H2
    Assert-IastFallbackFit -LogPath $logPath -ExpectedModels $simpler
}

function Invoke-QstCalculation {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string]$Csv77K,
        [Parameter(Mandatory)][string]$Csv87K,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][string]$Prefix
    )

    $scratchRoot = Join-Path (Split-Path -Parent $PSScriptRoot) '.tmp\qst_auto'
    $scratchDirectory = Join-Path $scratchRoot ([guid]::NewGuid().ToString('N'))
    [System.IO.Directory]::CreateDirectory($scratchDirectory) | Out-Null
    $scratchPrefix = 'qst_result'
    $scratch77K = Join-Path $scratchDirectory '77K.csv'
    $scratch87K = Join-Path $scratchDirectory '87K.csv'
    Copy-Item -LiteralPath $Csv77K -Destination $scratch77K -Force
    Copy-Item -LiteralPath $Csv87K -Destination $scratch87K -Force

    $arguments = @(
        '--temp1', '77', '--csv1', $scratch77K,
        '--temp2', '87', '--csv2', $scratch87K,
        '--max-a', '5', '--max-b', '2',
        '--output-dir', $scratchDirectory, '-o', $scratchPrefix
    )
    $argumentLine = ($arguments | ForEach-Object { Quote-NativeArgument ([string]$_) }) -join ' '
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $argumentLine
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Qst 程序运行失败，退出代码：$($process.ExitCode)" }

    [System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
    $suffixes = @('_Qst.csv', '_log.txt', '_Virial_Fit.svg', '_Qst.svg')
    foreach ($suffix in $suffixes) {
        $sourcePath = Join-Path $scratchDirectory "${scratchPrefix}${suffix}"
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Qst 未生成预期临时文件：$sourcePath" }
        $destinationPath = Join-Path $OutputDirectory "${Prefix}${suffix}"
        Move-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    }
    Remove-Item -LiteralPath $scratchDirectory -Recurse -Force
}

function Invoke-WorkbookResultSync {
    param([Parameter(Mandatory)][string]$WorkbookPath)

    $syncScript = Join-Path (Split-Path -Parent $PSScriptRoot) 'sync_calculation_results_to_workbook.py'
    if (-not (Test-Path -LiteralPath $syncScript -PathType Leaf)) { throw "找不到结果回写脚本：$syncScript" }

    $pythonPath = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (-not (Test-Path -LiteralPath $pythonPath -PathType Leaf)) { $pythonPath = 'python' }

    & $pythonPath $syncScript $WorkbookPath
    if ($LASTEXITCODE -ne 0) { throw "结果回写失败，退出代码：$LASTEXITCODE" }
}

function Assert-IsothermOrder {
    param([Parameter(Mandatory)][hashtable]$Cells)

    $headers = @($Cells.GetEnumerator() | Where-Object { $_.Value -match '(?i)\bAdsorption\b' } | ForEach-Object {
        [pscustomobject]@{
            Reference = [string]$_.Key
            Column = ConvertTo-ColumnNumber ([string]$_.Key)
            Title = [string]$_.Value
        }
    } | Sort-Object Column)
    if ($headers.Count -lt 4) { throw "吸附数据表少于四组 Adsorption 数据，实际找到 $($headers.Count) 组。" }

    $expected = @('H2-77K', 'D2-77K', 'H2-87K', 'D2-87K')
    $expectedColumns = @(1, 4, 7, 10)
    for ($index = 0; $index -lt 4; $index++) {
        if ($headers[$index].Column -ne $expectedColumns[$index]) {
            throw "第 $($index + 1) 组数据应从第 $($expectedColumns[$index]) 列开始，实际从第 $($headers[$index].Column) 列开始。"
        }
        if ($headers[$index].Title -notmatch "(?i)\b$([regex]::Escape($expected[$index]))\b") {
            Write-Warning "第 $($index + 1) 组按位置作为 $($expected[$index]) 处理；原表标题为：$($headers[$index].Title)"
        }
    }
    if ($headers.Count -ge 5 -and $headers[4].Title -notmatch '(?i)\bN2-77K\b') {
        Write-Warning "第五组数据未识别为 N2-77K，本流程不读取该组：$($headers[4].Title)"
    }
    return $headers[0..3]
}

if ($Mode -in @('Extract', 'All')) {
    if (-not $ExcelPath) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = [System.Windows.Forms.OpenFileDialog]::new()
        $dialog.Title = '步骤 1：选择包含吸附/脱附数据的 Excel 文件'
        $dialog.Filter = 'Excel 工作簿 (*.xlsx)|*.xlsx'
        if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return }
        $ExcelPath = $dialog.FileName
    }

    $ExcelPath = (Resolve-Path -LiteralPath $ExcelPath).Path
    if ([System.IO.Path]::GetExtension($ExcelPath) -ine '.xlsx') { throw '目前仅支持 .xlsx 文件。' }
    $workbookName = [System.IO.Path]::GetFileNameWithoutExtension($ExcelPath)
    $sourceParent = [System.IO.Path]::GetDirectoryName($ExcelPath)
    $resultRoot = $sourceParent

    $cells = Get-FirstExistingWorksheetCells -Path $ExcelPath -SheetNames @('吸附数据', 'ads/des', 'ads-des', '簾現方象')
    $orderedHeaders = @(Assert-IsothermOrder -Cells $cells)
    $datasets = @{
        '77K-H2' = Get-IsothermDataByHeader -Cells $cells -HeaderReference $orderedHeaders[0].Reference -ExpectedLabel '77K-H2'
        '77K-D2' = Get-IsothermDataByHeader -Cells $cells -HeaderReference $orderedHeaders[1].Reference -ExpectedLabel '77K-D2'
        '87K-H2' = Get-IsothermDataByHeader -Cells $cells -HeaderReference $orderedHeaders[2].Reference -ExpectedLabel '87K-H2'
        '87K-D2' = Get-IsothermDataByHeader -Cells $cells -HeaderReference $orderedHeaders[3].Reference -ExpectedLabel '87K-D2'
    }
    [System.IO.Directory]::CreateDirectory($resultRoot) | Out-Null
    foreach ($key in @('77K-H2', '77K-D2', '87K-H2', '87K-D2')) {
        $csvPath = Join-Path $resultRoot "${workbookName}-${key}.csv"
        Write-IsothermCsv -Rows $datasets[$key] -Path $csvPath
        Write-Host "已输出：$csvPath（$($datasets[$key].Count) 点）"
    }

    $ResultFolder = $resultRoot
    Write-Host '步骤 1 完成。'
}

if ($Mode -in @('Calculate', 'All')) {
    if (-not $ResultFolder) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = [System.Windows.Forms.FolderBrowserDialog]::new()
        $dialog.Description = '步骤 2：选择步骤 1 创建的 Excel 同名文件夹'
        if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return }
        $ResultFolder = $dialog.SelectedPath
    }
    $ResultFolder = (Resolve-Path -LiteralPath $ResultFolder).Path
    if (-not (Test-Path -LiteralPath $IastExe -PathType Leaf)) { throw "找不到 IAST 程序：$IastExe" }
    if (-not (Test-Path -LiteralPath $QstExe -PathType Leaf)) { throw "找不到 Qst 程序：$QstExe" }
    $excelFiles = @(Get-ChildItem -LiteralPath $ResultFolder -File -Filter '*.xlsx')
    if ($excelFiles.Count -ne 1) { throw "步骤 2 要求文件夹内恰好有 1 个 .xlsx，实际找到 $($excelFiles.Count) 个。" }
    $workbookName = [System.IO.Path]::GetFileNameWithoutExtension($excelFiles[0].Name)
    $iastOutputRoot = Join-Path $ResultFolder 'IAST结果'
    $qstOutputRoot = Join-Path $ResultFolder 'Qst结果'

    foreach ($temperatureValue in @('77', '87')) {
        $h2Csv = Join-Path $ResultFolder "${workbookName}-${temperatureValue}K-H2.csv"
        $d2Csv = Join-Path $ResultFolder "${workbookName}-${temperatureValue}K-D2.csv"
        if (-not (Test-Path -LiteralPath $h2Csv -PathType Leaf)) { throw "缺少输入 CSV：$h2Csv" }
        if (-not (Test-Path -LiteralPath $d2Csv -PathType Leaf)) { throw "缺少输入 CSV：$d2Csv" }
        $outputDirectory = $iastOutputRoot
        [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
        $prefix = "${workbookName}-${temperatureValue}K-IAST"
        Invoke-IastCalculation -Executable $IastExe -H2Csv $h2Csv -D2Csv $d2Csv -OutputDirectory $outputDirectory -Prefix $prefix
        Invoke-IastFallbackWhenNeeded -Executable $IastExe -H2Csv $h2Csv -D2Csv $d2Csv -OutputDirectory $outputDirectory -Prefix $prefix

        $expectedFiles = @(
            "${prefix}_Selectivity.csv", "${prefix}_log.txt", "${prefix}_Exp_isotherms.svg",
            "${prefix}_IAST_validation.svg", "${prefix}_Selectivity.svg", "${prefix}_Separation_Potential.svg"
        )
        foreach ($expectedFile in $expectedFiles) {
            $expectedPath = Join-Path $outputDirectory $expectedFile
            if (-not (Test-Path -LiteralPath $expectedPath -PathType Leaf)) { throw "IAST 未生成预期文件：$expectedPath" }
        }
        Write-Host "${temperatureValue} K IAST 完成：$outputDirectory"
    }

    foreach ($gas in @('H2', 'D2')) {
        $csv77K = Join-Path $ResultFolder "${workbookName}-77K-${gas}.csv"
        $csv87K = Join-Path $ResultFolder "${workbookName}-87K-${gas}.csv"
        if (-not (Test-Path -LiteralPath $csv77K -PathType Leaf)) { throw "缺少输入 CSV：$csv77K" }
        if (-not (Test-Path -LiteralPath $csv87K -PathType Leaf)) { throw "缺少输入 CSV：$csv87K" }
        $outputDirectory = $qstOutputRoot
        [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
        $prefix = "${workbookName}-${gas}-77K-87K-Qst"
        Invoke-QstCalculation -Executable $QstExe -Csv77K $csv77K -Csv87K $csv87K -OutputDirectory $outputDirectory -Prefix $prefix

        $expectedFiles = @(
            "${prefix}_Qst.csv", "${prefix}_log.txt", "${prefix}_Virial_Fit.svg", "${prefix}_Qst.svg"
        )
        foreach ($expectedFile in $expectedFiles) {
            $expectedPath = Join-Path $outputDirectory $expectedFile
            if (-not (Test-Path -LiteralPath $expectedPath -PathType Leaf)) { throw "Qst 未生成预期文件：$expectedPath" }
        }
        Write-Host "${gas} Qst 完成：$outputDirectory"
    }
    Invoke-WorkbookResultSync -WorkbookPath $excelFiles[0].FullName
    Write-Host '步骤 2 完成。'
}
