--TEST--
No warning when PHP 7.4 and PHP 8.0 produce the same result
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
// 0 <= "foo": PHP 7.4 (0 <= 0) = true, PHP 8.0 ("0" <= "foo") = true — same
var_dump(0 <= "foo");
// 0 > "foo": PHP 7.4 (0 > 0) = false, PHP 8.0 ("0" > "foo") = false — same
var_dump(0 > "foo");
// 1 == "foo": PHP 7.4 (1 == 0) = false, PHP 8.0 ("1" == "foo") = false — same
var_dump(1 == "foo");
// 2 != "bar": PHP 7.4 (2 != 0) = true, PHP 8.0 ("2" != "bar") = true — same
var_dump(2 != "bar");
// -1 == "foo": PHP 7.4 (-1 == 0) = false, PHP 8.0 ("-1" == "foo") = false — same
var_dump(-1 == "foo");
// 5 == "": PHP 7.4 (5 == 0) = false, PHP 8.0 ("5" == "") = false — same
var_dump(5 == "");
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
