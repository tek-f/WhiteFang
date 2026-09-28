#!/usr/bin/env bash
# Golden-file test runner for WhiteFang. See docs/DECISIONS.md
# ("Golden-file test design") for the format this relies on:
#
#   tests/golden/<name>.wf         source program
#   tests/golden/<name>.expected   exact expected stdout (success tests only)
#   tests/golden/<name>.exitcode   expected exit code, default 0 if absent
#   tests/golden/errors/<name>.wf  same, but stdout is never checked --
#                                  these are compile/runtime-error programs,
#                                  only the exit code (65 or 70) matters
#
# Usage:
#   tests/run_golden.sh            run all tests, report pass/fail
#   tests/run_golden.sh --record   (re)generate .expected/.exitcode from
#                                  the current build's actual output --
#                                  use after adding a test or after a
#                                  deliberate behavior change

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WHITEFANG="$SCRIPT_DIR/../build/whitefang"
GOLDEN_DIR="$SCRIPT_DIR/golden"

if [[ ! -x "$WHITEFANG" ]]; then
    echo "error: $WHITEFANG not found -- run 'make' first" >&2
    exit 1
fi

RECORD=0
if [[ "${1:-}" == "--record" ]]; then
    RECORD=1
fi

pass=0
fail=0

run_one() {
    local wf="$1"
    local name dir expected_file exitcode_file is_error_test
    name="$(basename "$wf" .wf)"
    dir="$(dirname "$wf")"
    expected_file="$dir/$name.expected"
    exitcode_file="$dir/$name.exitcode"
    is_error_test=0
    [[ "$(basename "$dir")" == "errors" ]] && is_error_test=1

    local actual_stdout actual_exit
    actual_stdout="$("$WHITEFANG" "$wf" 2>/dev/null)"
    actual_exit=$?

    if [[ "$RECORD" -eq 1 ]]; then
        if [[ "$is_error_test" -eq 0 ]]; then
            printf '%s\n' "$actual_stdout" >"$expected_file"
        fi
        if [[ "$actual_exit" -ne 0 ]]; then
            echo "$actual_exit" >"$exitcode_file"
        else
            rm -f "$exitcode_file"
        fi
        echo "recorded: $name"
        return
    fi

    local expected_exit=0
    [[ -f "$exitcode_file" ]] && expected_exit="$(cat "$exitcode_file")"

    local ok=1
    if [[ "$actual_exit" -ne "$expected_exit" ]]; then
        ok=0
        echo "FAIL $name: exit code $actual_exit, expected $expected_exit"
    fi

    if [[ "$is_error_test" -eq 0 ]]; then
        local expected_stdout=""
        [[ -f "$expected_file" ]] && expected_stdout="$(cat "$expected_file")"
        if [[ "$actual_stdout" != "$expected_stdout" ]]; then
            ok=0
            echo "FAIL $name: stdout mismatch"
            diff <(printf '%s\n' "$expected_stdout") <(printf '%s\n' "$actual_stdout")
        fi
    fi

    if [[ "$ok" -eq 1 ]]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
}

shopt -s nullglob
for wf in "$GOLDEN_DIR"/*.wf "$GOLDEN_DIR"/errors/*.wf; do
    run_one "$wf"
done

if [[ "$RECORD" -eq 0 ]]; then
    echo "----"
    echo "passed: $pass, failed: $fail"
    [[ "$fail" -eq 0 ]]
fi
