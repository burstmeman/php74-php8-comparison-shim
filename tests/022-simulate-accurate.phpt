--TEST--
Simulate mode only changes result when PHP 7.4 and PHP 8.0 disagree
--INI--
php74_php8_comparison_shim.mode=simulate
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
// Use variables to prevent compile-time constant folding.
$zero = 0;
$one = 1;
$n42 = 42;

// DIFFERS: PHP 7.4 (0 == 0) = true; PHP 8.0 ("0" == "foo") = false — simulate returns false
var_dump($zero == "foo");
// SAME:    PHP 7.4 (1 == 0) = false; PHP 8.0 ("1" == "foo") = false — no change, returns false
var_dump($one == "foo");
// DIFFERS: PHP 7.4 (0 < 0) = false; PHP 8.0 ("0" < "foo") = true — simulate returns true
var_dump($zero < "foo");
// SAME:    PHP 7.4 (0 <= 0) = true; PHP 8.0 ("0" <= "foo") = true — no change, returns true
var_dump($zero <= "foo");
// SAME:    PHP 7.4 (0 > 0) = false; PHP 8.0 ("0" > "foo") = false — no change, returns false
var_dump($zero > "foo");
// DIFFERS: PHP 7.4 (42 == 42) = true; PHP 8.0 ("42" == "42foo") = false — simulate returns false
var_dump($n42 == "42foo");
?>
--EXPECT--
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
