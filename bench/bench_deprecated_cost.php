<?php

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This benchmark must be run via CLI.\n");
    exit(1);
}

$iterations = (int) (getenv('SNC_ITERATIONS') ?: 1000000);

$a = 0;
$b = "foo";
$c = 42;
$d = "42foo";

for ($i = 0; $i < $iterations; $i++) {
    $a == $b;
    $c == $d;
}
