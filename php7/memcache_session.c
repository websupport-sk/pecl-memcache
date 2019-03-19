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
#include "ext/standard/php_smart_string.h"
#include "ext/standard/url.h"
#include "session/php_session.h"
#ifdef PHP_WIN32
#include "win32/time.h"
#endif
#include "php_memcache.h"

static int mmc_deleted_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param);

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
	zval params, *param;
	int i, j, path_len;

	const char *path = MEMCACHE_G(session_save_path);
	if (!path) {
		/* allow to work with standard session.save_path option
		   and session_save_path function */
		path = save_path;
	}
	if (!path) {
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}

	pool = mmc_pool_new();

	for (i=0,j=0,path_len=strlen(path); i<path_len; i=j+1) {
		/* find beginning of url */
		while (i<path_len && (isspace(path[i]) || path[i] == ',')) {
			i++;
		}

		/* find end of url */
		j = i;
		while (j<path_len && !isspace(path[j]) && path[j] != ',') {
			 j++;
		}

		if (i < j) {
			int persistent = 0, udp_port = 0, weight = 1, retry_interval = MMC_DEFAULT_RETRY;
			double timeout = MMC_DEFAULT_TIMEOUT;

			/* translate unix: into file: */
			if (!strncmp(path+i, "unix:", sizeof("unix:")-1)) {
				int len = j-i;
				char *path2 = estrndup(path+i, len);
				memcpy(path2, "file:", sizeof("file:")-1);
				url = php_url_parse_ex(path2, len);
				efree(path2);
			}
			else {
				int len = j-i;
				char *path2 = estrndup(path+i, len);
				url = php_url_parse_ex(path2, strlen(path2));
				efree(path2);
			}

			if (!url) {
				php_error_docref(NULL, E_WARNING,
					"Failed to parse memcache.save_path (error at offset %d, url was '%s')", i, path);

				mmc_pool_free(pool);
				PS_SET_MOD_DATA(NULL);
				return FAILURE;
			}

			if (url->query != NULL) {
				array_init(&params);
			/* parse parameters */
#if PHP_VERSION_ID < 70300
				sapi_module.treat_data(PARSE_STRING, estrdup(url->query), &params);
#else
				sapi_module.treat_data(PARSE_STRING, estrdup(ZSTR_VAL(url->query)), &params);
#endif
				if ((param = zend_hash_str_find(Z_ARRVAL(params), "persistent", sizeof("persistent")-1)) != NULL) {
					convert_to_boolean_ex(param);
					persistent = Z_TYPE_P(param) == IS_TRUE;
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "udp_port", sizeof("udp_port")-1)) != NULL) {
					convert_to_long_ex(param);
					udp_port = Z_LVAL_P(param);
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "weight", sizeof("weight")-1)) != NULL) {
					convert_to_long_ex(param);
					weight = Z_LVAL_P(param);
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "timeout", sizeof("timeout")-1)) != NULL) {
					convert_to_double_ex(param);
					timeout = Z_DVAL_P(param);
				}

				if ((param = zend_hash_str_find(Z_ARRVAL(params), "retry_interval", sizeof("retry_interval")-1)) != NULL) {
					convert_to_long_ex(param);
					retry_interval = Z_LVAL_P(param);
				}

				zval_ptr_dtor(&params);
			}
