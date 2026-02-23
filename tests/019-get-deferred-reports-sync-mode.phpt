--TEST--
Get deferred reports returns empty list in sync mode
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.report_mode=sync
display_errors=0
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
$dummy = ($a == "foo");
$reports = php74_php8_cmps_get_deferred_reports();
var_dump($reports === []);
?>
--EXPECT--
bool(true)
