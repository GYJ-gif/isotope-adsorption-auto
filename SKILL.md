---
name: isotope-adsorption-auto
description: Automate isotope adsorption data workflows for H2/D2 materials: prepare raw instrument XLSX files into per-sample workbooks, run bundled IAST and Qst calculations, sync results back to Excel, and generate Origin OPJU/PNG figures. Use when the user asks to process isotope adsorption data folders, run the full adsorption automation, calculate IAST/Qst, or create Origin plots from the packaged workflow templates.
---

# Isotope Adsorption Automation

## Core Workflow

Use this skill to run the packaged H2/D2 isotope adsorption automation in `assets/auto-workflow/`. Do not assume any fixed local path from the machine that created this skill; locate bundled files relative to the skill directory or script directory, and require the user to provide the input data directory.

1. Confirm the input type:
   - Raw instrument export folder containing `.xlsx` files: use `-PrepareRawInput`.
   - Already prepared folder shaped as `<InputRoot>/<Sample>/<Sample>.xlsx`: omit `-PrepareRawInput`.
2. Run the full workflow from the skill directory:
   ```powershell
   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot <InputRoot>
   ```
3. For raw instrument exports:
   ```powershell
   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot <RawExcelRoot> -PrepareRawInput
   ```
4. If the user wants to copy the bundled workflow to another working directory first:
   ```powershell
   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Deploy-IsotopeAdsorptionAuto.ps1 -DestinationRoot <WorkflowRoot>
   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -WorkflowRoot <WorkflowRoot> -InputRoot <InputRoot>
   ```

## Required Inputs

Prepared input should use this layout:

```text
<InputRoot>\<Sample>\<Sample>.xlsx
```

Each workbook should contain H2/D2 adsorption data at 77K and 87K. The default order is:

1. H2-77K
2. D2-77K
3. H2-87K
4. D2-87K

Raw instrument `.xlsx` filenames must include sample name plus one of `77K-H2`, `77K-D2`, `87K-H2`, or `87K-D2`. The preparation step groups those files into per-sample workbooks.

## Common Commands

Run only selected samples:

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot <InputRoot> -Samples <Sample1>,<Sample2>
```

Run calculations only, skipping Origin:

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot <InputRoot> -SkipOrigin
```

Run Origin only, using existing IAST/Qst sheets in workbooks:

```powershell
.\scripts\Invoke-IsotopeAdsorptionAuto.ps1 -InputRoot <InputRoot> -SkipCalculate
```

Deploy to a custom workflow directory:

```powershell
.\scripts\Deploy-IsotopeAdsorptionAuto.ps1 -DestinationRoot <WorkflowRoot>
```

## Outputs

For each sample, expect outputs in the sample folder:

```text
<Sample>-77K-H2.csv
<Sample>-77K-D2.csv
<Sample>-87K-H2.csv
<Sample>-87K-D2.csv
IAST结果\
Qst结果\
<Sample>.opju
<Sample>_HD.png
<Sample>_IAST.png
<Sample>_Qst.png
```

If Qst values are negative, skip only the Qst plot/export for that sample. HD, IAST, and `.opju` export should continue, with warnings recorded in the Origin report.

## Dependencies

Check these before running on a new computer:

- Windows PowerShell.
- Python 3 with `openpyxl` available. The script first tries the bundled Codex runtime Python when it can be discovered under the current user profile, then falls back to `python`.
- Microsoft Excel desktop application for Origin automation input reading.
- Origin desktop application with `Origin.ApplicationSI` COM automation available.
- The packaged IAST/Qst `.exe` files in `assets/auto-workflow/IAST_Auto_Template` and `assets/auto-workflow/Qst_Auto_Template`.

Do not claim Excel, Origin, or Python dependencies are installed unless verified on that machine.

## Packaged Resources

- `assets/auto-workflow/prepare_ads_workbooks.py`: Convert raw instrument XLSX files into per-sample workbooks.
- `assets/auto-workflow/run_full_workflow.ps1`: Orchestrate prepare, IAST/Qst calculation, result sync, and Origin export.
- `assets/auto-workflow/IAST_Auto_Template/`: IAST executable, extraction script, source, and examples.
- `assets/auto-workflow/Qst_Auto_Template/`: Qst executables, source, and examples.
- `assets/auto-workflow/Origin_Auto_Template/templates/template.opju`: Origin plotting template.
- `references/workflow-guide.md`: Detailed execution notes, file inventory, and troubleshooting.

Never modify user data in place without confirming the intended input root. Generated results are expected in each sample directory.
