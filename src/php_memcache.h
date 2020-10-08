/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Antony Dovgal <tony2001@phpclub.net>                        |
  |          Mikael Johansson <mikael AT synd DOT info>                  |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_MEMCACHE_H
#define PHP_MEMCACHE_H

extern zend_module_entry memcache_module_entry;
#define phpext_memcache_ptr &memcache_module_entry

#ifdef PHP_WIN32
#define PHP_MEMCACHE_API __declspec(dllexport)
#else
#define PHP_MEMCACHE_API
#endif

#include "memcache_pool.h"

PHP_RINIT_FUNCTION(memcache);
PHP_MINIT_FUNCTION(memcache);
PHP_MSHUTDOWN_FUNCTION(memcache);
PHP_MINFO_FUNCTION(memcache);

PHP_NAMED_FUNCTION(zif_memcache_pool_connect);
PHP_NAMED_FUNCTION(zif_memcache_pool_addserver);
PHP_NAMED_FUNCTION(zif_memcache_pool_findserver);

PHP_FUNCTION(memcache_connect);
PHP_FUNCTION(memcache_pconnect);
PHP_FUNCTION(memcache_add_server);
PHP_FUNCTION(memcache_set_server_params);
PHP_FUNCTION(memcache_set_failure_callback);
PHP_FUNCTION(memcache_get_server_status);
PHP_FUNCTION(memcache_get_version);
PHP_FUNCTION(memcache_add);
PHP_FUNCTION(memcache_set);
PHP_FUNCTION(memcache_replace);
PHP_FUNCTION(memcache_cas);
PHP_FUNCTION(memcache_append);
PHP_FUNCTION(memcache_prepend);
PHP_FUNCTION(memcache_get);
PHP_FUNCTION(memcache_delete);
PHP_FUNCTION(memcache_debug);
PHP_FUNCTION(memcache_get_stats);
PHP_FUNCTION(memcache_get_extended_stats);
PHP_FUNCTION(memcache_set_compress_threshold);
PHP_FUNCTION(memcache_increment);
PHP_FUNCTION(memcache_decrement);
PHP_FUNCTION(memcache_close);
PHP_FUNCTION(memcache_flush);
PHP_FUNCTION(memcache_set_sasl_auth_data);

#define PHP_MEMCACHE_VERSION "4.0.5.2"

#define MMC_DEFAULT_TIMEOUT 1				/* seconds */
#define MMC_DEFAULT_RETRY 15 				/* retry failed server after x seconds */
#define MMC_DEFAULT_CACHEDUMP_LIMIT	100		/* number of entries */

#define MEMCACHE_IS_CALLABLE(cb_zv, flags, cb_sp) zend_is_callable((cb_zv), (flags), (cb_sp))
#define MEMCACHE_LIST_INSERT(list, val) zend_list_insert((list), (val))

/* internal functions */
mmc_t *mmc_find_persistent(const char *, int, unsigned short, unsigned short, double, int);
int mmc_value_handler_single(const char *, unsigned int, zval *, unsigned int, unsigned long, void *);
int mmc_value_handler_multi(const char *, unsigned int, zval *, unsigned int, unsigned long, void *);
int mmc_stored_handler(mmc_t *, mmc_request_t *, int, const char *, unsigned int, void *);
int mmc_numeric_response_handler(mmc_t *, mmc_request_t *, int, const char *, unsigned int, void *);

/* session handler struct */
#if HAVE_MEMCACHE_SESSION
#include "ext/session/php_session.h"

extern ps_module ps_mod_memcache;
#define ps_memcache_ptr &ps_mod_memcache

PS_FUNCS(memcache);
#endif

#endif	/* PHP_MEMCACHE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
