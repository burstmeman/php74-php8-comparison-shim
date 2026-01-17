--TEST--
Simulate mode returns PHP 8.0 results without reporting
--SKIPIF--
<?php
if (!extension_loaded("php74_php8_comparison_shim")) {
    echo "skip";
}
?>
--INI--
php74_php8_comparison_shim.mode=simulate
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
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
