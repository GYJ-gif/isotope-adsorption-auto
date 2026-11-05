#!/usr/bin/env bash
# Build the portable IAST/Qst command-line tools and their unit tests.
# Windows GUI programs are built with scripts/build_cpp.ps1 or source/build_gui.bat.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:-"$root/build"}"
iast_src="$root/assets/auto-workflow/IAST_Auto_Template/source"
qst_src="$root/assets/auto-workflow/Qst_Auto_Template/source"

mkdir -p "$out"
cxx="${CXX:-g++}"

"$cxx" -O2 -std=c++17 -o "$out/iast_auto_fit_test" "$iast_src/iast_auto_fit_test.cpp"
"$cxx" -O2 -std=c++17 -o "$out/qst_auto_fit_test" "$qst_src/qst_auto_fit_test.cpp"
"$cxx" -O2 -std=c++17 -o "$out/iast_calc" "$iast_src/iast_cli.cpp"
"$cxx" -O2 -std=c++17 -o "$out/qst_calc" "$qst_src/qst_cli.cpp"

echo "Built CLI tools and unit tests in $out"
