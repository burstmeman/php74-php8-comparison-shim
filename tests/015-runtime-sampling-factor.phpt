--TEST--
Runtime sampling factor setter applies to reporting
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
echo "Sampling factor setup result: " . var_export(php74_php8_cmps_set_sampling(2), true) . PHP_EOL;

$a = 0;
var_dump($a == "foo");
var_dump($a == "foo");
var_dump($a == "foo");
var_dump($a == "foo");
?>
--EXPECTF--
Sampling factor setup result: true
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(true)
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(true)