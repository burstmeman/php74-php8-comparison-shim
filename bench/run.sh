#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PHP_BIN="${PHP_BIN:-php}"
ITERATIONS="${SNC_ITERATIONS:-1000000}"
RUNS="${SNC_RUNS:-5}"

echo "PHP: $(${PHP_BIN} -v | head -n 1)"
echo "Iterations: ${ITERATIONS}"
echo "Runs per case: ${RUNS}"
echo

run_case() {
  local label="$1"
  shift

  local start_ns end_ns elapsed_ms
  local total_ms=0
  for _ in $(seq 1 "${RUNS}"); do
    start_ns=$(date +%s%N)
    "$@" >/dev/null 2>&1
    end_ns=$(date +%s%N)
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    total_ms=$(( total_ms + elapsed_ms ))
  done

  local avg_ms=$(( total_ms / RUNS ))
  echo "case=${label}"
  echo "avg_elapsed_ms=${avg_ms}"
  echo
}

# Baseline: no extension loaded. This is the lowest expected time.
run_case "No extension (disabled)" \
  env SNC_ITERATIONS="${ITERATIONS}" \
  "${PHP_BIN}" -n "${ROOT_DIR}/bench/compare.php"

# Off mode: handlers are not installed. Should be close to baseline.
run_case "Extension loaded: Off" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=off \
  "${PHP_BIN}" -n -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=off \
  "${ROOT_DIR}/bench/compare.php"

# Report mode with mixed comparisons. Expect higher time from zend_error().
run_case "Extension loaded: Report" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/compare.php"

# Error mode with try/catch. Expect high overhead from throw/catch path.
run_case "Extension loaded: Error" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=error SNC_VALIDATE=1 \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=error \
  "${ROOT_DIR}/bench/compare.php"

# Report mode but numeric strings only. Measures opcode handler overhead only.
run_case "Opcode overhead (no report)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench_opcode_overhead.php"

# Report mode with non-numeric strings. Measures zend_error() cost.
run_case "Deprecated cost (with report)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench_deprecated_cost.php"
