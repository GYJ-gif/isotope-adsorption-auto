# Auto Workflow

This folder contains the complete packaged isotope adsorption automation workflow used by the `isotope-adsorption-auto` skill. It is designed to be portable: scripts locate sibling modules and templates relative to this folder, not through a fixed machine-specific path.

## Run

Prepared input:

```powershell
.\run_full_workflow.ps1 -InputRoot <InputRoot>
```

Raw instrument Excel input:

```powershell
.\run_full_workflow.ps1 -InputRoot <RawExcelRoot> -PrepareRawInput
```

Selected samples:

```powershell
.\run_full_workflow.ps1 -InputRoot <InputRoot> -Samples <Sample1>,<Sample2>
```

Skip Origin plotting:

```powershell
.\run_full_workflow.ps1 -InputRoot <InputRoot> -SkipOrigin
```

Run only Origin using existing calculation sheets:

```powershell
.\run_full_workflow.ps1 -InputRoot <InputRoot> -SkipCalculate
```

## Input Layout

Prepared workbooks should use:

```text
<InputRoot>\<Sample>\<Sample>.xlsx
```

Each sample workbook should contain H2/D2 data at 77K and 87K. The default order is:

1. H2-77K
2. D2-77K
3. H2-87K
4. D2-87K

Raw instrument files should include sample name, temperature, and gas in the filename, such as `77K-H2`, `77K-D2`, `87K-H2`, or `87K-D2`.

## Modules

- `prepare_ads_workbooks.py`: groups raw instrument XLSX files into sample workbooks.
- `IAST_Auto_Template`: extracts adsorption CSVs and runs IAST/Qst calculation.
- `sync_calculation_results_to_workbook.py`: writes IAST/Qst CSV outputs back into the workbook.
- `Origin_Auto_Template`: reads workbook sheets and exports OPJU/PNG figures from the bundled Origin template.

## Outputs

Each sample folder receives calculation CSVs, `IAST结果`, `Qst结果`, `<Sample>.opju`, `<Sample>_HD.png`, `<Sample>_IAST.png`, and `<Sample>_Qst.png`.

Workflow reports are written under the current workflow directory:

```text
logs\full_workflow_report.txt
Origin_Auto_Template\logs\workflow_report.csv
```
