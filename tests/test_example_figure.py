from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _load_figure_module():
    path = ROOT / "docs" / "generate_example_figure.py"
    spec = importlib.util.spec_from_file_location("generate_example_figure", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_render_example_figure(tmp_path: Path):
    module = _load_figure_module()
    output = tmp_path / "example_outputs.png"
    module.render(output)
    assert output.stat().st_size > 10_000
