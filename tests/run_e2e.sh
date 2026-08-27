#!/usr/bin/env bash
# cpp_jq - SPDX-License-Identifier: MIT
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/cpp_jq"
FIXTURES="$ROOT/tests/fixtures"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0

# build if needed
if [[ ! -x "$BIN" ]]; then make -C "$ROOT" build >/dev/null; fi

run_positive() {
    local d="$1" name; name="$(basename "$d")"
    local input_file
    if   [[ -f "$d/input.ndjson" ]]; then input_file="$d/input.ndjson"
    elif [[ -f "$d/input.json"   ]]; then input_file="$d/input.json"
    else echo "SKIP $name (no input)"; return; fi

    local expected_file
    if   [[ -f "$d/expected.json" ]]; then expected_file="$d/expected.json"
    elif [[ -f "$d/expected.txt" ]]; then expected_file="$d/expected.txt"
    elif [[ -f "$d/expected.ndjson" ]]; then expected_file="$d/expected.ndjson"
    else echo "SKIP $name (no expected)"; return; fi

    local filter="$d/filter.jq"
    local args=()
    [[ -f "$d/args" ]] && mapfile -t args < "$d/args"
    local out_file="$TMP/out.$name"

    "$BIN" "${args[@]}" -f "$filter" < "$input_file" >"$out_file" 2>"$TMP/err.$name"
    local code=$?

    if [[ -f "$expected_file" && ! -s "$expected_file" ]]; then
        if [[ $code -eq 0 && ! -s "$out_file" ]]; then
            echo "PASS $name"; PASS=$((PASS+1))
        else
            echo "FAIL $name (expected empty stdout, exit=$code)"
            FAIL=$((FAIL+1))
        fi
        return
    fi

    if [[ $code -eq 0 ]] && diff -q "$out_file" "$expected_file" >/dev/null; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name (exit=$code)"
        diff -u "$expected_file" "$out_file" | sed 's/^/    /'
        FAIL=$((FAIL+1))
    fi
}

run_negative() {
    local d="$1" name; name="$(basename "$d")"
    local input_file
    if   [[ -f "$d/input.ndjson" ]]; then input_file="$d/input.ndjson"
    elif [[ -f "$d/input.json"   ]]; then input_file="$d/input.json"
    else echo "SKIP $name (no input)"; return; fi

    local filter="$d/filter.jq"
    [[ ! -f "$d/expected_exit" ]] && { echo "SKIP $name"; return; }
    "$BIN" -f "$filter" < "$input_file" >/dev/null 2>"$TMP/err.$name"
    local code=$?
    local exp_exit; exp_exit="$(cat "$d/expected_exit")"
    local exp_sub;   exp_sub="$(cat "$d/expected_stderr_substr")"
    local stderr;    stderr="$(cat "$TMP/err.$name")"
    if [[ "$code" == "$exp_exit" ]] && [[ "$stderr" == *"$exp_sub"* ]]; then
        echo "PASS $name"; PASS=$((PASS+1))
    else
        echo "FAIL $name (got exit=$code stderr='$stderr')"; FAIL=$((FAIL+1))
    fi
}

for d in "$FIXTURES"/*/; do
    [[ -d "$d" ]] || continue
    n="$(basename "$d")"
    if [[ "$n" == invalid_* ]]; then run_negative "$d"; else run_positive "$d"; fi
done

echo
echo "PASS=$PASS  FAIL=$FAIL"
[[ $FAIL -eq 0 ]]