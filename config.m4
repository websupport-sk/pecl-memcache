dnl
dnl $Id$
dnl

PHP_ARG_ENABLE(memcache, whether to enable memcache support,
[  --enable-memcache       Enable memcache support])

if test "$PHP_MEMCACHE" != "no"; then
  AC_DEFINE(HAVE_MEMCACHE,1,[Whether you want memcache support])
  PHP_NEW_EXTENSION(memcache, memcache.c, $ext_shared)
fi
