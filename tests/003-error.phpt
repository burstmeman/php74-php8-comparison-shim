--TEST--
Error mode throws on changed comparisons
--SKIPIF--
<?php
if (!extension_loaded("php80_string_number_comparison")) {
    echo "skip";
}
?>
--INI--
php80.string_number_comparison=error
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
function check_case($left, $right) {
    try {
        $result = ($left == $right);
        echo "ok: ";
        var_dump($result);
    } catch (Throwable $e) {
        echo "error: " . $e->getMessage() . "\n";
    }
}

check_case(0, "0");
check_case(0, "0.0");
check_case(0, "foo");
check_case(0, "");
check_case(42, " 42");
check_case(42, "42foo");
?>
--EXPECTF--
ok: bool(true)
ok: bool(true)
error: php80.string_number_comparison: Non-strict comparison between "0" and "foo" using ==
error: php80.string_number_comparison: Non-strict comparison between "0" and "" using ==
ok: bool(true)
error: php80.string_number_comparison: Non-strict comparison between "42" and "42foo" using ==
