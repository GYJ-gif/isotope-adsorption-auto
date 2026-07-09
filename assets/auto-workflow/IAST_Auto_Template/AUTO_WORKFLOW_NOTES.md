# IAST Automation Module

This module is bundled inside the skill and contains the IAST executable, source files, examples, and Excel automation script. It should be used through the top-level workflow whenever possible.

## Role In The Full Workflow

1. Read `吸附数据` from each `<Sample>.xlsx` workbook.
2. Extract H2/D2 77K and 87K adsorption data into four CSV files:
   - `<Sample>-77K-H2.csv`
   - `<Sample>-77K-D2.csv`
   - `<Sample>-87K-H2.csv`
   - `<Sample>-87K-D2.csv`
3. Calculate D2/H2 IAST at 77K and 87K.
4. Invoke the Qst CLI from the sibling `Qst_Auto_Template` module.

## Fixed Calculation Settings

- IAST: D2/H2, gas-phase mole fraction 1:1.
- Model: Auto.
- Pressure points: 1-10 kPa, 20-100 kPa, and 105 kPa.
- Qst: H2 and D2 are calculated separately from 77K/87K data.

## Standalone Example

```powershell
Set-Location <WorkflowRoot>\IAST_Auto_Template
.\run_iast_from_excel.ps1 -ExcelPath <InputRoot>\<Sample>\<Sample>.xlsx -Mode All -QstExe <WorkflowRoot>\Qst_Auto_Template\qst_calc_cli.exe
```
