# isotope-adsorption-auto

同位素 H2/D2 吸附数据自动化 Codex Skill。它把 `D:\calculate\Auto` 中的完整流程封装为可迁移 Skill，用于自动完成原始仪器 Excel 整理、IAST/Qst 计算、结果回写、Origin 作图和 PNG/OPJU 导出。

## 功能

- 将原始仪器导出的 `.xlsx` 文件整理为每个样品一个工作簿。
- 从样品工作簿提取 H2/D2 在 77K、87K 的吸附数据。
- 调用随 Skill 打包的 IAST 和 Qst 可执行程序完成计算。
- 将 IAST/Qst CSV 结果回写到样品 Excel。
- 基于 Origin 模板导出 `<样品>.opju`、`<样品>_HD.png`、`<样品>_IAST.png`、`<样品>_Qst.png`。
- 当 Qst 出现负值时，仅跳过该样品 Qst 图导出，HD/IAST/OPJU 继续输出。

## 安装

把仓库下载到 Codex 可发现的 skills 目录，例如：

```powershell
git clone https://github.com/GYJ-gif/isotope-adsorption-auto.git "$env:USERPROFILE\.codex\skills\isotope-adsorption-auto"
```

也可以放在任意目录，然后在 Codex 中明确引用这个 Skill 路径。

## 新电脑依赖

运行完整流程前，请确认新电脑具备：

- Windows PowerShell。
- Python 3，并安装 `openpyxl`。
- Microsoft Excel 桌面版。
- Origin 桌面版，并启用 `Origin.ApplicationSI` COM 自动化。
- 本仓库内打包的 IAST/Qst `.exe`、Origin `.opju` 模板和脚本文件完整存在。

Excel、Origin、Python 环境属于本机外部依赖，实际可用性需要在目标电脑上确认。

## 推荐用法

在 Codex 中可以直接说：

```text
使用 $isotope-adsorption-auto 处理 D:\calculate\260708 的全部样品。
```

如果输入目录中直接放的是仪器导出的原始 `.xlsx`：

```text
使用 $isotope-adsorption-auto 处理 D:\calculate\test 中的原始仪器 Excel，并完成全部计算和 Origin 作图。
```

## 手动命令

从 Skill 目录直接运行完整流程：

```powershell
Set-Location "$env:USERPROFILE\.codex\skills\isotope-adsorption-auto"
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot D:\calculate\260708
```

处理原始仪器导出 Excel：

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot D:\calculate\test -PrepareRawInput
```

只处理指定样品：

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot D:\calculate\260708 -Samples Ag-MOR,Cu-MOR
```

只计算 IAST/Qst，不运行 Origin：

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot D:\calculate\260708 -SkipOrigin
```

只运行 Origin，使用已有计算结果：

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot D:\calculate\260708 -SkipCalculate
```

如需先把打包流程部署到固定目录：

```powershell
.\scripts\Deploy-IsotopeAdsorptionAuto.ps1 -DestinationRoot D:\calculate\Auto
```

## 输入结构

已整理好的输入目录应为：

```text
<输入根目录>\<样品名>\<样品名>.xlsx
```

每个样品默认需要：

1. H2-77K
2. D2-77K
3. H2-87K
4. D2-87K

原始仪器 Excel 文件名需要能识别样品名、温度和气体，例如包含 `77K-H2`、`77K-D2`、`87K-H2`、`87K-D2`。

## 输出

每个样品目录中会生成：

```text
<样品>-77K-H2.csv
<样品>-77K-D2.csv
<样品>-87K-H2.csv
<样品>-87K-D2.csv
IAST结果\
Qst结果\
<样品>.opju
<样品>_HD.png
<样品>_IAST.png
<样品>_Qst.png
```

运行报告位于：

```text
assets\auto-workflow\logs\full_workflow_report.txt
assets\auto-workflow\Origin_Auto_Template\logs\workflow_report.csv
```

## 文件说明

- `SKILL.md`：Codex 调用 Skill 时读取的核心说明。
- `agents/openai.yaml`：Skill 展示和默认提示元数据。
- `scripts/Invoke-IsotopeAdsorptionAuto.ps1`：从 Skill 启动完整流程。
- `scripts/Deploy-IsotopeAdsorptionAuto.ps1`：将打包的自动化流程复制到目标目录。
- `assets/auto-workflow/`：完整自动化脚本、模板、可执行文件、示例和源码。
- `references/workflow-guide.md`：流程细节、依赖检查和排错说明。

## 排错

- 找不到 `<样品>.xlsx`：确认 `-InputRoot` 是样品文件夹的上一级目录。
- 原始 Excel 没有生成样品：确认文件名包含温度和气体标识。
- Origin 无法运行：确认 Excel 和 Origin 已安装，并且 COM 自动化可用。
- 计算完成但 Origin 没图：不要使用 `-SkipCalculate`，先让流程把计算结果回写进 Excel。
- Qst 图缺失：查看报告；如果 Qst 为负值，流程会按规则跳过 Qst 图。
