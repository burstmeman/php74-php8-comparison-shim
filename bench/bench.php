<?php

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This benchmark must be run via CLI.\n");
    exit(1);
}

// getenv() returns false when unset. '0' is falsy in PHP, so a naive
// `getenv(...) ?: default` silently ignores an explicit '0' value.
// These helpers compare strings explicitly to avoid that pitfall.
function env_int(string $key, int $default): int
{
    $raw = getenv($key);
    return $raw !== false ? (int) $raw : $default;
}

function env_bool(string $key, bool $default): bool
{
    $raw = getenv($key);
    if ($raw === false) {
        return $default;
    }
    return filter_var($raw, FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE) ?? $default;
}

$iterations = env_int('SNC_ITERATIONS', 1_000_000);
$case       = getenv('SNC_CASE') ?: 'compare';
$allowThrow = env_bool('SNC_ALLOW_THROW', false);

// ── Standard cases ─────────────────────────────────────────────────────────
// Each case is a list of [left, right] pairs run in the comparison loop.
// Using a fixed set of pairs per case keeps the loop hot in the CPU cache
// so we measure extension overhead, not memory access patterns.
$cases = [

    // Realistic mix: half of the pairs are intercepted (result differs between
    // PHP 7.4 and PHP 8.0), half are numeric strings that are never intercepted.
    'compare' => [
        [0,  'foo'],   // intercepted: PHP7 true (0==0), PHP8 false ("0"!="foo")
        [0,  '0'],     // skipped:     numeric string, both versions agree
        [42, '42foo'], // intercepted: PHP7 true (42==42), PHP8 false ("42"!="42foo")
        [42, ' 42'],   // skipped:     leading-space numeric, both versions agree
    ],

    // All numeric strings — the type-gate in the handler rejects them immediately.
    // Measures the bare opcode-handler dispatch cost with no actual interception.
    'opcode_overhead' => [
        [0,  '0'],
        [0,  '0'],
        [42, '42'],
        [42, '42'],
    ],

    // All non-numeric strings where the result differs — every comparison is
    // intercepted and, in sync report mode, triggers a zend_error(E_DEPRECATED).
    // Measures the full interception path including inline reporting.
    'report_cost' => [
        [0,  'foo'],
        [0,  'bar'],
        [42, '42foo'],
        [42, 'bar'],
    ],

    // Same pairs as report_cost, used with report_mode=defer.
    // The comparison loop only buffers entries; reporting is flushed explicitly
    // after the loop so comparison and reporting costs can be timed separately.
    // All four pairs land on the same source line (inside the foreach), so the
    // buffer holds a single unique file:line entry — the minimal-buffer baseline.
    'defer_report' => [
        [0,  'foo'],
        [0,  'bar'],
        [42, '42foo'],
        [42, 'bar'],
    ],
];

if (!isset($cases[$case])) {
    fwrite(STDERR, "Unknown benchmark case: {$case}\n");
    fwrite(STDERR, "Available: " . implode(', ', array_keys($cases)) . "\n");
    exit(1);
}

$pairs = $cases[$case];

// ── Comparison loop ────────────────────────────────────────────────────────
$loopStart = hrtime(true);
for ($i = 0; $i < $iterations; $i++) {
    foreach ($pairs as [$left, $right]) {
        if ($allowThrow) {
            try {
                /** @noinspection PhpNonStrictObjectEqualityInspection */
                $left == $right;
            } catch (Throwable $e) {
                // swallow — error mode throws on every intercepted comparison
            }
        } else {
            /** @noinspection PhpNonStrictObjectEqualityInspection */
            $left == $right;
        }
    }
}
$comparisonsElapsedNs = hrtime(true) - $loopStart;

echo "comparisons_elapsed_ns={$comparisonsElapsedNs}\n";

// ── Deferred flush ─────────────────────────────────────────────────────────
// Only for defer_report: flush the buffer explicitly so we can measure the
// reporting cost (zend_error calls) separately from the comparison loop cost.
// Without an explicit flush, entries are discarded at RSHUTDOWN and the
// reporting cost is folded into the shell-level total time with no breakdown.
if ($case === 'defer_report' && function_exists('php74_php8_cmps_flush_deferred')) {
    $flushStart = hrtime(true);
    php74_php8_cmps_flush_deferred();
    $flushElapsedNs = hrtime(true) - $flushStart;
    echo "flush_elapsed_ns={$flushElapsedNs}\n";
}
