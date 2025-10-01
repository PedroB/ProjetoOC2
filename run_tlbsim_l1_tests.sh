#!/bin/bash
set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
EXPECTED_OUTPUTS_TARGET_DIR="tlbsim-l1"

cd "$SCRIPT_DIR"
mkdir -p reports

make -j

pass=0
fail=0
total=0

for input in inputs/*; do
    input_file=$(basename "$input" .txt)

    expected_output_file="outputs/$EXPECTED_OUTPUTS_TARGET_DIR/$input_file.out"
    report_file="reports/$input_file.diff"

    if [[ ! -f "$expected_output_file" ]]; then
        echo "Expected output file $expected_output_file not found"
        exit 1
    fi

    ./build/tlbsim "$input" > "reports/$input_file.out" 2> /dev/null
    ./build/tlbsim "$input" > "reports/$input_file.log" 2>&1

    {
        echo "#####################################################################"
        echo "# Input: $input"
        echo "# Left side: expected ($expected_output_file)"
        echo "# Right side: actual (reports/$input_file.out)"
        echo "#####################################################################"
    } > "$report_file"

    if diff -y --expand-tabs "$expected_output_file" "reports/$input_file.out" >> "$report_file"; then
        echo "✅ PASS: $input_file"
        ((pass++))
    else
        echo "❌ FAIL: $input_file (see $report_file)"
        ((fail++))
    fi
    ((total++))
done

echo "--------------------------------------------------------"
echo "Total: $total | Passed: $pass | Failed: $fail"

# exit code para CI (1 se houver falhas)
if (( fail > 0 )); then
  exit 1
fi
