--TEST--
Get deferred reports returns list of maps with string keys
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.report_mode=defer
display_errors=0
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
$dummy = ($a == "foo");
$reports = php74_php8_cmps_get_deferred_reports();
var_dump(is_array($reports));
var_dump(count($reports) >= 1);
$r = $reports[0];
var_dump(array_keys($r) === ["filename", "line", "entry_count", "operator", "left_op", "right_op"]);
var_dump($r["operator"] === "==");
var_dump($r["left_op"] === "0");
var_dump($r["right_op"] === "foo");
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
