--TEST--
Report mode emits deprecations for changed comparisons
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
$a = 0;
$b = 42;
var_dump($a == "0");
var_dump($a == "0.0");
var_dump($a == "foo");
var_dump($a == "");
var_dump($b == " 42");
var_dump($b == "42foo");
?>
--EXPECTF--
bool(true)
bool(true)

%rDeprecated: php80.string_number_comparison: Non-strict comparison between "0" and "foo" using == in .+ on line \d+%r
bool(true)

%rDeprecated: php80.string_number_comparison: Non-strict comparison between "0" and "" using == in .+ on line \d+%r
bool(true)
bool(true)

%rDeprecated: php80.string_number_comparison: Non-strict comparison between "42" and "42foo" using == in .+ on line \d+%r
bool(true)
