--TEST--
Report mode covers switch/case comparison
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

%rDeprecated: php80.string_number_comparison: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
matched
