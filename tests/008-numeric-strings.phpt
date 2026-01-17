--TEST--
Report mode does not warn for numeric strings
--SKIPIF--
<?php
if (!extension_loaded("php80_string_number_comparison")) {
    echo "skip";
}
?>
--INI--
php80.string_number_comparison=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$n = 42;
var_dump($n == "42");
var_dump($n == " 42");
var_dump($n == "42.0");
var_dump($n == "4.2e1");
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
