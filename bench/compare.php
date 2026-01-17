<?php

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This benchmark must be run via CLI.\n");
    exit(1);
}

$iterations = (int) (getenv('SNC_ITERATIONS') ?: 5000000);

$a = 0;
$b = "foo";
$c = "0";
$d = "42foo";
$e = 42;

for ($i = 0; $i < $iterations; $i++) {
    try {
        $a == $b;
        $a == $c;
        $e == $d;
        $e == " 42";
    } catch (Throwable $e) {
        // Ignore errors in benchmark for error mode.
    }
}
