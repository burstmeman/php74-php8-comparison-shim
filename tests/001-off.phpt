--TEST--
Off mode disables reporting
--SKIPIF--
<?php
if (!extension_loaded("php80_string_number_comparison")) {
    echo "skip";
}
?>
--INI--
php80.string_number_comparison=off
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
var_dump(0 == "0");
var_dump(0 == "0.0");
var_dump(0 == "foo");
var_dump(0 == "");
var_dump(42 == " 42");
var_dump(42 == "42foo");
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
