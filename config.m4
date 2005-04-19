dnl
dnl $Id$
dnl

PHP_ARG_ENABLE(memcache, whether to enable memcache support,
[  --enable-memcache       Enable memcache support])

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for the location of libz,
[  --with-zlib-dir[=DIR]   memcache: Set the path to libz install prefix.], no, no)
fi

if test "$PHP_MEMCACHE" != "no"; then

  dnl # zlib
  AC_MSG_CHECKING([for the location of zlib])
  if test "$PHP_ZLIB_DIR" = "no"; then
    AC_MSG_ERROR([memcache support requires ZLIB. Use --with-zlib-dir=<DIR>])
  else
    AC_MSG_RESULT([$PHP_ZLIB_DIR])
    PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/lib, MEMCACHE_SHARED_LIBADD)
  fi

  AC_DEFINE(HAVE_MEMCACHE,1,[Whether you want memcache support])
  PHP_NEW_EXTENSION(memcache, memcache.c, $ext_shared)
fi


