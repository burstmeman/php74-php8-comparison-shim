--TEST--
Defer report mode emits deprecations at request shutdown
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.report_mode=defer
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
var_dump($a == "foo");
var_dump($a == "foo");
?>
--EXPECTF--
bool(true)
bool(true)

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+ in Unknown on line 0%r

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+ in Unknown on line 0%r
