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
  | Authors: Antony Dovgal <tony2001@phpclub.net>                        |
  |          Mikael Johansson <mikael AT synd DOT info>                  |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include "php.h"
#include "php_ini.h"
#include "php_variables.h"

#include "SAPI.h"
#include "ext/standard/url.h"
#include "php_memcache.h"

#if HAVE_MEMCACHE_SESSION

ps_module ps_mod_memcache = {
	PS_MOD(memcache)
};

/* {{{ PS_OPEN_FUNC
 */
PS_OPEN_FUNC(memcache)
{
	mmc_pool_t *pool;
	mmc_t *mmc;

	php_url *url;
	zval *params, **param;
	int i, j, path_len;

	pool = mmc_pool_new();

	for (i=0,j=0,path_len=strlen(save_path); i<path_len; i=j+1) {
		/* find beginning of url */
		while (i<path_len && (isspace(save_path[i]) || save_path[i] == ','))
			i++;

		/* find end of url */
		j = i;
		while (j<path_len && !isspace(save_path[j]) && save_path[j] != ',')
			 j++;

		if (i < j) {
			int persistent = 0, weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
			url = php_url_parse_ex(save_path+i, j-i);

			/* parse parameters */
			if (url->query != NULL) {
				MAKE_STD_ZVAL(params);
				array_init(params);

				sapi_module.treat_data(PARSE_STRING, estrdup(url->query), params TSRMLS_CC);

				if (zend_hash_find(Z_ARRVAL_P(params), "persistent", sizeof("persistent"), (void **) &param) != FAILURE) {
					convert_to_boolean_ex(param);
					persistent = Z_BVAL_PP(param);
				}

				if (zend_hash_find(Z_ARRVAL_P(params), "weight", sizeof("weight"), (void **) &param) != FAILURE) {
					convert_to_long_ex(param);
					weight = Z_LVAL_PP(param);
				}

				if (zend_hash_find(Z_ARRVAL_P(params), "timeout", sizeof("timeout"), (void **) &param) != FAILURE) {
					convert_to_long_ex(param);
					timeout = Z_LVAL_PP(param);
				}

				if (zend_hash_find(Z_ARRVAL_P(params), "retry_interval", sizeof("retry_interval"), (void **) &param) != FAILURE) {
					convert_to_long_ex(param);
					retry_interval = Z_LVAL_PP(param);
				}

				zval_ptr_dtor(&params);
			}

			if (url->host == NULL || weight <= 0 || timeout <= 0) {
				php_url_free(url);
				mmc_pool_free(pool TSRMLS_CC);
				PS_SET_MOD_DATA(NULL);
				return FAILURE;
			}

			if (persistent) {
				mmc = mmc_find_persistent(url->host, strlen(url->host), url->port, timeout, retry_interval TSRMLS_CC);
			}
			else {
				mmc = mmc_server_new(url->host, strlen(url->host), url->port, 0, timeout, retry_interval TSRMLS_CC);
			}

			mmc_pool_add(pool, mmc, 1);
			php_url_free(url);
		}
	}

	if (pool->num_servers) {
		PS_SET_MOD_DATA(pool);
		return SUCCESS;
	}

	mmc_pool_free(pool TSRMLS_CC);
	PS_SET_MOD_DATA(NULL);
	return FAILURE;
}
/* }}} */

/* {{{ PS_CLOSE_FUNC
 */
PS_CLOSE_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();

	if (pool) {
		mmc_pool_free(pool TSRMLS_CC);
		PS_SET_MOD_DATA(NULL);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PS_READ_FUNC
 */
PS_READ_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();
	zval *result;

	if (pool) {
		MAKE_STD_ZVAL(result);
		ZVAL_NULL(result);

		if (mmc_exec_retrieval_cmd(pool, key, strlen(key), &result TSRMLS_CC) <= 0 || Z_TYPE_P(result) != IS_STRING) {
			zval_ptr_dtor(&result);
			return FAILURE;
		}

		*val = Z_STRVAL_P(result);
		*vallen = Z_STRLEN_P(result);
		FREE_ZVAL(result);
		return SUCCESS;
	}

	return FAILURE;
}
/* }}} */

/* {{{ PS_WRITE_FUNC
 */
PS_WRITE_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();

	if (pool && mmc_pool_store(pool, "set", sizeof("set")-1, key, strlen(key), 0, INI_INT("session.gc_maxlifetime"), val, vallen TSRMLS_CC)) {
		return SUCCESS;
	}

	return FAILURE;
}
/* }}} */

/* {{{ PS_DESTROY_FUNC
 */
PS_DESTROY_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();
	mmc_t *mmc;

	int result = -1;

	if (pool) {
		while (result < 0 && (mmc = mmc_pool_find(pool, key, strlen(key) TSRMLS_CC)) != NULL) {
			if ((result = mmc_delete(mmc, key, strlen(key), 0 TSRMLS_CC)) < 0 && mmc_server_failure(mmc TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", mmc->host, mmc->port);
			}
		}

		if (result >= 0) {
			return SUCCESS;
		}
	}

	return FAILURE;
}
/* }}} */

/* {{{ PS_GC_FUNC
 */
PS_GC_FUNC(memcache)
{
	return SUCCESS;
}
/* }}} */

#endif /* HAVE_MEMCACHE_SESSION */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
