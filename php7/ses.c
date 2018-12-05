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

/* $Id: memcache_session.c 326387 2012-06-30 17:41:36Z ab $ */

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

#ifdef PHP_WIN32
void usleep(int waitTime) { 
    __int64 time1 = 0, time2 = 0, freq = 0; 
	 
	QueryPerformanceCounter((LARGE_INTEGER *) &time1); 
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq); 
			  
	do { 
		QueryPerformanceCounter((LARGE_INTEGER *) &time2); 
	} while((time2-time1) < waitTime); 
} 

#endif

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
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Failed to parse session.save_path (error at offset %d, url was '%s')", i, path);
				efree(path);

				mmc_pool_free(pool TSRMLS_CC);
				PS_SET_MOD_DATA(NULL);
				return FAILURE;
			}

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

			if (url->scheme && url->path && !strcmp(url->scheme, "file")) {
				char *host;
				int host_len = spprintf(&host, 0, "unix://%s", url->path);

				/* chop off trailing :0 port specifier */
				if (!strcmp(host + host_len - 2, ":0")) {
					host_len -= 2;
				}

				if (persistent) {
					mmc = mmc_find_persistent(host, host_len, 0, 0, timeout, retry_interval TSRMLS_CC);
				}
				else {
					mmc = mmc_server_new(host, host_len, 0, 0, 0, timeout, retry_interval TSRMLS_CC);
				}

				efree(host);
			}
			else {
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
			}

			mmc_pool_add(pool, mmc, weight);
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

static int php_mmc_session_read_request(
	mmc_pool_t *pool, zval *zkey, zval **lockparam, zval *addparam, zval **dataparam,
	mmc_request_t **lockreq, mmc_request_t **addreq, mmc_request_t **datareq TSRMLS_DC) /* {{{ */
{
	mmc_request_t *lreq, *areq, *dreq;
	zval lockvalue;

	/* increment request which stores the response int using value_handler_single */
	lreq = mmc_pool_request(
		pool, MMC_PROTO_TCP, mmc_numeric_response_handler, lockparam[0],
		mmc_pool_failover_handler_null, NULL TSRMLS_CC);
	lreq->value_handler = mmc_value_handler_single;
	lreq->value_handler_param = lockparam;

	/* add request which should fail if lock has been incremented */
	areq = mmc_pool_request(
		pool, MMC_PROTO_TCP, mmc_stored_handler, addparam,
		mmc_pool_failover_handler_null, NULL TSRMLS_CC);

	/* request to fetch the session data */
	dreq = mmc_pool_request_get(
		pool, MMC_PROTO_TCP, mmc_value_handler_single, dataparam,
		mmc_pool_failover_handler_null, NULL TSRMLS_CC);

	/* prepare key */
	if (mmc_prepare_key_ex(Z_STRVAL_P(zkey), Z_STRLEN_P(zkey), dreq->key, &(dreq->key_len)) != MMC_OK) {
		mmc_pool_release(pool, lreq);
		mmc_pool_release(pool, areq);
		mmc_pool_release(pool, dreq);
		return MMC_REQUEST_FAILURE;
	}

	/* append .lock to key */
	memcpy(lreq->key, dreq->key, dreq->key_len);
	strcpy(lreq->key + dreq->key_len, ".lock");

	memcpy(areq->key, dreq->key, dreq->key_len);
	strcpy(areq->key + dreq->key_len, ".lock");

	lreq->key_len = areq->key_len = dreq->key_len + sizeof(".lock")-1;

	/* value for add request */
	ZVAL_LONG(&lockvalue, 1);

	/* build requests */
	pool->protocol->mutate(lreq, zkey, lreq->key, lreq->key_len, 1, 1, 1, MEMCACHE_G(lock_timeout));
	pool->protocol->store(pool, areq, MMC_OP_ADD, areq->key, areq->key_len, 0, MEMCACHE_G(lock_timeout), 0, &lockvalue TSRMLS_CC);
	pool->protocol->get(dreq, MMC_OP_GET, zkey, dreq->key, dreq->key_len);

	*lockreq = lreq;
	*addreq = areq;
	*datareq = dreq;
	return MMC_OK;
}
/* }}} */

/* {{{ PS_READ_FUNC
 */
