--TEST--
Simulate and report mode emits deprecations and returns PHP 8.0 results
--SKIPIF--
<?php
if (!extension_loaded("php74_php8_comparison_shim")) {
    echo "skip";
}
?>
--INI--
php74_php8_comparison_shim.mode=simulate_and_report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
$b = 42;
var_dump($a == "0");
var_dump($a == "0.0");
var_dump($a == "foo");
var_dump($a == "");
var_dump($b == " 42");
var_dump($b == "42foo");
?>
--EXPECTF--
bool(true)
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(false)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "" using == in .+ on line \d+%r
bool(false)
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "42" and "42foo" using == in .+ on line \d+%r
bool(false)