#if PHP_VERSION_ID < 70300
			if (url->scheme && url->path && !strcmp(url->scheme, "file")) {
				char *host;
				int host_len = spprintf(&host, 0, "unix://%s", url->path);
#else
			if (url->scheme && url->path && !strcmp(ZSTR_VAL(url->scheme), "file")) {
				char *host;
				int host_len = spprintf(&host, 0, "unix://%s", ZSTR_VAL(url->path));
#endif

				/* chop off trailing :0 port specifier */
				if (!strcmp(host + host_len - 2, ":0")) {
					host_len -= 2;
				}
				if (persistent) {
					mmc = mmc_find_persistent(host, host_len, 0, 0, timeout, retry_interval);
				}
				else {
					mmc = mmc_server_new(host, host_len, 0, 0, 0, timeout, retry_interval);
				}

				efree(host);
			}
			else {
				if (url->host == NULL || weight <= 0 || timeout <= 0) {
					php_url_free(url);
					mmc_pool_free(pool);
					PS_SET_MOD_DATA(NULL);
					return FAILURE;
				}

#if PHP_VERSION_ID < 70300
				if (persistent) {
                       mmc = mmc_find_persistent(url->host, strlen(url->host), url->port, udp_port, timeout, retry_interval);
				}
				else {
                       mmc = mmc_server_new(url->host, strlen(url->host), url->port, udp_port, 0, timeout, retry_interval);
				}
#else 
				if (persistent) {
					mmc = mmc_find_persistent(ZSTR_VAL(url->host), ZSTR_LEN(url->host), url->port, udp_port, timeout, retry_interval);
				}
				else {
					mmc = mmc_server_new(ZSTR_VAL(url->host), ZSTR_LEN(url->host), url->port, udp_port, 0, timeout, retry_interval);
				}
#endif

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

static int php_mmc_session_read_request(
	mmc_pool_t *pool, zval *zkey, zval **lockparam, zval *addparam, zval **dataparam,
	mmc_request_t **lockreq, mmc_request_t **addreq, mmc_request_t **datareq) /* {{{ */
{
	mmc_request_t *lreq, *areq, *dreq;
	zval lockvalue;

	/* increment request which stores the response int using value_handler_single */
	lreq = mmc_pool_request(
		pool, MMC_PROTO_TCP, mmc_numeric_response_handler, lockparam[0],
		mmc_pool_failover_handler_null, NULL);
	lreq->value_handler = mmc_value_handler_single;
	lreq->value_handler_param = lockparam;

	/* add request which should fail if lock has been incremented */
	areq = mmc_pool_request(
		pool, MMC_PROTO_TCP, mmc_stored_handler, addparam,
		mmc_pool_failover_handler_null, NULL);

	/* request to fetch the session data */
	dreq = mmc_pool_request_get(
		pool, MMC_PROTO_TCP, mmc_value_handler_single, dataparam,
		mmc_pool_failover_handler_null, NULL);

	/* prepare key */
	if (mmc_prepare_key_ex(Z_STRVAL_P(zkey), Z_STRLEN_P(zkey), dreq->key, &(dreq->key_len), MEMCACHE_G(session_key_prefix)) != MMC_OK) {
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
	pool->protocol->store(pool, areq, MMC_OP_ADD, areq->key, areq->key_len, 0, MEMCACHE_G(lock_timeout), 0, &lockvalue);
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
	*val = ZSTR_EMPTY_ALLOC();
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

		ZVAL_STR(&zkey, key);

		do {
			/* first request tries to increment lock */
			ZVAL_NULL(&lockresult);

			/* second request tries to add lock, succeeds if lock doesn't exist (not relevant for binary
			 * protocol where increment takes a default value */
			ZVAL_NULL(&addresult);

			/* third request fetches the data, data is only valid if either of the lock requests succeeded */
			ZVAL_EMPTY_STRING(&dataresult);

			/* create requests */
			if (php_mmc_session_read_request(pool, &zkey, lockparam, &addresult, dataparam, &lockrequest, &addrequest, &datarequest) != MMC_OK) {
				return FAILURE;
			}

			/* find next server in line */
			prev_index = last_index;
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index);

			/* schedule the requests */
			if (!mmc_server_valid(mmc) ||
				 mmc_pool_schedule(pool, mmc, lockrequest) != MMC_OK ||
				 /*pool->protocol != &mmc_binary_protocol && */mmc_pool_schedule(pool, mmc, addrequest) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, datarequest) != MMC_OK) {
				mmc_pool_release(pool, lockrequest);
				mmc_pool_release(pool, addrequest);
				mmc_pool_release(pool, datarequest);
				mmc_queue_push(&skip_servers, mmc);
				continue;
			}

			/* execute requests */
			mmc_pool_run(pool);

			if ((Z_TYPE(lockresult) == IS_LONG && Z_LVAL(lockresult) == 1) || ((Z_TYPE(addresult) == IS_TRUE || Z_TYPE(addresult) == IS_FALSE) && Z_TYPE(addresult) == IS_TRUE)) {
				if (Z_TYPE(dataresult) == IS_STRING) {
					/* break if successfully locked with existing value */
					mmc_queue_free(&skip_servers);
					*val = zend_string_init(Z_STRVAL(dataresult), Z_STRLEN(dataresult), 0);
					zval_ptr_dtor(&dataresult);
					return SUCCESS;
				}

				/* if missing value, skip this server and try next */
				zval_dtor(&dataresult);
				mmc_queue_push(&skip_servers, mmc);
				
				/* if it is the last server in pool and connection was ok return success and empty string due to php70 changes */
				if (skip_servers.len == pool->num_servers && skip_servers.len < MEMCACHE_G(session_redundancy)) {
					*val = ZSTR_EMPTY_ALLOC();
					mmc_queue_free(&skip_servers);
					zval_ptr_dtor(&dataresult);
					return SUCCESS;

				}
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
		} while (skip_servers.len < MEMCACHE_G(session_redundancy) && skip_servers.len < pool->num_servers && remainingtime > 0);

		mmc_queue_free(&skip_servers);
		zval_dtor(&dataresult);

		return SUCCESS;
	}
	else
	{
	return FAILURE;
	}
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
		size_t lifetime = (size_t)time(NULL) + INI_INT("session.gc_maxlifetime");
		unsigned int last_index = 0;

		ZVAL_NULL(&lockresult);
		ZVAL_NULL(&dataresult);

		do {
			/* allocate requests */
			datarequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_stored_handler, &dataresult,
				mmc_pool_failover_handler_null, NULL);

			if (mmc_prepare_key_ex(ZSTR_VAL(key), ZSTR_LEN(key), datarequest->key, &(datarequest->key_len), MEMCACHE_G(session_key_prefix)) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				break;
			}

			/* append .lock to key */
			lockrequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_stored_handler, &lockresult,
				mmc_pool_failover_handler_null, NULL);

			memcpy(lockrequest->key, datarequest->key, datarequest->key_len);
			strcpy(lockrequest->key + datarequest->key_len, ".lock");
			lockrequest->key_len = datarequest->key_len + sizeof(".lock")-1;

			ZVAL_LONG(&lockvalue, 0);
			ZVAL_STR(&value, val);

			/* assemble commands to store data and reset lock */
			if (pool->protocol->store(pool, datarequest, MMC_OP_SET, datarequest->key, datarequest->key_len, 0, lifetime, 0, &value) != MMC_OK ||
					pool->protocol->store(pool, lockrequest, MMC_OP_SET, lockrequest->key, lockrequest->key_len, 0, MEMCACHE_G(lock_timeout), 0, &lockvalue) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				mmc_queue_push(&skip_servers, mmc);
				break;
			}

			/* find next server in line */
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index);
			mmc_queue_push(&skip_servers, mmc);

			if (!mmc_server_valid(mmc) ||
				 mmc_pool_schedule(pool, mmc, datarequest) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, lockrequest) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				continue;
			}
		} while (skip_servers.len < MEMCACHE_G(session_redundancy) && skip_servers.len < pool->num_servers);

		mmc_queue_free(&skip_servers);

		/* execute requests */
		mmc_pool_run(pool);

		if (Z_TYPE(lockresult) == IS_TRUE && Z_TYPE(dataresult) == IS_TRUE) {
			return SUCCESS;
		}
	}

	return FAILURE;
}
/* }}} */

