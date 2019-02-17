dnl config.m4 for extension overload

PHP_ARG_ENABLE(overload, whether to enable overload support,
[  --enable-overload          Enable overload support], no)

if test "$PHP_OVERLOAD" != "no"; then
  PHP_NEW_EXTENSION(overload, overload.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi