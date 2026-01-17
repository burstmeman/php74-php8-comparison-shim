PHP_ARG_ENABLE([php74_php8_comparison_shim],
  [whether to enable php74_php8_comparison_shim],
  [AS_HELP_STRING([--enable-php74-php8-comparison-shim],
    [Enable php74_php8_comparison_shim extension])],
  [no])

PHP_ARG_ENABLE([php74_php8_comparison_shim_risky],
  [whether to enable risky simulate modes],
  [AS_HELP_STRING([--enable-php74-php8-comparison-shim-risky],
    [Enable simulate modes (risky)])],
  [no],
  [no])

if test "$PHP_PHP74_PHP8_COMPARISON_SHIM" = "yes"; then
  PHP_NEW_EXTENSION(php74_php8_comparison_shim,
    php74_php8_comparison_shim.c src/p748_cmps_modes.c src/p748_cmps_handlers.c,
    $ext_shared)
  if test "$PHP_PHP74_PHP8_COMPARISON_SHIM_RISKY" = "yes"; then
    AC_DEFINE(PHP74_PHP8_COMPARISON_SHIM_RISKY, 1, [Enable risky modes])
  else
    AC_DEFINE(PHP74_PHP8_COMPARISON_SHIM_RISKY, 0, [Enable risky modes])
  fi
fi
