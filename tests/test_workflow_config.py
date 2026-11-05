from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLACEHOLDERS = [f"Sample-{letter}" for letter in "ABCDEFGH"]


def _load(relative: str) -> dict:
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def test_placeholder_sample_lists_match():
    top = _load("assets/auto-workflow/workflow_config.json")
    origin = _load("assets/auto-workflow/Origin_Auto_Template/workflow_config.json")
    assert top["samplesDefault"] == PLACEHOLDERS
    assert origin["samplesDefault"] == PLACEHOLDERS


def test_qst_axis_is_shared_rather_than_per_sample():
    origin = _load("assets/auto-workflow/Origin_Auto_Template/workflow_config.json")
    qst = origin["axes"]["Qst"]
    assert qst["manualYBySample"] == {}
    assert qst["yFrom"] < qst["yTo"]
    assert qst["xFrom"] < qst["xTo"]


def test_example_logs_use_relative_paths():
    logs = [
        ROOT / "assets/auto-workflow/IAST_Auto_Template/example/IAST_result_log.txt",
        ROOT / "assets/auto-workflow/Qst_Auto_Template/Qst_final/Qst_result_log.txt",
    ]
    drive = re.compile(r"[A-Za-z]:\\")
    for path in logs:
        text = path.read_text(encoding="utf-8")
        assert drive.search(text) is None, path.name
    iast_log = logs[0].read_text(encoding="utf-8")
    assert "example/C3H6.csv" in iast_log
    assert "example/C2H4.csv" in iast_log
