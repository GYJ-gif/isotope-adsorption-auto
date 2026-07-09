# Qst Automation Module

This module is bundled inside the skill and contains the Qst CLI/GUI executables, source files, and examples. It should normally be invoked by the top-level workflow or by `IAST_Auto_Template\run_iast_from_excel.ps1`.

## Role In The Full Workflow

1. Receive H2 or D2 adsorption CSV files at 77K and 87K.
2. Fit the multi-temperature isotherms.
3. Export Qst CSV/SVG outputs under each sample folder.
4. Allow the top-level workflow to sync Qst CSV results back into `<Sample>.xlsx`.

## Standalone Example

```powershell
.\qst_calc_cli.exe <Csv77K> <Csv87K> <OutputPrefix>
```

Prefer the full workflow for production runs because it handles sample naming, folder layout, workbook sync, and Origin plotting.
