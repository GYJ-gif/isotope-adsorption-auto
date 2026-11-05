#!/usr/bin/env python3
"""Draw the portfolio figure from the bundled example isotherms.

The curves are the packaged CO2 virial example and the C2H4/C3H6 IAST example.
They illustrate the calculators; they are not unpublished isotope measurements.
"""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
IAST_EXAMPLE = ROOT / "assets" / "auto-workflow" / "IAST_Auto_Template" / "example"
QST_EXAMPLE = ROOT / "assets" / "auto-workflow" / "Qst_Auto_Template" / "example"
QST_RESULT = ROOT / "assets" / "auto-workflow" / "Qst_Auto_Template" / "Qst_final" / "Qst_result_Qst.csv"
DEFAULT_OUTPUT = ROOT / "docs" / "example_outputs.png"

NAVY = "#1B4F72"
TEAL = "#0E7C66"
CLAY = "#A93226"
GOLD = "#B9770E"
INK = "#243044"
GRID = "#D5DDE6"


def read_xy(path: Path) -> tuple[list[float], list[float]]:
    xs: list[float] = []
    ys: list[float] = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle)
        next(reader, None)
        for row in reader:
            if len(row) < 2 or not row[0].strip():
                continue
            xs.append(float(row[0]))
            ys.append(float(row[1]))
    if not xs:
        raise ValueError(f"No data rows in {path}")
    return xs, ys


def read_qst(path: Path) -> tuple[list[float], list[float], list[float]]:
    loading: list[float] = []
    heat: list[float] = []
    error: list[float] = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle)
        next(reader, None)
        for row in reader:
            if len(row) < 3 or not row[0].strip():
                continue
            loading.append(float(row[0]))
            heat.append(float(row[1]))
            error.append(float(row[2]))
    if not loading:
        raise ValueError(f"No Qst rows in {path}")
    return loading, heat, error


def read_selectivity(path: Path) -> tuple[list[float], list[float], list[float]]:
    pressure: list[float] = []
    selectivity: list[float] = []
    potential: list[float] = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle)
        next(reader, None)
        for row in reader:
            if len(row) < 7 or not row[0].strip():
                continue
            pressure.append(float(row[0]))
            selectivity.append(float(row[5]))
            potential.append(float(row[6]))
    if not pressure:
        raise ValueError(f"No selectivity rows in {path}")
    return pressure, selectivity, potential


def _style() -> None:
    plt.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["WenQuanYi Micro Hei", "Noto Sans CJK SC", "DejaVu Sans"],
            "axes.unicode_minus": False,
            "axes.labelsize": 11,
            "axes.titlesize": 12,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "legend.fontsize": 8,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "axes.edgecolor": INK,
            "axes.labelcolor": INK,
            "xtick.color": INK,
            "ytick.color": INK,
            "text.color": INK,
        }
    )


def _panel(ax: plt.Axes, label: str, title: str) -> None:
    ax.set_title(f"{label}  {title}", loc="left", pad=10)
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def render(output_path: Path) -> None:
    _style()
    co2 = [
        (273, NAVY, QST_EXAMPLE / "CO2_273K.csv"),
        (298, TEAL, QST_EXAMPLE / "CO2_298K.csv"),
        (313, GOLD, QST_EXAMPLE / "CO2_313K.csv"),
    ]
    c2_p, c2_q = read_xy(IAST_EXAMPLE / "C2H4.csv")
    c3_p, c3_q = read_xy(IAST_EXAMPLE / "C3H6.csv")
    loading, heat, error = read_qst(QST_RESULT)
    pressure, selectivity, potential = read_selectivity(IAST_EXAMPLE / "IAST_result_Selectivity.csv")

    fig, axes = plt.subplots(2, 2, figsize=(10.8, 8.2), constrained_layout=True)
    fig.suptitle("示例计算输出  ·  Bundled example outputs", fontsize=15, color=INK)

    ax = axes[0, 0]
    for temperature, color, path in co2:
        x, y = read_xy(path)
        ax.plot(x, y, marker="o", markersize=3.5, linewidth=1.4, color=color, label=f"CO$_2$ {temperature} K")
    ax.set_xscale("log")
    ax.set_xlabel("Pressure / kPa")
    ax.set_ylabel("Uptake / mmol g$^{-1}$")
    ax.legend(frameon=False)
    _panel(ax, "(a)", "多温度等温线  Virial input")

    ax = axes[0, 1]
    lower = [value - err for value, err in zip(heat, error)]
    upper = [value + err for value, err in zip(heat, error)]
    ax.fill_between(loading, lower, upper, color=NAVY, alpha=0.18, linewidth=0)
    ax.plot(loading, heat, color=NAVY, linewidth=1.8)
    ax.set_xlabel("Loading / mmol g$^{-1}$")
    ax.set_ylabel("Q$_{st}$ / kJ mol$^{-1}$")
    _panel(ax, "(b)", "等量吸附热  Qst")

    ax = axes[1, 0]
    ax.plot(c2_p, c2_q, marker="o", markersize=2.4, linewidth=1.2, color=NAVY, label="C$_2$H$_4$")
    ax.plot(c3_p, c3_q, marker="o", markersize=2.4, linewidth=1.2, color=CLAY, label="C$_3$H$_6$")
    ax.set_xscale("log")
    ax.set_xlabel("Pressure / kPa")
    ax.set_ylabel("Uptake / mmol g$^{-1}$")
    ax.legend(frameon=False)
    _panel(ax, "(c)", "纯组分等温线  DSLF input")

    ax = axes[1, 1]
    ax.plot(pressure, selectivity, color=NAVY, marker="o", markersize=3.2, linewidth=1.5, label="Selectivity")
    ax.set_xlabel("Total pressure / kPa")
    ax.set_ylabel("Selectivity")
    twin = ax.twinx()
    twin.plot(pressure, potential, color=GOLD, marker="s", markersize=3.0, linewidth=1.3, label="Separation potential")
    twin.set_ylabel("Separation potential / mmol g$^{-1}$", color=GOLD)
    twin.tick_params(axis="y", colors=GOLD)
    twin.spines["top"].set_visible(False)
    handles, labels = ax.get_legend_handles_labels()
    twin_handles, twin_labels = twin.get_legend_handles_labels()
    ax.legend(handles + twin_handles, labels + twin_labels, frameon=False, loc="center right")
    _panel(ax, "(d)", "IAST 选择性  y$_1$ = 0.10")
    ax.spines["right"].set_visible(True)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def main() -> None:
    render(DEFAULT_OUTPUT)
    print(DEFAULT_OUTPUT)


if __name__ == "__main__":
    main()
