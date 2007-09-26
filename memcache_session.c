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
#include "ext/standard/php_smart_str.h"
#include "ext/standard/url.h"
#include "php_memcache.h"

ZEND_EXTERN_MODULE_GLOBALS(memcache)

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

	pool = mmc_pool_new(TSRMLS_C);

	for (i=0,j=0,path_len=strlen(save_path); i<path_len; i=j+1) {
		/* find beginning of url */
		while (i<path_len && (isspace(save_path[i]) || save_path[i] == ',')) {
			i++;
		}

		/* find end of url */
		j = i;
		while (j<path_len && !isspace(save_path[j]) && save_path[j] != ',') {
			 j++;
		}

		if (i < j) {
			int persistent = 0, udp_port = 0, weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
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

				if (zend_hash_find(Z_ARRVAL_P(params), "udp_port", sizeof("udp_port"), (void **) &param) != FAILURE) {
					convert_to_long_ex(param);
					udp_port = Z_LVAL_PP(param);
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
				mmc = mmc_find_persistent(url->host, strlen(url->host), url->port, udp_port, timeout, retry_interval TSRMLS_CC);
			}
			else {
				mmc = mmc_server_new(url->host, strlen(url->host), url->port, udp_port, 0, timeout, retry_interval TSRMLS_CC);
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

	if (pool != NULL) {
		zval result;
		mmc_request_t *request;
		ZVAL_FALSE(&result);

		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_UDP, mmc_request_parse_value, 
			mmc_value_handler_single, &result, mmc_pool_failover_handler, NULL TSRMLS_CC);

		if (mmc_prepare_key_ex(key, strlen(key), request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			return FAILURE;
		}

		smart_str_appendl(&(request->sendbuf.value), "get ", sizeof("get ")-1);
		smart_str_appendl(&(request->sendbuf.value), request->key, request->key_len);
		smart_str_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);

		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, 1 TSRMLS_CC) != MMC_OK) {
			return FAILURE;
		}
	
		/* execute request */
		mmc_pool_run(pool TSRMLS_CC);
		
		if (Z_TYPE(result) == IS_STRING) {
			*val = Z_STRVAL(result);
			*vallen = Z_STRLEN(result);
			return SUCCESS;
		}

		zval_dtor(&result);
	}

	return FAILURE;
}
/* }}} */

/* {{{ PS_WRITE_FUNC
 */
PS_WRITE_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();
	
	if (pool != NULL) {
		zval result, value;
		mmc_request_t *request;

		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_request_parse_line, 
			mmc_stored_handler, &result, mmc_pool_failover_handler, NULL TSRMLS_CC);
		
		ZVAL_NULL(&result);
		ZVAL_STRINGL(&value, (char *)val, vallen, 0);

		/* assemble command */
		if (mmc_prepare_store(pool, request, "set", sizeof("set")-1, key, strlen(key), 0, INI_INT("session.gc_maxlifetime"), &value TSRMLS_CC) != MMC_OK) {
			mmc_pool_release(pool, request);
			return FAILURE;
		}
		
		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(session_redundancy) TSRMLS_CC) != MMC_OK) {
			return FAILURE;
		}

		/* execute request */
		mmc_pool_run(pool TSRMLS_CC);

		if (Z_BVAL(result)) {
			return SUCCESS;
		}
	}
	
	return FAILURE;
}
/* }}} */

static int mmc_deleted_handler(mmc_t *mmc, mmc_request_t *request, void *value, unsigned int value_len, void *param TSRMLS_DC) /* 
	parses a DELETED response line, param is a zval pointer to store result into {{{ */
{
	if (mmc_str_left((char *)value, "DELETED", value_len, sizeof("DELETED")-1) ||
		mmc_str_left((char *)value, "NOT_FOUND", value_len, sizeof("NOT_FOUND")-1)) 
	{
		ZVAL_TRUE((zval *)param);
		return MMC_REQUEST_DONE;
	}
	
	return mmc_request_failure(mmc, request->io, (char *)value, value_len, 0 TSRMLS_CC);
}
/* }}} */

/* {{{ PS_DESTROY_FUNC
 */
PS_DESTROY_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();

	if (pool != NULL) {
		zval result;
		mmc_request_t *request;
		
		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_request_parse_line, 
			mmc_deleted_handler, &result, mmc_pool_failover_handler, NULL TSRMLS_CC);
		ZVAL_FALSE(&result);

		if (mmc_prepare_key_ex(key, strlen(key), request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			return FAILURE;
		}

		smart_str_appendl(&(request->sendbuf.value), "delete", sizeof("delete")-1);
		smart_str_appendl(&(request->sendbuf.value), " ", 1);
		smart_str_appendl(&(request->sendbuf.value), request->key, request->key_len);
		smart_str_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);

		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(session_redundancy) TSRMLS_CC) != MMC_OK) {
			return FAILURE;
		}
	
		/* execute request */
		mmc_pool_run(pool TSRMLS_CC);

		if (Z_BVAL(result)) {
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
