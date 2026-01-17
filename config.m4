PHP_ARG_ENABLE(php80_string_number_comparison, whether to enable php80_string_number_comparison,
  [  --enable-php80-string-number-comparison   Enable php80_string_number_comparison extension], no)

if test "$PHP_PHP80_STRING_NUMBER_COMPARISON" != "no"; then
  PHP_NEW_EXTENSION(php80_string_number_comparison, php80_string_number_comparison.c, $ext_shared,, -g -O0)
fi
