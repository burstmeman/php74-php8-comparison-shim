--TEST--
Runtime ini_set cannot change mode
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
$a = 0;
$b = "foo";

var_dump(ini_set("php80.string_number_comparison", "report"));
var_dump($a == $b);
?>
--EXPECT--
bool(false)
bool(true)
