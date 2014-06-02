PHP_ARG_ENABLE(xio, xio extension for php,
[ --enable-xio   Enable xio extension])

if test "$PHP_XIO" = "yes"; then
  AC_DEFINE(HAVE_XIO, 1, [Whether you have xio])
  PHP_ADD_LIBRARY(xio, 1, XIO_SHARED_LIBADD)
  PHP_SUBST(XIO_SHARED_LIBADD)
  PHP_NEW_EXTENSION(xio, php_xio.c, $ext_shared)
fi
