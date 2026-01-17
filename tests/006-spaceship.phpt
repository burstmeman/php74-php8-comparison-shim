--TEST--
Report mode covers spaceship operator
--SKIPIF--
<?php
if (!extension_loaded("php74_php8_comparison_shim")) {
    echo "skip";
}
?>
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$n = 0;
$s = "foo";
var_dump($n <=> $s);
?>
--EXPECTF--

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using <=> in .+ on line \d+%r
int(0)
