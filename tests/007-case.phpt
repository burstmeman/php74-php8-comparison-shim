--TEST--
Report mode covers switch/case comparison
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$n = 0;
switch ($n) {
    case "foo":
        echo "matched\n";
        break;
    default:
        echo "default\n";
}
?>
--EXPECTF--

%rDeprecated: php74_php8_comparison_shim.mode: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
matched
