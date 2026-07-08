# Qst-Calc — 吸附热计算工具

**版本**：v1.0  |  **作者**：袁大强  |  **日期**：2026-06-12

---

## 1. 软件简介

Qst-Calc 是一个基于 **Virial 方程** 的吸附热（Isosteric Heat of Adsorption, Qst）计算工具。通过多温度等温线数据，拟合 virial 系数并计算各覆盖度下的等量吸附热。

### 核心功能

| 功能 | 说明 |
|------|------|
| 多温度拟合 | 支持 2–5 个温度数据集同时拟合 |
| 自动阶数选择 | 自动尝试多组 Virial 阶数，用 BIC + 稳定性诊断择优 |
| 自动 Qst 区间 | 自动取所有温度数据共同覆盖的吸附量范围，减少外推 |
| Virial 方程 | 基于 Clausius-Clapeyron 关系推导 |
| 图表输出 | GDI+ 实时曲线预览 + SVG 矢量图导出 |
| 参数误差 | 显示拟合参数标准误差，并导出 Qst 标准误差 |

### 性能特点

- **纯 C++17**，零第三方依赖
- **可执行文件 < 400 KB**
- 解析 Jacobian，计算效率高

---

## 2. 理论基础

### 2.1 Virial 方程

吸附量与压力的关系用 virial 展开表示：

$$\ln P = \ln N + \frac{1}{T}\sum_{i=0}^{n} a_i N^i + \sum_{j=0}^{m} b_j N^j$$

其中：
- $N$：吸附量（mmol/g）
- $T$：温度（K）
- $a_i$：温度相关 virial 系数
- $b_j$：温度无关 virial 系数

### 2.2 吸附热计算

由 Clausius-Clapeyron 方程：

$$Q_{st} = -R \sum_{i=0}^{n} a_i N^i$$

其中 $R = 8.314$ J/(mol·K)。

---

## 3. GUI 使用方法

### 3.1 启动

双击 `qst_calc_gui.exe`，界面包含：

- **温度输入区**：2–5 组温度 + CSV 文件选择
- **参数配置**：Virial 阶数、初始参数
- **图表预览区**：GDI+ 实时绘制拟合曲线
- **控制按钮**：Run / Save Chart / Save Log

### 3.2 操作流程

1. **输入温度**：在 Temperature 1–5 中填入温度值（K）
2. **加载数据**：点击 `...` 选择各温度对应的 CSV 文件
3. **设置最高阶数**：`a` 和 `b` 作为自动搜索的最高 Virial 阶数，程序会自动选择最终阶数
4. **开始计算**：点击 Run
5. **查看图表**：实时显示拟合曲线
6. **保存结果**：Save Chart（PNG/BMP）或 Save Log（完整日志）

---

## 4. CLI 使用方法

### 4.1 编译

```batch
cl /O2 /EHsc /std:c++17 /Fe:qst_calc.exe qst_cli.cpp /link gdiplus.lib
```

### 4.2 命令行参数

```
qst_calc.exe
  --temp1 273       温度1 (K)
  --csv1  data1.csv  温度1数据文件
  --temp2 298       温度2 (K)
  --csv2  data2.csv  温度2数据文件
  ...
  --temp5 323       温度5 (K)
  --csv5  data5.csv  温度5数据文件
  
  --nT 2            Temperature-dependent 阶数
  --nI 2            Temperature-independent 阶数
  --prefix result   输出前缀
```

### 4.3 示例

```batch
qst_calc.exe --temp1 273 --csv1 273K.csv --temp2 298 --csv2 298K.csv --temp3 323 --csv3 323K.csv --nT 2 --nI 2 --prefix qst_result
```

---

## 5. 输入文件格式

CSV 格式，两列：`Pressure(kPa), Loading(mmol/g)`

```csv
Pressure(kPa),Loading(mmol/g)
0.1,0.5
1.0,2.3
10.0,5.1
...
```

---

## 6. 输出文件

| 文件 | 内容 |
|------|------|
| `qst_calc_gui.exe 所在目录/Qst_final/{prefix}_log.txt` | 完整计算日志（候选阶数、拟合参数、误差、Qst 数据） |
| `qst_calc_gui.exe 所在目录/Qst_final/{prefix}_Qst.csv` | Qst 曲线数据，含 Qst 标准误差 |
| `qst_calc_gui.exe 所在目录/Qst_final/{prefix}_Virial_Fit.svg` | Virial 拟合图 |
| `qst_calc_gui.exe 所在目录/Qst_final/{prefix}_Qst.svg` | Qst 曲线图 |

GUI 所有自动生成的计算结果都会写入 `qst_calc_gui.exe` 所在目录下的 `Qst_final/` 文件夹，不受快捷方式或命令行启动位置影响。

---

## 7. 从源码编译

### 7.1 前置要求

- Windows: Visual Studio 2022 BuildTools（或 VS 2019+）

### 7.2 编译 GUI

```batch
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd source
cl /O2 /EHsc /std:c++17 /Fe:qst_calc_gui.exe qst_gui.cpp /link gdiplus.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib
```

### 7.3 编译 CLI

```batch
cl /O2 /EHsc /std:c++17 /Fe:qst_calc.exe qst_cli.cpp /link gdiplus.lib
```

### 7.4 源码结构

```
source/
├── qst_core.h      # 核心库（virial 拟合、Qst 计算）
├── qst_gui.cpp     # Windows GUI 前端
├── qst_cli.cpp     # 命令行前端
└── build_gui.bat   # 一键编译脚本
```

---

## 8. 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v1.0 | 2026-06-12 | 初版，virial 方程拟合 + Qst 计算 |
| v1.1 | 2026-06-16 | 自动阶数选择、自动 Qst 区间、Qst_final 输出、Qst 标准误差 |
