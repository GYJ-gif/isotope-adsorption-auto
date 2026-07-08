# IAST + Qst Excel 自动化说明

## 一键全流程

双击 `运行IAST自动计算.cmd`，选择一个 `.xlsx` 文件。流程会依次执行：

1. 从工作表 `吸附数据` 提取前四组 Adsorption 数据。
2. 生成 `77K-H2`、`77K-D2`、`87K-H2`、`87K-D2` 四个 CSV。
3. 分别计算 77 K 和 87 K 的 IAST。
4. 分别用 `H2: 77K + 87K`、`D2: 77K + 87K` 计算 Qst。

假设输入文件为 `Ag-BeTa.xlsx`，会生成：

```text
Ag-BeTa\
├─ Ag-BeTa.xlsx
├─ Ag-BeTa-77K-H2.csv
├─ Ag-BeTa-77K-D2.csv
├─ Ag-BeTa-87K-H2.csv
├─ Ag-BeTa-87K-D2.csv
├─ IAST结果\
│  ├─ 77K\
│  └─ 87K\
└─ Qst结果\
   ├─ H2\
   └─ D2\
```

## 分步运行

步骤 1：双击 `步骤1-提取Adsorption数据.cmd`。

步骤 2：双击 `步骤2-计算IAST.cmd`。该步骤现在会连续计算 IAST 和 Qst。

命令行也可以这样运行：

```powershell
.\run_iast_from_excel.ps1 -Mode Extract -ExcelPath "D:\calculate\260706\Ag-BeTa.xlsx"
.\run_iast_from_excel.ps1 -Mode Calculate -ResultFolder "D:\calculate\260706\Ag-BeTa"
```

## 固定计算设置

- IAST：D2/H2，气相摩尔分数 1:1，模型 `Auto`。
- IAST 压力点：1-10 kPa、20-100 kPa（步长 10 kPa）和 105 kPa。
- Qst：分别计算 H2 和 D2，每个气体使用 77 K 与 87 K 两组数据。
- Qst 自动阶数：`a=5`、`b=2`。
- Qst 输出写入 `Qst结果`，不会创建 `Qst_final`。
