# Origin Plotting Module

This module reads sample Excel workbooks, writes adsorption/IAST/Qst data into the bundled Origin template, and exports OPJU/PNG figures. Paths are resolved relative to this module unless explicitly provided.

## Standalone Run

```powershell
.\run_origin_workflow.ps1 -InputRoot <InputRoot>
```

Selected samples:

```powershell
.\run_origin_workflow.ps1 -InputRoot <InputRoot> -Samples <Sample1>,<Sample2>
```

## Plotting Rules

- Use `templates\template.opju` as the Origin template.
- Use the existing graph pages in the template:
  - `/Folder1/吸附/HD`
  - `/Folder1/计算/IAST`
  - `/Folder1/计算/Qst`
- Replace graph titles with `\b(<Sample>)`.
- IAST axis defaults: x = 0-106, y = 1-1.6.
- Qst starts at x = 0 and uses per-sample manual y-axis ranges when configured; otherwise it autos-scales and records a warning.
- If Qst contains negative values, skip only the Qst plot; HD and IAST continue.
- PNG outputs are written back to each sample folder.
