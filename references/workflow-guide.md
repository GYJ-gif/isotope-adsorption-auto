# Isotope Adsorption Auto Workflow Guide

## Flow Summary

The packaged automation performs these steps:

1. Optionally prepare raw instrument `.xlsx` files into one workbook per material.
2. Extract H2/D2 adsorption CSV files from each material workbook.
3. Run IAST calculations for 77K and 87K.
4. Run Qst calculations from 77K/87K H2 and D2 data.
5. Write IAST/Qst CSV results back into `<Sample>.xlsx`.
6. Use the Origin OPJU template to export `<Sample>.opju`, `<Sample>_HD.png`, `<Sample>_IAST.png`, and `<Sample>_Qst.png`.

## Important Files

- `assets/auto-workflow/workflow_config.json`: Top-level module and default sample configuration.
- `assets/auto-workflow/run_full_workflow.ps1`: Main orchestrator.
- `assets/auto-workflow/run_prepare_ads_workbooks.ps1`: Raw input wrapper.
- `assets/auto-workflow/prepare_ads_workbooks.py`: Raw XLSX parser and workbook writer.
- `assets/auto-workflow/sync_calculation_results_to_workbook.py`: Writes calculation CSV files into workbook sheets.
- `assets/auto-workflow/IAST_Auto_Template/run_iast_from_excel.ps1`: Extracts adsorption data and invokes IAST/Qst executables.
- `assets/auto-workflow/Origin_Auto_Template/run_origin_workflow.ps1`: Reads Excel through COM and exports Origin files.

## External Software Checks

Before running Origin export, verify:

```powershell
$excel = New-Object -ComObject Excel.Application; $excel.Quit()
$origin = New-Object -ComObject Origin.ApplicationSI; $origin.Exit()
```

If either command fails, the target computer is missing the required desktop application or COM registration.

Before raw XLSX preparation or result sync, verify Python and openpyxl:

```powershell
python -c "import openpyxl; print(openpyxl.__version__)"
```

## Sheet Names

The current workflow writes and reads these canonical sheets:

- `吸附数据`
- `Qst结果`
- `IAST结果`

The scripts also accept several legacy aliases defined in the packaged Python and PowerShell files.

## Default Samples

The default configured sample list is:

```text
Ag-BeTa
Ag-MOR
Ag-USY
Ag-ZSM-5
Cu-FER
Cu-MOR
Cu-USY
Cu-ZSM-5
```

Pass `-Samples` to process a smaller set.

## Troubleshooting

- Missing `<Sample>.xlsx`: confirm the input root is the parent folder containing sample subfolders.
- Raw input produces no samples: confirm filenames include sample name plus 77K/87K and H2/D2.
- IAST/Qst results exist but Origin exports are empty: rerun without `-SkipCalculate` so result sheets are synced back into the workbook.
- Origin returns success but expected files are absent: check `Origin_Auto_Template/logs/workflow_report.csv`; the current workflow validates `.opju` and PNG existence.
- Negative Qst values: the workflow skips only Qst plot/export and continues HD/IAST output.
