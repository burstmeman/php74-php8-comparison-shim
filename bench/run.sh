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
  local total_comparisons_ns=0
  for _ in $(seq 1 "${RUNS}"); do
    start_ns=$(date +%s%N)
    output="$("$@" 2>/dev/null)"
    end_ns=$(date +%s%N)
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    total_ms=$(( total_ms + elapsed_ms ))

    comparisons_ns=$(printf '%s\n' "${output}" | sed -n 's/^comparisons_elapsed_ns=//p' | tail -n 1)
    comparisons_ns=${comparisons_ns:-0}
    total_comparisons_ns=$(( total_comparisons_ns + comparisons_ns ))
  done

  local avg_total_ms=$(( total_ms / RUNS ))
  local avg_comparisons_ns=$(( total_comparisons_ns / RUNS ))
  local avg_comparisons_ms=$(( avg_comparisons_ns / 1000000 ))
  echo "case=${label}"
  echo "avg_total_elapsed_ms=${avg_total_ms}"
  echo "avg_comparisons_elapsed_ns=${avg_comparisons_ns}"
  echo "avg_comparisons_elapsed_ms=${avg_comparisons_ms}"
  echo
}

# Baseline: no extension loaded. This is the lowest expected time.
run_case "No extension (disabled)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n "${ROOT_DIR}/bench/bench.php"

# Off mode: handlers are not installed. Should be close to baseline.
run_case "Extension loaded: Off" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=off SNC_CASE=compare \
  "${PHP_BIN}" -n -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=off \
  "${ROOT_DIR}/bench/bench.php"

# Report mode with mixed comparisons. Expect higher time from zend_error().
run_case "Extension loaded: Report" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report SNC_CASE=compare \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# Report mode with sampling. Expect lower cost from reduced reporting.
run_case "Extension loaded: Report (sampling=5)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report SNC_CASE=compare \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  -d php74_php8_comparison_shim.sampling_factor=5 \
  "${ROOT_DIR}/bench/bench.php"

# Simulate mode: returns PHP 8.0 results without reporting.
run_case "Extension loaded: Simulate" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=simulate SNC_CASE=compare \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=simulate \
  "${ROOT_DIR}/bench/bench.php"

# Simulate and report: returns PHP 8.0 results with deprecations.
run_case "Extension loaded: Simulate + Report" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=simulate_and_report SNC_CASE=compare \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=simulate_and_report \
  "${ROOT_DIR}/bench/bench.php"

# Error mode with try/catch. Expect high overhead from throw/catch path.
run_case "Extension loaded: Error" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=error SNC_VALIDATE=1 SNC_ALLOW_THROW=1 SNC_CASE=compare \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=error \
  "${ROOT_DIR}/bench/bench.php"

# Report mode but numeric strings only. Measures opcode handler overhead only.
run_case "Opcodes iteration overhead" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report SNC_CASE=opcode_overhead \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# Report mode with non-numeric strings. Measures zend_error() cost.
run_case "Deprecation log cost" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report SNC_CASE=deprecated_cost \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# Report mode with deferred reporting; measure only comparison loop time.
run_case "Extension loaded: Report (defer)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_MODE=report SNC_CASE=defer_report SNC_MEASURE_INTERNAL=1 \
  "${PHP_BIN}" -n -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  -d php74_php8_comparison_shim.report_mode=defer \
  "${ROOT_DIR}/bench/bench.php"
