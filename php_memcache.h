/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony2001@phpclub.net>                         |
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

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(memcache);
PHP_MSHUTDOWN_FUNCTION(memcache);
PHP_RINIT_FUNCTION(memcache);
PHP_RSHUTDOWN_FUNCTION(memcache);
PHP_MINFO_FUNCTION(memcache);

PHP_FUNCTION(memcache_connect);
PHP_FUNCTION(memcache_get_version);
PHP_FUNCTION(memcache_add);
PHP_FUNCTION(memcache_set);
PHP_FUNCTION(memcache_replace);
PHP_FUNCTION(memcache_get);
PHP_FUNCTION(memcache_delete);
PHP_FUNCTION(memcache_debug);
PHP_FUNCTION(memcache_get_stats);
PHP_FUNCTION(memcache_increment);
PHP_FUNCTION(memcache_decrement);

#define MMC_BUF_SIZE 4096
#define MMC_SERIALIZED 1
#define MMC_COMPRESSED 2
#define MMC_DEFAULT_TIMEOUT 1 /* seconds */
#define MMC_KEY_MAX_SIZE 250 /* stoled from memcached sources =) */

typedef struct mmc {
	int						id;
	php_stream				*stream;
	char					inbuf[MMC_BUF_SIZE];
	long					timeout;
} mmc_t;

/* our globals */
typedef struct {
	long debug_mode;
} php_memcache_globals;

#ifdef ZTS
#define MEMCACHE_G(v) TSRMG(memcache_globals_id, zend_memcache_globals *, v)
#else
#define MEMCACHE_G(v) (memcache_globals.v)
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
