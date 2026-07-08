# 数据处理 + Origin 科研绘图自动化

本目录整合完整默认流程：

1. `prepare_ads_workbooks.py`：把仪器导出的原始 `.xlsx` 整理成每个材料一个工作簿。
2. `IAST_Auto_Template`：从材料工作簿提取 H2/D2 吸附数据，计算 IAST 和 Qst。
3. `sync_calculation_results_to_workbook.py`：把 IAST/Qst 结果回写到 `<材料名>.xlsx`。
4. `Origin_Auto_Template`：从 `<材料名>.xlsx` 读取吸附、IAST、Qst 数据，写入 Origin 模板并导出图。

## 推荐一键流程

如果输入目录已经是每个材料一个文件夹：

```powershell
Set-Location D:\calculate\Auto
.\run_full_workflow.ps1 -InputRoot D:\calculate\260708
```

如果输入目录中直接放的是仪器导出的原始 `.xlsx`：

```powershell
Set-Location D:\calculate\Auto
.\run_full_workflow.ps1 -InputRoot D:\calculate\test -PrepareRawInput
```

只处理部分材料：

```powershell
.\run_full_workflow.ps1 -InputRoot D:\calculate\260708 -Samples Ag-MOR,Cu-MOR
```

只跑计算，不跑 Origin：

```powershell
.\run_full_workflow.ps1 -InputRoot D:\calculate\260708 -SkipOrigin
```

只跑 Origin，不重新计算 IAST/Qst：

```powershell
.\run_full_workflow.ps1 -InputRoot D:\calculate\260708 -SkipCalculate
```

## 输入要求

默认完整材料工作簿结构：

```text
D:\calculate\<日期>\<材料名>\<材料名>.xlsx
```

每个材料需要 H2/D2 在 77K 和 87K 下的数据，默认顺序为：

1. H2-77K
2. D2-77K
3. H2-87K
4. D2-87K

N2 数据不会参与默认计算和绘图。

## 工作簿表名

新整理出的工作簿使用以下表名：

- `吸附数据`
- `Qst结果`
- `IAST结果`

流程也兼容历史表名：

- `ads-des`
- `ads/des`
- `Qst`
- `IAST`

## 关键规则

- 默认读取每个材料文件夹中的 `<材料名>.xlsx`。
- 计算阶段会生成 CSV/SVG/TXT，同时把 IAST/Qst CSV 结果回写到 `<材料名>.xlsx` 的 `IAST结果` 和 `Qst结果` 表。
- Origin 阶段只从 `<材料名>.xlsx` 读取数据，不直接读取 IAST/Qst 结果文件夹。
- 使用 `D:\calculate\Auto\Origin_Auto_Template\templates\template.opju` 作为 Origin 模板。
- 每个材料导出 `<材料名>.opju`、`<材料名>_HD.png`、`<材料名>_IAST.png`、`<材料名>_Qst.png`。
- 如果 Qst 数据出现负值，只跳过该材料的 Qst 图和 Qst PNG 导出；HD、IAST 和 `.opju` 继续导出，并在 Origin 报告中记录警告。
- Origin 导出后会校验 `.opju` 和预期 PNG 是否存在；缺失会标记为失败，避免误报成功。
- 图标题格式为 `\b(<材料名>)`。

## 输出位置

每个材料目录输出：

```text
<材料名>-77K-H2.csv
<材料名>-77K-D2.csv
<材料名>-87K-H2.csv
<材料名>-87K-D2.csv
IAST结果\
Qst结果\
<材料名>.opju
<材料名>_HD.png
<材料名>_IAST.png
<材料名>_Qst.png
```

总报告：

```text
D:\calculate\Auto\logs\full_workflow_report.txt
D:\calculate\Auto\Origin_Auto_Template\logs\workflow_report.csv
```

## 已固化的问题修复

- 修复“IAST/Qst 已计算但没有写入 `<材料名>.xlsx`”的问题：新增 `sync_calculation_results_to_workbook.py`，并在计算流程末尾自动调用。
- 修复“Origin 从空表读取导致无图导出”的问题：计算结果会先回写 Excel，Origin 再读取 Excel。
- 修复“Origin 内部失败但外层报告显示 OK”的问题：Origin 脚本现在会在有错误时返回非 0。
- 修复“Origin 报告 OK 但目录里没有 PNG/opju”的问题：新增导出产物存在性校验。
- 保留默认规则：Qst 为负值时跳过 Qst 图，不影响 HD/IAST 导出。
