/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Antony Dovgal <tony@daylessday.org>                         |
  |          Mikael Johansson <mikael AT synd DOT info>                  |
  |                                                                      |
  | Port to PHP7: Szabolcs Balogh <baloghsz@szabi.org>                   |
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
#include "session/php_session.h"

#include "SAPI.h"
#include "ext/standard/url.h"
#include "php_memcache.h"

#if HAVE_MEMCACHE_SESSION
#include "ext/session/php_session.h"
#endif

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
	zval params, *param;
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
			zend_string *host;
			
			/* translate unix: into file: */
			if (!strncmp(save_path+i, "unix:", sizeof("unix:")-1)) {
				int len = j-i;
				char *path = estrndup(save_path+i, len);
				memcpy(path, "file:", sizeof("file:")-1);
				url = php_url_parse_ex(path, len);
				efree(path);
			}
			else {
				url = php_url_parse_ex(save_path+i, j-i);
			}

			if (!url) {
				char *path = estrndup(save_path+i, j-i);
				php_error_docref(NULL, E_WARNING,
					"Failed to parse session.save_path (error at offset %d, url was '%s')", i, path);
				efree(path);
				
				mmc_pool_free(pool);
				PS_SET_MOD_DATA(NULL);
				return FAILURE;
			}
			
			/* parse parameters */
			if (url->query != NULL) {
				array_init(&params);

				sapi_module.treat_data(PARSE_STRING, estrdup(url->query), &params);

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "persistent", sizeof("persistent")-1)) != NULL) {
					convert_to_boolean_ex(param);
					persistent = Z_TYPE_P(param)==IS_TRUE;
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "weight", sizeof("weight")-1)) != NULL) {
					convert_to_long_ex(param);
					weight = Z_LVAL_P(param);
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "timeout", sizeof("timeout"))) != NULL) {
					convert_to_long_ex(param);
					timeout = Z_LVAL_P(param);
				}

				if ((param =zend_hash_str_find(Z_ARRVAL(params), "retry_interval", sizeof("retry_interval"))) != NULL) {
					convert_to_long_ex(param);
					retry_interval = Z_LVAL_P(param);
				}

				zval_ptr_dtor(&params);
			}
			
			if (url->scheme && url->path && !strcmp(url->scheme, "file")) {
				host = strpprintf(0, "unix://%s", url->path);

				/* chop off trailing :0 port specifier */
				if (!strcmp(ZSTR_VAL(host) + ZSTR_LEN(host) - 2, ":0")) {
					host = zend_string_realloc(host, ZSTR_LEN(host)-2, 0);
					ZSTR_VAL(host)[ZSTR_LEN(host)] = '\0';
					zend_string_forget_hash_val(host);
				}
				
				if (persistent) {
					mmc = mmc_find_persistent(host, 0, timeout, retry_interval);
				}
				else {
					mmc = mmc_server_new(host, 0, 0, timeout, retry_interval);
				}
				
				zend_string_release(host);
			}
			else {
				if (url->host == NULL || weight <= 0 || timeout <= 0) {
					php_url_free(url);
					mmc_pool_free(pool);
					PS_SET_MOD_DATA(NULL);
					return FAILURE;
				}

				host = zend_string_init(url->host, strlen(url->host), 0);

				if (persistent) {
					mmc = mmc_find_persistent(host, url->port, timeout, retry_interval);
				}
				else {
					mmc = mmc_server_new(host, url->port, 0, timeout, retry_interval);
				}

				zend_string_release(host);
			}

			mmc_pool_add(pool, mmc, weight);
			php_url_free(url);
		}
	}

	if (pool->num_servers) {
		PS_SET_MOD_DATA(pool);
		return SUCCESS;
	}

	mmc_pool_free(pool);
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
		mmc_pool_free(pool);
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
	zval result;

	if (pool) {
		char key_tmp[MMC_KEY_MAX_SIZE];
		unsigned int key_tmp_len;

		if (mmc_prepare_key_ex(key, key_tmp, &key_tmp_len) != MMC_OK) {
			return FAILURE;
		}

		ZVAL_NULL(&result);

		if (mmc_exec_retrieval_cmd(pool, key_tmp, key_tmp_len, &result, NULL) <= 0 || Z_TYPE(result) != IS_STRING) {
			zval_ptr_dtor(&result);
			return FAILURE;
		}

		*val = zend_string_init(Z_STRVAL(result), Z_STRLEN(result), 1);
		zval_ptr_dtor(&result);
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

	if (pool) { 
		char key_tmp[MMC_KEY_MAX_SIZE];
		unsigned int key_tmp_len;

		if (mmc_prepare_key_ex(key, key_tmp, &key_tmp_len) != MMC_OK) {
			return FAILURE;
		}			
		
		if (mmc_pool_store(pool, "set", sizeof("set")-1, key_tmp, key_tmp_len, 0, INI_INT("session.gc_maxlifetime"), ZSTR_VAL(val), ZSTR_LEN(val))) {
			return SUCCESS;
		}
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
		char key_tmp[MMC_KEY_MAX_SIZE];
		unsigned int key_tmp_len;

		if (mmc_prepare_key_ex(key, key_tmp, &key_tmp_len) != MMC_OK) {
			return FAILURE;
		}
		
		while (result < 0 && (mmc = mmc_pool_find(pool, key_tmp, key_tmp_len)) != NULL) {
			if ((result = mmc_delete(mmc, key_tmp, key_tmp_len, 0)) < 0) {
				mmc_server_failure(mmc);
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
