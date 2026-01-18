--TEST--
Defer report mode warns once when buffer limit is reached
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.report_mode=defer
php74_php8_comparison_shim.report_limit=1
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
var_dump($a == "foo");
var_dump($a == "bar");
?>
--EXPECTF--
bool(true)
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r

Warning: php74_php8_comparison_shim.report_mode=defer: report buffer full, dropping further reports in Unknown on line 0
