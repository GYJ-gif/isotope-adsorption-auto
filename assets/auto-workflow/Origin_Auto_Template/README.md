# Origin 自动作图模块

本模块负责把样品 Excel/计算结果写入 Origin 模板，并导出三类图：

- HD：吸附/脱附图
- IAST：选择性图
- Qst：等量吸附热图

## 单独运行

```powershell
Set-Location D:\calculate\Auto\Origin_Auto_Template
.\run_origin_workflow.ps1 -InputRoot D:\calculate\260706
```

只处理部分样品：

```powershell
.\run_origin_workflow.ps1 -InputRoot D:\calculate\260706 -Samples Ag-MOR,Cu-MOR
```

## 作图规则

- 使用 `templates\template.opju` 作为模板。
- 不新建图页，只使用模板已有：
  - `/Folder1/吸附/HD`
  - `/Folder1/计算/IAST`
  - `/Folder1/计算/Qst`
- 图标题替换为 `\b(<样品名>)`。
- IAST: x = 0~106, y = 1~1.6。
- Qst: x 从 0 开始，y 轴优先使用配置里的样品手动范围；新增样品如无配置，先自动缩放并提醒。
- Qst 有负值时只跳过 Qst 图，不影响 HD 和 IAST。
- PNG 统一尺寸导出，结果放回各样品文件夹。
