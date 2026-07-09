# IAST + Qst Excel 自动化说明

## 一键全流程

优先使用 Skill 根目录下的 `scripts\Invoke-IsotopeAdsorptionAuto.ps1` 或工作流根目录下的 `run_full_workflow.ps1`。它们会按相对路径自动定位 IAST、Qst 和 Origin 模块。

如果需要单独运行本模块：

```powershell
.\run_iast_from_excel.ps1 -ExcelPath <InputRoot>\<Sample>\<Sample>.xlsx -Mode All -QstExe <WorkflowRoot>\Qst_Auto_Template\qst_calc_cli.exe
```

## 处理步骤

1. 从工作表 `吸附数据` 提取前四组 Adsorption 数据。
2. 生成 `77K-H2`、`77K-D2`、`87K-H2`、`87K-D2` 四个 CSV。
3. 分别计算 77K 和 87K 的 IAST。
4. 分别用 `H2: 77K + 87K`、`D2: 77K + 87K` 计算 Qst。

## 输出结构示例

```text
<Sample>\
  <Sample>.xlsx
  <Sample>-77K-H2.csv
  <Sample>-77K-D2.csv
  <Sample>-87K-H2.csv
  <Sample>-87K-D2.csv
  IAST结果\
  Qst结果\
```

## 固定计算设置

- IAST：D2/H2，气相摩尔分数 1:1，模型 `Auto`。
- IAST 压力点：1-10 kPa、20-100 kPa 和 105 kPa。
- Qst：分别计算 H2 和 D2，每个气体使用 77K 与 87K 两组数据。
- Qst 输出写入 `Qst结果`。
