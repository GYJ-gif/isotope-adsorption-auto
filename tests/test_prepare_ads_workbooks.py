from __future__ import annotations

import sys
from pathlib import Path

from openpyxl import Workbook

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "assets" / "auto-workflow"))

import prepare_ads_workbooks as prep


def test_parse_temperature_and_gas_orders():
    assert prep.parse_source_name(Path("Sample-A_77K-H2.xlsx")) == ("Sample-A", "77", "H2")
    assert prep.parse_source_name(Path("Sample-B-H2-87K.xlsx")) == ("Sample-B", "87", "H2")
    assert prep.parse_source_name(Path("Sample-C 87 K - D2.xlsx")) == ("Sample-C", "87", "D2")


def test_optional_rename_map():
    prep.SAMPLE_RENAMES["Alias-Name"] = "Sample-A"
    try:
        sample, temperature, gas = prep.parse_source_name(Path("Alias-Name_77K-D2.xlsx"))
        assert (sample, temperature, gas) == ("Sample-A", "77", "D2")
    finally:
        prep.SAMPLE_RENAMES.clear()


def test_convert_instrument_units():
    pressure_kpa, loading = prep.convert_row(2500.0, 44.828)
    assert pressure_kpa == 2.5
    assert abs(loading - 2.0) < 1e-12


def test_reject_filename_without_condition():
    try:
        prep.parse_source_name(Path("Sample-A.xlsx"))
    except ValueError as exc:
        assert "77K" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_extract_adsorption_and_desorption(tmp_path: Path):
    workbook = Workbook()
    sheet = workbook.active
    sheet.title = "等温线"
    sheet.cell(1, 1).value = "P/(Pa)"
    sheet.cell(1, 2).value = "V/(cm³/g.STP)"
    pressures = [1000 * (index + 1) for index in range(12)] + [8000, 5000, 2000]
    for row, pressure in enumerate(pressures, start=2):
        sheet.cell(row, 1).value = pressure
        sheet.cell(row, 2).value = pressure / 1000.0 * prep.MOLAR_VOLUME_STP_CM3_PER_MMOL
    path = tmp_path / "Sample-A_77K-H2.xlsx"
    workbook.save(path)

    parsed = prep.extract_isotherm_rows(path)
    assert len(parsed["adsorption"]) == 12
    assert len(parsed["desorption"]) == 3
    assert parsed["adsorption"][0] == (1.0, 1.0)
    assert parsed["desorption"][-1][0] == 2.0
