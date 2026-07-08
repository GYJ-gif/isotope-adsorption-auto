# Qst 自动化模块说明

本模块来自 `D:\calculate\Qst_release`，保留了 CLI、GUI、源码和示例。

## 在全流程中的职责

Qst 计算通常由 `IAST_Auto_Template\run_iast_from_excel.ps1` 联动调用，不需要单独手动运行。

输入来自同一样品的 77K 和 87K Adsorption CSV：

- H2: `<样品名>-77K-H2.csv` + `<样品名>-87K-H2.csv`
- D2: `<样品名>-77K-D2.csv` + `<样品名>-87K-D2.csv`

输出到：

```text
<样品名>\Qst结果\H2
<样品名>\Qst结果\D2
```

## 负值规则

如果后续 Origin 作图阶段读取到 Qst 数据中存在负值：

- 只跳过该样品的 Qst 图；
- HD 和 IAST 图继续生成；
- 日志中写入提醒。