static int mmc_deleted_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	parses a DELETED response line, param is a zval pointer to store result into {{{ */
{
	if (response == MMC_OK || response == MMC_RESPONSE_NOT_FOUND) {
		ZVAL_TRUE((zval *)param);
		return MMC_REQUEST_DONE;
	}

	if (response == MMC_RESPONSE_CLIENT_ERROR) {
		ZVAL_FALSE((zval *)param);
		php_error_docref(NULL, E_NOTICE, 
				"Server %s (tcp %d, udp %d) failed with: %s (%d)",
				mmc->host, mmc->tcp.port, 
				mmc->udp.port, message, response);

		return MMC_REQUEST_DONE;
	}


	return mmc_request_failure(mmc, request->io, message, message_len, 0);
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
				mmc_pool_failover_handler_null, NULL);

			if (mmc_prepare_key_ex(ZSTR_VAL(key), ZSTR_LEN(key), datarequest->key, &(datarequest->key_len), MEMCACHE_G(session_key_prefix)) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				break;
			}

			/* append .lock to key */
			lockrequest = mmc_pool_request(
				pool, MMC_PROTO_TCP, mmc_deleted_handler, &lockresult,
				mmc_pool_failover_handler_null, NULL);

			memcpy(lockrequest->key, datarequest->key, datarequest->key_len);
			strcpy(lockrequest->key + datarequest->key_len, ".lock");
			lockrequest->key_len = datarequest->key_len + sizeof(".lock")-1;

			/* assemble commands to store data and reset lock */
			pool->protocol->delete(datarequest, datarequest->key, datarequest->key_len, 0);
			pool->protocol->delete(lockrequest, lockrequest->key, lockrequest->key_len, 0);

			/* find next server in line */
			mmc = mmc_pool_find_next(pool, datarequest->key, datarequest->key_len, &skip_servers, &last_index);
			mmc_queue_push(&skip_servers, mmc);

			if (!mmc_server_valid(mmc) ||
				 mmc_pool_schedule(pool, mmc, datarequest) != MMC_OK ||
				 mmc_pool_schedule(pool, mmc, lockrequest) != MMC_OK) {
				mmc_pool_release(pool, datarequest);
				mmc_pool_release(pool, lockrequest);
				continue;
			}
		} while (skip_servers.len < MEMCACHE_G(session_redundancy) && skip_servers.len < pool->num_servers);

		mmc_queue_free(&skip_servers);

		/* execute requests */
		mmc_pool_run(pool);

		if (Z_TYPE(lockresult) == IS_TRUE && Z_TYPE(dataresult) == IS_TRUE) {
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