PS_READ_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();

	if (pool != NULL) {
		zval lockresult, addresult, dataresult, zkey;
		zval *lockparam[3];
		zval *dataparam[3];

		mmc_t *mmc;
		mmc_request_t *lockrequest, *addrequest, *datarequest;
		mmc_queue_t skip_servers = {0};
		unsigned int last_index = 0, prev_index = 0, timeout = 5000;
		long remainingtime = MEMCACHE_G(lock_timeout) * 1000000 * 2;

		lockparam[0] = &lockresult;
		lockparam[1] = NULL;
		lockparam[2] = NULL;

		dataparam[0] = &dataresult;
		dataparam[1] = NULL;
		dataparam[2] = NULL;

		ZVAL_STRING(&zkey, (char *)key, 0);

		do {
			/* first request tries to increment lock */
			ZVAL_NULL(&lockresult);

			/* second request tries to add lock, succeeds if lock doesn't exist (not relevant for binary
			 * protocol where increment takes a default value */
			ZVAL_NULL(&addresult);

			/* third request fetches the data, data is only valid if either of the lock requests succeeded */
			ZVAL_NULL(&dataresult);

			/* create requests */
			if (php_mmc_session_read_request(pool, &zkey, lockparam, &addresult, dataparam, &lockrequest, &addrequest, &datarequest TSRMLS_CC) != MMC_OK) {
				break;
			}

			/* find next server in line */
			prev_index = last_index;
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index TSRMLS_CC);

			/* schedule the requests */
			if (!mmc_server_valid(mmc TSRMLS_CC) ||
				 mmc_pool_schedule(pool, mmc, lockrequest TSRMLS_CC) != MMC_OK ||
				 /*pool->protocol != &mmc_binary_protocol && */mmc_pool_schedule(pool, mmc, addrequest TSRMLS_CC) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, datarequest TSRMLS_CC) != MMC_OK) {
				mmc_pool_release(pool, lockrequest);
				mmc_pool_release(pool, addrequest);
				mmc_pool_release(pool, datarequest);
				mmc_queue_push(&skip_servers, mmc);
				continue;
			}

			/* execute requests */
			mmc_pool_run(pool TSRMLS_CC);

			if ((Z_TYPE(lockresult) == IS_LONG && Z_LVAL(lockresult) == 1) || (Z_TYPE(addresult) == IS_BOOL && Z_BVAL(addresult))) {
				if (Z_TYPE(dataresult) == IS_STRING) {
					/* break if successfully locked with existing value */
					mmc_queue_free(&skip_servers);
					*val = Z_STRVAL(dataresult);
					*vallen = Z_STRLEN(dataresult);
					return SUCCESS;
				}

				/* if missing value, skip this server and try next */
				zval_dtor(&dataresult);
				mmc_queue_push(&skip_servers, mmc);
			}
			else {
				/* if missing lock, back off and retry same server */
				last_index = prev_index;
				usleep(timeout);
				remainingtime -= timeout;
				timeout *= 2;

				/* max value to usleep() is 1 second */
				if (timeout > 1000000) {
					timeout = 1000000;
				}
			}
		} while (skip_servers.len < MEMCACHE_G(session_redundancy)-1 && skip_servers.len < pool->num_servers && remainingtime > 0);

		mmc_queue_free(&skip_servers);
		zval_dtor(&dataresult);
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
		zval lockresult, dataresult, lockvalue, value;

		mmc_t *mmc;
		mmc_request_t *lockrequest, *datarequest;
		mmc_queue_t skip_servers = {0};
		unsigned int last_index = 0;

		ZVAL_NULL(&lockresult);
		ZVAL_NULL(&dataresult);

		do {
			/* allocate requests */
			datarequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_stored_handler, &dataresult,
				mmc_pool_failover_handler_null, NULL TSRMLS_CC);

			if (mmc_prepare_key_ex(key, strlen(key), datarequest->key, &(datarequest->key_len)) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				break;
			}

			/* append .lock to key */
			lockrequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_stored_handler, &lockresult,
				mmc_pool_failover_handler_null, NULL TSRMLS_CC);

			memcpy(lockrequest->key, datarequest->key, datarequest->key_len);
			strcpy(lockrequest->key + datarequest->key_len, ".lock");
			lockrequest->key_len = datarequest->key_len + sizeof(".lock")-1;

			ZVAL_LONG(&lockvalue, 0);
			ZVAL_STRINGL(&value, (char *)val, vallen, 0);

			/* assemble commands to store data and reset lock */
			if (pool->protocol->store(pool, datarequest, MMC_OP_SET, datarequest->key, datarequest->key_len, 0, INI_INT("session.gc_maxlifetime"), 0, &value TSRMLS_CC) != MMC_OK ||
				pool->protocol->store(pool, lockrequest, MMC_OP_SET, lockrequest->key, lockrequest->key_len, 0, MEMCACHE_G(lock_timeout), 0, &lockvalue TSRMLS_CC) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				break;
			}

			/* find next server in line */
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index TSRMLS_CC);
			mmc_queue_push(&skip_servers, mmc);

			if (!mmc_server_valid(mmc TSRMLS_CC) ||
				 mmc_pool_schedule(pool, mmc, datarequest TSRMLS_CC) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, lockrequest TSRMLS_CC) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				continue;
			}
		} while (skip_servers.len < MEMCACHE_G(session_redundancy)-1 && skip_servers.len < pool->num_servers);

		mmc_queue_free(&skip_servers);

		/* execute requests */
		mmc_pool_run(pool TSRMLS_CC);

		if (Z_BVAL(lockresult) && Z_BVAL(dataresult)) {
			return SUCCESS;
		}
	}

	return FAILURE;
}
/* }}} */

