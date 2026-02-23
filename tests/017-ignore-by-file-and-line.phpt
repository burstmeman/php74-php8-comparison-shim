--TEST--
Ignore comparison checks by file suffix and line
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
php74_php8_cmps_set_ignored_locations([
    basename(__FILE__) . ":" . 7,
    basename(__FILE__) . ":" . 9,
]);
$a = 0;
var_dump($a == "foo"); // Line 7, ignored
var_dump($a == "bar"); // Line 8, not ignored
var_dump($a == "baz"); // Line 9, ignored
?>
--EXPECTF--
bool(true)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "0" and "bar" using == in .+ on line \d+%r
bool(true)
bool(true)