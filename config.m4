PHP_ARG_ENABLE(php74_php8_comparison_shim, whether to enable php74_php8_comparison_shim,
  [  --enable-php74-php8-comparison-shim   Enable php74_php8_comparison_shim extension], no)

if test "$PHP_PHP74_PHP8_COMPARISON_SHIM" != "no"; then
  PHP_NEW_EXTENSION(php74_php8_comparison_shim, php74_php8_comparison_shim.c, $ext_shared,, -g -O0)
fi
