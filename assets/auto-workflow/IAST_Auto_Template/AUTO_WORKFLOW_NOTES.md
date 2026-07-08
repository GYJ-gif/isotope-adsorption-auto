# IAST 自动化模块说明

本模块来自 `D:\calculate\IAST_release`，保留了可执行文件、源码、示例和 Excel 自动化脚本。

## 在全流程中的职责

1. 从每个样品的 `<样品名>.xlsx` 中读取 `吸附数据`。
2. 提取 H2/D2 的 77K、87K Adsorption 数据，生成四个 CSV：
   - `<样品名>-77K-H2.csv`
   - `<样品名>-77K-D2.csv`
   - `<样品名>-87K-H2.csv`
   - `<样品名>-87K-D2.csv`
3. 计算 77K 和 87K 的 D2/H2 IAST。
4. 调用 Qst CLI，用 77K/87K 两组数据分别计算 H2 和 D2 的 Qst。

## 固定计算设置

- IAST：D2/H2，气相摩尔分数 1:1。
- 模型：Auto。
- 压力点：1-10 kPa、20-100 kPa 和 105 kPa。
- Qst：H2 和 D2 分别计算。
- Qst 阶数：自动脚本内设置。

## 单独运行

```powershell
Set-Location D:\calculate\Auto\IAST_Auto_Template
.\run_iast_from_excel.ps1 -ExcelPath D:\calculate\260706\Ag-BeTa\Ag-BeTa.xlsx -Mode All -QstExe D:\calculate\Auto\Qst_Auto_Template\qst_calc_cli.exe
```

