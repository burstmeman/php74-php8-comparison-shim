--TEST--
Defer report mode emits deprecations at request shutdown
--INI--
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.report_mode=defer
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$a = 0;
for ($i = 0; $i < 10; $i++) {
    var_dump($a == "foo");
}
php74_php8_cmps_flush_deferred();
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "0" and "foo" using == \(repeated 10 times\) in .+ on line \d+%r
