--TEST--
Ordering operators warn when result differs between PHP 7.4 and PHP 8.0
--INI--
php74_php8_comparison_shim.mode=report
display_errors=1
log_errors=0
error_reporting=E_ALL
--FILE--
<?php
$n = 1;
// PHP 7.4: 1 < 0 = false (non-numeric "foo" -> 0); PHP 8.0: "1" < "foo" = true — differ
var_dump($n < "foo");
// PHP 7.4: 1 > 0 = true;  PHP 8.0: "1" > "foo" = false — differ
// Note: PHP compiles $n > "foo" as IS_SMALLER("foo", $n), so operands are swapped in message
var_dump($n > "foo");
// PHP 7.4: 1 <= 0 = false; PHP 8.0: "1" <= "foo" = true — differ
var_dump($n <= "foo");
// PHP 7.4: 1 >= 0 = true;  PHP 8.0: "1" >= "foo" = false — differ
// Note: PHP compiles $n >= "foo" as IS_SMALLER_OR_EQUAL("foo", $n)
var_dump($n >= "foo");
// PHP 7.4: 1 <=> 0 = 1;   PHP 8.0: "1" <=> "foo" = -1 — differ
var_dump($n <=> "foo");
?>
--EXPECTF--

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "1" and "foo" using < in .+ on line \d+%r
bool(false)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "foo" and "1" using < in .+ on line \d+%r
bool(true)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "1" and "foo" using <= in .+ on line \d+%r
bool(false)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "foo" and "1" using <= in .+ on line \d+%r
bool(true)

%rDeprecated: php74_php8_comparison_shim: Non-strict comparison between "1" and "foo" using <=> in .+ on line \d+%r
int(1)
