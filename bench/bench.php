<?php

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This benchmark must be run via CLI.\n");
    exit(1);
}

$iterations = (int) (getenv('SNC_ITERATIONS') ?: 1000000);
$case = getenv('SNC_CASE') ?: 'compare';
$measureInternal = (int) (getenv('SNC_MEASURE_INTERNAL') ?: 1) === 1;
$silence = (int) (getenv('SNC_SILENCE') ?: 1) === 1;
$allowThrow = (int) (getenv('SNC_ALLOW_THROW') ?: 0) === 1;

if ($silence) {
    ini_set('display_errors', '0');
    ini_set('log_errors', '0');
    ini_set('html_errors', '0');
}

$cases = [
    'compare' => [
        [0, "foo"],
        [0, "0"],
        [42, "42foo"],
        [42, " 42"],
    ],
    'opcode_overhead' => [
        [0, "0"],
        [0, "0"],
        [42, "42"],
        [42, "42"],
    ],
    'deprecated_cost' => [
        [0, "foo"],
        [0, "bar"],
        [42, "42foo"],
        [42, "bar"],
    ],
    'defer_report' => [
        [0, "foo"],
        [0, "bar"],
        [42, "42foo"],
        [42, "bar"],
    ],
];

if (!isset($cases[$case])) {
    fwrite(STDERR, "Unknown benchmark case: {$case}\n");
    exit(1);
}

$pairs = $cases[$case];

$start = $measureInternal ? hrtime(true) : 0;
for ($i = 0; $i < $iterations; $i++) {
    foreach ($pairs as [$left, $right]) {
        if ($allowThrow) {
            try {
                $left == $right;
            } catch (Throwable $e) {
                // Ignore errors in benchmark for error mode.
            }
        } else {
            $left == $right;
        }
    }
}
$elapsedNs = $measureInternal ? (hrtime(true) - $start) : 0;

echo "elapsed_ns={$elapsedNs}\n";
