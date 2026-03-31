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

# run_case LABEL CMD [ARGS...]
#
# Runs CMD RUNS times, collects wall-clock time and the per-run metrics
# emitted by bench.php on stdout:
#   comparisons_elapsed_ns=<ns>   — time spent in the comparison loop
#   flush_elapsed_ns=<ns>         — time spent flushing the deferred buffer
#                                   (only present for the defer_report case)
#
# Outputs averaged results. avg_flush_elapsed_ms is shown only when non-zero.
run_case() {
  local label="$1"
  shift

  local run start_ns end_ns elapsed_ms output comparisons_ns flush_ns
  local total_ms=0 total_comparisons_ns=0 total_flush_ns=0

  for ((run = 1; run <= RUNS; run++)); do
    start_ns=$(date +%s%N)
    output="$("$@" 2>/dev/null)"
    end_ns=$(date +%s%N)

    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    total_ms=$(( total_ms + elapsed_ms ))

    comparisons_ns=$(printf '%s\n' "${output}" | sed -n 's/^comparisons_elapsed_ns=//p' | head -n 1)
    total_comparisons_ns=$(( total_comparisons_ns + ${comparisons_ns:-0} ))

    flush_ns=$(printf '%s\n' "${output}" | sed -n 's/^flush_elapsed_ns=//p' | head -n 1)
    total_flush_ns=$(( total_flush_ns + ${flush_ns:-0} ))
  done

  local avg_total_ms=$(( total_ms / RUNS ))
  local avg_comparisons_ms=$(( total_comparisons_ns / RUNS / 1000000 ))
  local avg_flush_ns=$(( total_flush_ns / RUNS ))

  echo "case=${label}"
  echo "avg_total_elapsed_ms=${avg_total_ms}"
  echo "avg_comparisons_elapsed_ms=${avg_comparisons_ms}"
  # Show flush timing only when the defer case emitted flush_elapsed_ns.
  # Reported in microseconds because the flush of a small buffer is sub-millisecond.
  if [ "${avg_flush_ns}" -gt 0 ]; then
    echo "avg_flush_elapsed_us=$(( avg_flush_ns / 1000 ))"
  fi
  echo
}

# ── Baseline ────────────────────────────────────────────────────────────────

# No extension loaded. Lowest expected time — pure PHP comparison loop.
run_case "No extension (disabled)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n "${ROOT_DIR}/bench/bench.php"

# ── Extension loaded ─────────────────────────────────────────────────────────

# Off mode: extension is loaded but handlers are not installed.
# Should be close to baseline — only module load cost.
run_case "Extension loaded: Off" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=off \
  "${ROOT_DIR}/bench/bench.php"

# Report mode: mixed pairs (half intercepted, half numeric).
# Overhead comes from zend_error(E_DEPRECATED) on every changed comparison.
run_case "Extension loaded: Report" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# Report mode with sampling=5: check and report once per 5 intercepted comparisons.
run_case "Extension loaded: Report (sampling=5)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  -d php74_php8_comparison_shim.sampling_factor=5 \
  "${ROOT_DIR}/bench/bench.php"

# Simulate mode: returns PHP 8.0 results without emitting any deprecation.
# Cheaper than report because it skips the zend_error() path.
run_case "Extension loaded: Simulate" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=simulate \
  "${ROOT_DIR}/bench/bench.php"

# Simulate and report: returns PHP 8.0 results AND emits deprecations.
run_case "Extension loaded: Simulate + Report" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=simulate_and_report \
  "${ROOT_DIR}/bench/bench.php"

# Error mode: throws an Error on every intercepted comparison.
# SNC_ALLOW_THROW=1 wraps each comparison in try/catch to keep the loop alive.
run_case "Extension loaded: Error" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=compare SNC_ALLOW_THROW=1 \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=error \
  "${ROOT_DIR}/bench/bench.php"

# ── Isolation cases ──────────────────────────────────────────────────────────

# Numeric strings only — the type-gate rejects every pair immediately.
# Measures bare opcode-handler dispatch overhead with no interception logic.
run_case "Opcodes iteration overhead" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=opcode_overhead \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=0 \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# All pairs intercepted, sync report mode — every comparison triggers zend_error().
# Isolates the full interception + inline reporting cost.
run_case "Report cost: all intercepted (sync)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=report_cost \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  "${ROOT_DIR}/bench/bench.php"

# Defer report mode: comparisons buffer entries instead of calling zend_error().
# bench.php calls php74_php8_cmps_flush_deferred() explicitly after the loop,
# so comparison and reporting costs are timed separately:
#   avg_comparisons_elapsed_ms — comparison loop + buffering only (no zend_error)
#   avg_flush_elapsed_us       — flush: emitting all buffered deprecations
run_case "Extension loaded: Report (defer)" \
  env SNC_ITERATIONS="${ITERATIONS}" SNC_CASE=defer_report \
  "${PHP_BIN}" -n \
  -d display_errors=0 -d log_errors=0 -d error_reporting=E_ALL \
  -d extension_dir="${ROOT_DIR}/modules" \
  -d extension=php74_php8_comparison_shim.so \
  -d php74_php8_comparison_shim.mode=report \
  -d php74_php8_comparison_shim.report_mode=defer \
  "${ROOT_DIR}/bench/bench.php"
