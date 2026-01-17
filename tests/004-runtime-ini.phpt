--TEST--
Runtime ini_set cannot change mode
--SKIPIF--
<?php
if (!extension_loaded("php74_php8_comparison_shim")) {
    echo "skip";
}
?>
--INI--
php74_php8_comparison_shim.mode=off
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
$b = "foo";

var_dump(ini_set("php74_php8_comparison_shim.mode", "report"));
var_dump($a == $b);
?>
--EXPECT--
bool(false)
bool(true)
