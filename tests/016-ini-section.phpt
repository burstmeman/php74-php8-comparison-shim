--TEST--
INI section config applies extension settings
--INI--
display_errors=1
log_errors=0
error_reporting=E_ALL
[php74_php8_comparison_shim]
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.sampling_factor=1
--FILE--
<?php
$a = 0;
var_dump($a == "foo");
?>
--EXPECTF--
%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(true)