static int mmc_deleted_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /*
	parses a DELETED response line, param is a zval pointer to store result into {{{ */
{
	if (response == MMC_OK || response == MMC_RESPONSE_NOT_FOUND) {
		ZVAL_TRUE((zval *)param);
		return MMC_REQUEST_DONE;
	}

	if (response == MMC_RESPONSE_CLIENT_ERROR) {
		ZVAL_FALSE((zval *)param);
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, 
				"Server %s (tcp %d, udp %d) failed with: %s (%d)",
				mmc->host, mmc->tcp.port, 
				mmc->udp.port, message, response);

		return MMC_REQUEST_DONE;
	}


	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

/* {{{ PS_DESTROY_FUNC
 */
PS_DESTROY_FUNC(memcache)
{
	mmc_pool_t *pool = PS_GET_MOD_DATA();

	if (pool != NULL) {
		zval lockresult, dataresult;

		mmc_t *mmc;
		mmc_request_t *lockrequest, *datarequest;
		mmc_queue_t skip_servers = {0};
		unsigned int last_index = 0;

		ZVAL_NULL(&lockresult);
		ZVAL_NULL(&dataresult);

		do {
			/* allocate requests */
			datarequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_deleted_handler, &dataresult,
				mmc_pool_failover_handler_null, NULL TSRMLS_CC);

			if (mmc_prepare_key_ex(key, strlen(key), datarequest->key, &(datarequest->key_len)) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				break;
			}

			/* append .lock to key */
			lockrequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_deleted_handler, &lockresult,
				mmc_pool_failover_handler_null, NULL TSRMLS_CC);

			memcpy(lockrequest->key, datarequest->key, datarequest->key_len);
			strcpy(lockrequest->key + datarequest->key_len, ".lock");
			lockrequest->key_len = datarequest->key_len + sizeof(".lock")-1;

			/* assemble commands to store data and reset lock */
			pool->protocol->delete(datarequest, datarequest->key, datarequest->key_len, 0);
			pool->protocol->delete(lockrequest, lockrequest->key, lockrequest->key_len, 0);

			/* find next server in line */
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index TSRMLS_CC);
			mmc_queue_push(&skip_servers, mmc);

			if (!mmc_server_valid(mmc TSRMLS_CC) ||
				 mmc_pool_schedule(pool, mmc, datarequest TSRMLS_CC) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, lockrequest TSRMLS_CC) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				continue;
			}
		} while (skip_servers.len < MEMCACHE_G(session_redundancy)-1 && skip_servers.len < pool->num_servers);

		mmc_queue_free(&skip_servers);

		/* execute requests */
		mmc_pool_run(pool TSRMLS_CC);

		if (Z_BVAL(lockresult) && Z_BVAL(dataresult)) {
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
