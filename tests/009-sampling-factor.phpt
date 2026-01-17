--TEST--
Sampling factor reduces report frequency
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.sampling_factor=2
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$n = 0;
$s = "foo";

var_dump($n == $s);
var_dump($n == $s);
var_dump($n == $s);
?>
--EXPECTF--
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(true)
bool(true)
