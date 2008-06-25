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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "php_memcache.h"

#ifndef ZEND_ENGINE_2
#define OnUpdateLong OnUpdateInt
#endif

/* True global resources - no need for thread safety here */
static int le_memcache_pool, le_memcache_server;
static zend_class_entry *memcache_pool_ce;
static zend_class_entry *memcache_ce;

ZEND_EXTERN_MODULE_GLOBALS(memcache)

/* {{{ memcache_functions[]
 */
zend_function_entry memcache_functions[] = {
	PHP_FE(memcache_connect,				NULL)
	PHP_FE(memcache_pconnect,				NULL)
	PHP_FE(memcache_add_server,				NULL)
	PHP_FE(memcache_set_server_params,		NULL)
	PHP_FE(memcache_set_failure_callback,	NULL)
	PHP_FE(memcache_get_server_status,		NULL)
	PHP_FE(memcache_get_version,			NULL)
	PHP_FE(memcache_add,					NULL)
	PHP_FE(memcache_set,					NULL)
	PHP_FE(memcache_replace,				NULL)
	PHP_FE(memcache_cas,					NULL)
	PHP_FE(memcache_append,					NULL)
	PHP_FE(memcache_prepend,				NULL)
	PHP_FE(memcache_get,					NULL)
	PHP_FE(memcache_delete,					NULL)
	PHP_FE(memcache_debug,					NULL)
	PHP_FE(memcache_get_stats,				NULL)
	PHP_FE(memcache_get_extended_stats,		NULL)
	PHP_FE(memcache_set_compress_threshold,	NULL)
	PHP_FE(memcache_increment,				NULL)
	PHP_FE(memcache_decrement,				NULL)
	PHP_FE(memcache_close,					NULL)
	PHP_FE(memcache_flush,					NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_pool_class_functions[] = {
	PHP_NAMED_FE(connect,				zif_memcache_pool_connect,			NULL)
	PHP_NAMED_FE(addserver,				zif_memcache_pool_addserver,		NULL)
	PHP_FALIAS(setserverparams,			memcache_set_server_params,			NULL)
	PHP_FALIAS(setfailurecallback,		memcache_set_failure_callback,		NULL)
	PHP_FALIAS(getserverstatus,			memcache_get_server_status,			NULL)
	PHP_FALIAS(getversion,				memcache_get_version,				NULL)
	PHP_FALIAS(add,						memcache_add,						NULL)
	PHP_FALIAS(set,						memcache_set,						NULL)
	PHP_FALIAS(replace,					memcache_replace,					NULL)
	PHP_FALIAS(cas,						memcache_cas,						NULL)
	PHP_FALIAS(append,					memcache_append,					NULL)
	PHP_FALIAS(prepend,					memcache_prepend,					NULL)
	PHP_FALIAS(get,						memcache_get,						NULL)
	PHP_FALIAS(delete,					memcache_delete,					NULL)
	PHP_FALIAS(getstats,				memcache_get_stats,					NULL)
	PHP_FALIAS(getextendedstats,		memcache_get_extended_stats,		NULL)
	PHP_FALIAS(setcompressthreshold,	memcache_set_compress_threshold,	NULL)
	PHP_FALIAS(increment,				memcache_increment,					NULL)
	PHP_FALIAS(decrement,				memcache_decrement,					NULL)
	PHP_FALIAS(close,					memcache_close,						NULL)
	PHP_FALIAS(flush,					memcache_flush,						NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_class_functions[] = {
	PHP_FALIAS(connect,					memcache_connect,					NULL)
	PHP_FALIAS(pconnect,				memcache_pconnect,					NULL)
	PHP_FALIAS(addserver,				memcache_add_server,				NULL)
	{NULL, NULL, NULL}
};

/* }}} */

/* {{{ memcache_module_entry
 */
zend_module_entry memcache_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"memcache",
	memcache_functions,
	PHP_MINIT(memcache),
	PHP_MSHUTDOWN(memcache),
	NULL,
	NULL,
	PHP_MINFO(memcache),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_MEMCACHE_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHE
ZEND_GET_MODULE(memcache)
#endif

static PHP_INI_MH(OnUpdateChunkSize) /* {{{ */
{
	long int lval;

	lval = strtol(new_value, NULL, 10);
	if (lval <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.chunk_size must be a positive integer ('%s' given)", new_value);
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}
/* }}} */

static PHP_INI_MH(OnUpdateFailoverAttempts) /* {{{ */
{
	long int lval;

	lval = strtol(new_value, NULL, 10);
	if (lval <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.max_failover_attempts must be a positive integer ('%s' given)", new_value);
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}
/* }}} */

static PHP_INI_MH(OnUpdateProtocol) /* {{{ */
{
	if (!strcasecmp(new_value, "ascii")) {
		MEMCACHE_G(protocol) = MMC_ASCII_PROTOCOL;
	}
	else if (!strcasecmp(new_value, "binary")) {
		MEMCACHE_G(protocol) = MMC_BINARY_PROTOCOL;
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.protocol must be in set {ascii, binary} ('%s' given)", new_value);
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateHashStrategy) /* {{{ */
{
	if (!strcasecmp(new_value, "standard")) {
		MEMCACHE_G(hash_strategy) = MMC_STANDARD_HASH;
	}
	else if (!strcasecmp(new_value, "consistent")) {
		MEMCACHE_G(hash_strategy) = MMC_CONSISTENT_HASH;
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.hash_strategy must be in set {standard, consistent} ('%s' given)", new_value);
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateHashFunction) /* {{{ */
{
	if (!strcasecmp(new_value, "crc32")) {
		MEMCACHE_G(hash_function) = MMC_HASH_CRC32;
	}
	else if (!strcasecmp(new_value, "fnv")) {
		MEMCACHE_G(hash_function) = MMC_HASH_FNV1A;
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.hash_function must be in set {crc32, fnv} ('%s' given)", new_value);
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateRedundancy) /* {{{ */
{
	long int lval;

	lval = strtol(new_value, NULL, 10);
	if (lval <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.redundancy must be a positive integer ('%s' given)", new_value);
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("memcache.allow_failover",		"1",			PHP_INI_ALL, OnUpdateLong,			allow_failover,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.max_failover_attempts",	"20",			PHP_INI_ALL, OnUpdateFailoverAttempts,		max_failover_attempts,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.default_port",			"11211",		PHP_INI_ALL, OnUpdateLong,			default_port,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.chunk_size",			"32768",		PHP_INI_ALL, OnUpdateChunkSize,		chunk_size,		zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.protocol",				"ascii",		PHP_INI_ALL, OnUpdateProtocol,		protocol,		zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.hash_strategy",			"consistent",	PHP_INI_ALL, OnUpdateHashStrategy,	hash_strategy,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.hash_function",			"crc32",		PHP_INI_ALL, OnUpdateHashFunction,	hash_function,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.redundancy",			"1",			PHP_INI_ALL, OnUpdateRedundancy,	redundancy,			zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_redundancy",	"2",			PHP_INI_ALL, OnUpdateRedundancy,	session_redundancy,	zend_memcache_globals,	memcache_globals)
PHP_INI_END()
/* }}} */

/* {{{ macros */
#define MMC_PREPARE_KEY(key, key_len) \
	php_strtr(key, key_len, "\t\r\n ", "____", 4); \
/* }}} */

/* {{{ internal function protos */
static void _mmc_pool_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static void _mmc_server_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static void php_mmc_set_failure_callback(mmc_pool_t *, zval *, zval * TSRMLS_DC);
static void php_mmc_failure_callback(mmc_pool_t *, mmc_t *, void * TSRMLS_DC);
/* }}} */

/* {{{ php_memcache_init_globals()
*/
static void php_memcache_init_globals(zend_memcache_globals *memcache_globals_p TSRMLS_DC)
{
	MEMCACHE_G(hash_strategy)	  = MMC_STANDARD_HASH;
	MEMCACHE_G(hash_function)	  = MMC_HASH_CRC32;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memcache)
{
	zend_class_entry ce;
	
	INIT_CLASS_ENTRY(ce, "MemcachePool", php_memcache_pool_class_functions);
	memcache_pool_ce = zend_register_internal_class(&ce TSRMLS_CC);
	
	INIT_CLASS_ENTRY(ce, "Memcache", php_memcache_class_functions);
	memcache_ce = zend_register_internal_class_ex(&ce, memcache_pool_ce, NULL TSRMLS_CC);

	le_memcache_pool = zend_register_list_destructors_ex(_mmc_pool_list_dtor, NULL, "memcache connection", module_number);
	le_memcache_server = zend_register_list_destructors_ex(NULL, _mmc_server_list_dtor, "persistent memcache connection", module_number);

#ifdef ZTS
	ts_allocate_id(&memcache_globals_id, sizeof(zend_memcache_globals), (ts_allocate_ctor) php_memcache_init_globals, NULL);
#else
	php_memcache_init_globals(&memcache_globals TSRMLS_CC);
#endif

	REGISTER_LONG_CONSTANT("MEMCACHE_COMPRESSED", MMC_COMPRESSED, CONST_CS | CONST_PERSISTENT);
	REGISTER_INI_ENTRIES();

#if HAVE_MEMCACHE_SESSION
	REGISTER_LONG_CONSTANT("MEMCACHE_HAVE_SESSION", 1, CONST_CS | CONST_PERSISTENT);
	php_session_register_module(ps_memcache_ptr);
#else
	REGISTER_LONG_CONSTANT("MEMCACHE_HAVE_SESSION", 0, CONST_CS | CONST_PERSISTENT);
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(memcache)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(memcache)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "memcache support", "enabled");
	php_info_print_table_row(2, "Version", PHP_MEMCACHE_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision$");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* ------------------
   internal functions
   ------------------ */

static void _mmc_pool_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	mmc_pool_free((mmc_pool_t *)rsrc->ptr TSRMLS_CC);
}
/* }}} */

static void _mmc_server_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	mmc_server_free((mmc_t *)rsrc->ptr TSRMLS_CC);
}
/* }}} */

static int mmc_get_pool(zval *id, mmc_pool_t **pool TSRMLS_DC) /* {{{ */
{
	zval **connection;
	int resource_type;

	if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to extract 'connection' variable from object");
		return 0;
	}

	*pool = (mmc_pool_t *) zend_list_find(Z_LVAL_PP(connection), &resource_type);

	if (!*pool || resource_type != le_memcache_pool) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown connection identifier");
		return 0;
	}

	return Z_LVAL_PP(connection);
}
/* }}} */

int mmc_stored_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /* 
	handles SET/ADD/REPLACE response, param is a zval pointer to store result into {{{ */
{
	if (response == MMC_OK) {
		if (param != NULL && Z_TYPE_P((zval *)param) == IS_NULL) {
			ZVAL_TRUE((zval *)param);
		}
		return MMC_REQUEST_DONE;
	}

	/* return FALSE or catch memory errors without failover */
	if (response == MMC_RESPONSE_EXISTS || response == MMC_RESPONSE_OUT_OF_MEMORY || response == MMC_RESPONSE_TOO_LARGE) {
		if (param != NULL) {
			ZVAL_FALSE((zval *)param);
		}
		return MMC_REQUEST_DONE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

static void php_mmc_store(INTERNAL_FUNCTION_PARAMETERS, int op) /* {{{ */
{
	mmc_pool_t *pool;
	mmc_request_t *request;
	zval *keys, *value = 0, *mmc_object = getThis();
	long flags = 0, exptime = 0, cas = 0;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oz|zlll", &mmc_object, memcache_pool_ce, &keys, &value, &flags, &exptime, &cas) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zlll", &keys, &value, &flags, &exptime, &cas) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	RETVAL_NULL();

	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zstr key;
		char keytmp[MAX_LENGTH_OF_LONG + 1];
		unsigned int key_len;
		unsigned long index;
		int key_type;
		
		zval **arrval;
		HashPosition pos;
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);
		
		while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **)&arrval, &pos) == SUCCESS) {
			key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(keys), &key, &key_len, &index, 0, &pos);
			zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);
			
			switch (key_type) {
				case HASH_KEY_IS_STRING:
					key_len--;
					break;

				case HASH_KEY_IS_LONG:
					key_len = sprintf(keytmp, "%lu", index);
					key = ZSTR(keytmp);
					break;

				default:
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
					continue;
			}

			/* allocate request */
			if (return_value_used) {
				request = mmc_pool_request(pool, MMC_PROTO_TCP,  
					mmc_stored_handler, return_value, mmc_pool_failover_handler, NULL TSRMLS_CC);
			}
			else {
				request = mmc_pool_request(pool, MMC_PROTO_TCP,  
					mmc_stored_handler, NULL, mmc_pool_failover_handler, NULL TSRMLS_CC);
			}
			
			if (mmc_prepare_key_ex(ZSTR_VAL(key), key_len, request->key, &(request->key_len)) != MMC_OK) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
				mmc_pool_release(pool, request);
				continue;
			}
			
			/* assemble command */
			if (pool->protocol->store(pool, request, op, ZSTR_VAL(key), key_len, flags, exptime, cas, *arrval TSRMLS_CC) != MMC_OK) {
				mmc_pool_release(pool, request);
				continue;
			}
			
			/* schedule request */
			if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy) TSRMLS_CC) != MMC_OK) {
				continue;
			}

			/* begin sending requests immediatly */
			mmc_pool_select(pool, 0 TSRMLS_CC);
		}
	}
	else if (value) {
		
		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stored_handler, return_value, mmc_pool_failover_handler, NULL TSRMLS_CC);

		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
			mmc_pool_release(pool, request);
			RETURN_FALSE;
		}
		
		/* assemble command */
		if (pool->protocol->store(pool, request, op, request->key, request->key_len, flags, exptime, cas, value TSRMLS_CC) != MMC_OK) {
			mmc_pool_release(pool, request);
			RETURN_FALSE;
		}
		
		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy) TSRMLS_CC) != MMC_OK) {
			RETURN_FALSE;
		}
	}
	else {
		WRONG_PARAM_COUNT;
	}
	
	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);

	if (Z_TYPE_P(return_value) == IS_NULL) {
		RETVAL_FALSE;
	}
}
/* }}} */

static int mmc_deleted_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /* 
	parses a DELETED response line, param is a zval pointer to store result into {{{ */
{
	if (response == MMC_OK) {
		if (param != NULL && Z_TYPE_P((zval *)param) == IS_NULL) {
			ZVAL_TRUE((zval *)param);
		}
		return MMC_REQUEST_DONE;
	}
	
	if (response == MMC_RESPONSE_NOT_FOUND) {
		if (param != NULL) {
			ZVAL_FALSE((zval *)param);
		}
		return MMC_REQUEST_DONE;
	}
	
	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

static void php_mmc_numeric(INTERNAL_FUNCTION_PARAMETERS, int deleted, int invert) /* 
	sends one or several commands which have a single optional numeric parameter (incr, decr, delete) {{{ */
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();
	
	zval *keys;
	long value = 1, defval = 0, exptime = 0;
	mmc_request_t *request;
	mmc_request_response_handler response_handler;
	void *value_handler_param[3];
	                          
	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oz|lll", &mmc_object, memcache_pool_ce, &keys, &value, &defval, &exptime) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|lll", &keys, &value, &defval, &exptime) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}
	
	if (deleted) {
		response_handler = mmc_deleted_handler;
	}

	ZVAL_NULL(return_value);
	value_handler_param[0] = return_value;
	value_handler_param[1] = NULL;
	value_handler_param[2] = NULL;
	
	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zval **key;
		HashPosition pos;
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);
		
		while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **)&key, &pos) == SUCCESS) {
			zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);

			/* allocate request */
			if (return_value_used) {
				request = mmc_pool_request(
					pool, MMC_PROTO_TCP, response_handler, return_value, 
					mmc_pool_failover_handler, NULL TSRMLS_CC);
			}
			else {
				request = mmc_pool_request(
					pool, MMC_PROTO_TCP, response_handler, NULL, 
					mmc_pool_failover_handler, NULL TSRMLS_CC);
			}

			request->value_handler = mmc_value_handler_multi;
			request->value_handler_param = value_handler_param;
			
			if (mmc_prepare_key(*key, request->key, &(request->key_len)) != MMC_OK) {
				mmc_pool_release(pool, request);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
				continue;
			}
			
			if (deleted) {
				pool->protocol->delete(request, request->key, request->key_len, value);
			}
			else {
				pool->protocol->mutate(request, *key, request->key, request->key_len, invert ? -value : value, defval, exptime);
			}

			/* schedule request */
			if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy) TSRMLS_CC) != MMC_OK) {
				continue;
			}

			/* begin sending requests immediatly */
			mmc_pool_select(pool, 0 TSRMLS_CC);
		}
	}
	else {
		if (deleted) {
			RETVAL_NULL();
		}
		else {
			RETVAL_FALSE;
		}
		
		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, 
			response_handler, return_value, mmc_pool_failover_handler, NULL TSRMLS_CC);

		request->value_handler = mmc_value_handler_single;
		request->value_handler_param = value_handler_param;
		
		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
			RETURN_FALSE;
		}

		if (deleted) {
			pool->protocol->delete(request, request->key, request->key_len, value);
		}
		else {
			pool->protocol->mutate(request, keys, request->key, request->key_len, invert ? -value : value, defval, exptime);
		}
		
		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy) TSRMLS_CC) != MMC_OK) {
			RETURN_FALSE;
		}
	}
	
	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);
}
/* }}} */

mmc_t *mmc_find_persistent(const char *host, int host_len, unsigned short port, unsigned short udp_port, int timeout, int retry_interval TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;
	zend_rsrc_list_entry *le;
	char *key;
	int key_len;

	key_len = spprintf(&key, 0, "memcache:server:%s:%u:%u", host, port, udp_port);
	
	if (zend_hash_find(&EG(persistent_list), key, key_len+1, (void **)&le) == FAILURE) {
		zend_rsrc_list_entry new_le;

		mmc = mmc_server_new(host, host_len, port, udp_port, 1, timeout, retry_interval TSRMLS_CC);
		new_le.type = le_memcache_server;
		new_le.ptr  = mmc;

		/* register new persistent connection */
		if (zend_hash_update(&EG(persistent_list), key, key_len+1, (void *)&new_le, sizeof(zend_rsrc_list_entry), NULL) == FAILURE) {
			mmc_server_free(mmc TSRMLS_CC);
			mmc = NULL;
		} else {
			zend_list_insert(mmc, le_memcache_server);
		}
	}
	else if (le->type != le_memcache_server || le->ptr == NULL) {
		zend_rsrc_list_entry new_le;
		zend_hash_del(&EG(persistent_list), key, key_len+1);

		mmc = mmc_server_new(host, host_len, port, udp_port, 1, timeout, retry_interval TSRMLS_CC);
		new_le.type = le_memcache_server;
		new_le.ptr  = mmc;

		/* register new persistent connection */
		if (zend_hash_update(&EG(persistent_list), key, key_len+1, (void *)&new_le, sizeof(zend_rsrc_list_entry), NULL) == FAILURE) {
			mmc_server_free(mmc TSRMLS_CC);
			mmc = NULL;
		}
		else {
			zend_list_insert(mmc, le_memcache_server);
		}
	}
	else {
		mmc = (mmc_t *)le->ptr;
		mmc->timeout = timeout;
		mmc->tcp.retry_interval = retry_interval;

		/* attempt to reconnect this node before failover in case connection has gone away */
		if (mmc->tcp.status == MMC_STATUS_CONNECTED) {
			mmc->tcp.status = MMC_STATUS_UNKNOWN;
		}
		if (mmc->udp.status == MMC_STATUS_CONNECTED) {
			mmc->udp.status = MMC_STATUS_UNKNOWN;
		}
	}

	efree(key);
	return mmc;
}
/* }}} */

static mmc_t *php_mmc_pool_addserver(
	zval *mmc_object, const char *host, int host_len, long tcp_port, long udp_port, long weight, 
	zend_bool persistent, long timeout, long retry_interval, zend_bool status, mmc_pool_t **pool_result TSRMLS_DC) /* {{{ */
{
	zval **connection;
	mmc_pool_t *pool;
	mmc_t *mmc;
	int list_id, resource_type;
	
	if (weight < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "weight must be a positive integer");
		return NULL;
	}

	/* initialize pool if need be */
	if (zend_hash_find(Z_OBJPROP_P(mmc_object), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		pool = mmc_pool_new(TSRMLS_C);
		pool->failure_callback = &php_mmc_failure_callback;
		list_id = zend_list_insert(pool, le_memcache_pool);
		add_property_resource(mmc_object, "connection", list_id);
	}
	else {
		pool = (mmc_pool_t *)zend_list_find(Z_LVAL_PP(connection), &resource_type);
		if (!pool || resource_type != le_memcache_pool) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown connection identifier");
			return NULL;
		}
	}
	
	/* binary protocol isn't support over UDP yet */
	if (udp_port && pool->protocol == &mmc_binary_protocol) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "binary protocol isn't support over UDP, defaulting to TCP");
		udp_port = 0;
	}
	
	/* lazy initialization of server struct */
	if (persistent && status) {
		mmc = mmc_find_persistent(host, host_len, tcp_port, udp_port, timeout, retry_interval TSRMLS_CC);
	}
	else {
		mmc = mmc_server_new(host, host_len, tcp_port, udp_port, 0, timeout, retry_interval TSRMLS_CC);
	}

	/* add server in failed mode */
	if (!status) {
		mmc->tcp.status = MMC_STATUS_FAILED;
		mmc->udp.status = MMC_STATUS_FAILED;
	}

	mmc_pool_add(pool, mmc, weight);
	
	if (pool_result != NULL) {
		*pool_result = pool;
	}
	
	return mmc;
}
/* }}} */

static void php_mmc_connect(INTERNAL_FUNCTION_PARAMETERS, zend_bool persistent) /* {{{ */
{
	zval *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc;

	char *host;
	int host_len;
	long tcp_port = MEMCACHE_G(default_port), timeout = MMC_DEFAULT_TIMEOUT;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &host, &host_len, &tcp_port, &timeout) == FAILURE) {
		return;
	}

	/* initialize pool and object if need be */
	if (!mmc_object) {
		int list_id;
		mmc_pool_t *pool = mmc_pool_new(TSRMLS_C);
		pool->failure_callback = &php_mmc_failure_callback;
		list_id = zend_list_insert(pool, le_memcache_pool);
		mmc_object = return_value;
		object_init_ex(mmc_object, memcache_ce);
		add_property_resource(mmc_object, "connection", list_id);
	}
	else {
		RETVAL_TRUE;
	}
	
	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, 0, 1, persistent, timeout, MMC_DEFAULT_RETRY, 1, NULL TSRMLS_CC);
	if (mmc == NULL) {
		RETURN_FALSE;
	}

	/* force a reconnect attempt if stream EOF */
	if (mmc->tcp.stream != NULL && php_stream_eof(mmc->tcp.stream)) {
		mmc_server_disconnect(mmc, &(mmc->tcp) TSRMLS_CC);
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	/* force a tcp connect (if not persistently connected) */
	if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0 TSRMLS_CC) != MMC_OK) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't connect to %s:%d, %s (%d)", host, mmc->tcp.port, mmc->error ? mmc->error : "Unknown error", mmc->errnum);
		RETURN_FALSE;
	}
}
/* }}} */

/*
 * STAT 6:chunk_size 64
 */
static int mmc_stats_parse_stat(char *start, char *end, zval *result TSRMLS_DC)  /* {{{ */
{
	char *space, *colon, *key;
	long index = 0;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	/* find space delimiting key and value */
	if ((space = php_memnstr(start, " ", 1, end)) == NULL) {
		return 0;
	}

	/* find colon delimiting subkeys */
	if ((colon = php_memnstr(start, ":", 1, space - 1)) != NULL) {
		zval *element, **elem;
		key = estrndup(start, colon - start);

		/* find existing or create subkey array in result */
		if ((is_numeric_string(key, colon - start, &index, NULL, 0) &&
			zend_hash_index_find(Z_ARRVAL_P(result), index, (void **)&elem) != FAILURE) ||
			zend_hash_find(Z_ARRVAL_P(result), key, colon - start + 1, (void **)&elem) != FAILURE) {
			element = *elem;
		}
		else {
			MAKE_STD_ZVAL(element);
			array_init(element);
			add_assoc_zval_ex(result, key, colon - start + 1, element);
		}

		efree(key);
		return mmc_stats_parse_stat(colon + 1, end, element TSRMLS_CC);
	}

	/* no more subkeys, add value under last subkey */
	key = estrndup(start, space - start);
	add_assoc_stringl_ex(result, key, space - start + 1, space + 1, end - space, 1);
	efree(key);

	return 1;
}
/* }}} */

/*
 * ITEM test_key [3 b; 1157099416 s]
 */
static int mmc_stats_parse_item(char *start, char *end, zval *result TSRMLS_DC)  /* {{{ */
{
	char *space, *value, *value_end, *key;
	zval *element;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	/* find space delimiting key and value */
	if ((space = php_memnstr(start, " ", 1, end)) == NULL) {
		return 0;
	}

	MAKE_STD_ZVAL(element);
	array_init(element);

	/* parse each contained value */
	for (value = php_memnstr(space, "[", 1, end); value != NULL && value <= end; value = php_memnstr(value + 1, ";", 1, end)) {
		do {
			value++;
		} while (*value == ' ' && value <= end);

		if (value <= end && (value_end = php_memnstr(value, " ", 1, end)) != NULL && value_end <= end) {
			add_next_index_stringl(element, value, value_end - value, 1);
		}
	}

	/* add parsed values under key */
	key = estrndup(start, space - start);
	add_assoc_zval_ex(result, key, space - start + 1, element);
	efree(key);

	return 1;
}
/* }}} */

static int mmc_stats_parse_generic(char *start, char *end, zval *result TSRMLS_DC)  /* {{{ */
{
	char *space, *key;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	if (start < end) {
		if ((space = php_memnstr(start, " ", 1, end)) != NULL) {
			key = estrndup(start, space - start);
			add_assoc_stringl_ex(result, key, space - start + 1, space + 1, end - space, 1);
			efree(key);
		}
		else {
			add_next_index_stringl(result, start, end - start, 1);
		}
	}
	else {
		return 0;
	}

	return 1;
}
/* }}} */

static void php_mmc_failure_callback(mmc_pool_t *pool, mmc_t *mmc, void *param TSRMLS_DC) /* {{{ */ 
{
	zval **callback;
	
	/* check for userspace callback */
	if (param != NULL && zend_hash_find(Z_OBJPROP_P((zval *)param), "_failureCallback", sizeof("_failureCallback"), (void **)&callback) == SUCCESS && Z_TYPE_PP(callback) != IS_NULL) {
		if (zend_is_callable(*callback, 0, NULL)) {
			zval *retval = NULL;
			zval *host, *tcp_port, *udp_port, *error, *errnum;
			zval **params[5];

			params[0] = &host;
			params[1] = &tcp_port;
			params[2] = &udp_port;
			params[3] = &error;
			params[4] = &errnum;

			MAKE_STD_ZVAL(host);
			MAKE_STD_ZVAL(tcp_port); MAKE_STD_ZVAL(udp_port);
			MAKE_STD_ZVAL(error); MAKE_STD_ZVAL(errnum);

			ZVAL_STRING(host, mmc->host, 1);
			ZVAL_LONG(tcp_port, mmc->tcp.port); ZVAL_LONG(udp_port, mmc->udp.port);
			
			if (mmc->error != NULL) {
				ZVAL_STRING(error, mmc->error, 1);
			}
			else {
				ZVAL_NULL(error);
			}
			ZVAL_LONG(errnum, mmc->errnum);

			call_user_function_ex(EG(function_table), NULL, *callback, &retval, 5, params, 0, NULL TSRMLS_CC);

			zval_ptr_dtor(&host);
			zval_ptr_dtor(&tcp_port); zval_ptr_dtor(&udp_port);
			zval_ptr_dtor(&error); zval_ptr_dtor(&errnum);
			
			if (retval != NULL) {
				zval_ptr_dtor(&retval);
			}
		}
		else {
			php_mmc_set_failure_callback(pool, (zval *)param, NULL TSRMLS_CC);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid failure callback");
		}
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Server %s (tcp %d, udp %d) failed with: %s (%d)", 
			mmc->host, mmc->tcp.port, mmc->udp.port, mmc->error, mmc->errnum);
	}
}
/* }}} */

static void php_mmc_set_failure_callback(mmc_pool_t *pool, zval *mmc_object, zval *callback TSRMLS_DC)  /* {{{ */
{
	if (callback != NULL) {
		zval *callback_tmp;
		ALLOC_ZVAL(callback_tmp);

		*callback_tmp = *callback;
		zval_copy_ctor(callback_tmp);
		INIT_PZVAL(callback_tmp);

		add_property_zval(mmc_object, "_failureCallback", callback_tmp);
		pool->failure_callback_param = mmc_object;  

		INIT_PZVAL(callback_tmp);
	}
	else {
		add_property_null(mmc_object, "_failureCallback");
		pool->failure_callback_param = NULL;  
	}
}
/* }}} */

/* ----------------
   module functions
   ---------------- */

/* {{{ proto bool MemcachePool::connect(string host [, int tcp_port [, int udp_port [, bool persistent [, int weight [, int timeout [, int retry_interval] ] ] ] ] ])
   Connects to server and returns a Memcache object */
PHP_NAMED_FUNCTION(zif_memcache_pool_connect)
{
	zval *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc;

	char *host;
	int host_len;
	long tcp_port = MEMCACHE_G(default_port), udp_port = 0, weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
	zend_bool persistent = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|llblll", 
		&host, &host_len, &tcp_port, &udp_port, &persistent, &weight, &timeout, &retry_interval) == FAILURE) {
		return;
	}
	
	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, udp_port, weight, persistent, timeout, retry_interval, 1, NULL TSRMLS_CC);
	if (mmc == NULL) {
		RETURN_FALSE;
	}
	
	/* force a reconnect attempt if stream EOF */
	if (mmc->tcp.stream != NULL && php_stream_eof(mmc->tcp.stream)) {
		mmc_server_disconnect(mmc, &(mmc->tcp) TSRMLS_CC);
	}
	
	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	/* force a tcp connect (if not persistently connected) */
	if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0 TSRMLS_CC) != MMC_OK) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't connect to %s:%d, %s (%d)", host, mmc->tcp.port, mmc->error ? mmc->error : "Unknown error", mmc->errnum);
		RETURN_FALSE;
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object memcache_connect(string host [, int port [, int timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_connect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto object memcache_pconnect(string host [, int port [, int timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_pconnect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto bool MemcachePool::addServer(string host [, int tcp_port [, int udp_port [, bool persistent [, int weight [, int timeout [, int retry_interval [, bool status] ] ] ] ])
   Adds a server to the pool */
PHP_NAMED_FUNCTION(zif_memcache_pool_addserver)
{
	zval *mmc_object = getThis();
	mmc_t *mmc;

	char *host;
	int host_len;
	long tcp_port = MEMCACHE_G(default_port), udp_port = 0, weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
	zend_bool persistent = 1, status = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|llblllb", 
		&host, &host_len, &tcp_port, &udp_port, &persistent, &weight, &timeout, &retry_interval, &status) == FAILURE) {
		return;
	}
	
	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, udp_port, weight, persistent, timeout, retry_interval, status, NULL TSRMLS_CC);
	if (mmc == NULL) {
		RETURN_FALSE;
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_add_server(string host [, int port [, bool persistent [, int weight [, int timeout [, int retry_interval [, bool status [, callback failure_callback ] ] ] ] ] ] ])
   Adds a connection to the pool. The order in which this function is called is significant */
PHP_FUNCTION(memcache_add_server)
{
	zval *mmc_object = getThis(), *failure_callback = NULL;
	mmc_pool_t *pool;
	mmc_t *mmc;

	char *host;
	int host_len;
	long tcp_port = MEMCACHE_G(default_port), weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
	zend_bool persistent = 1, status = 1;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lblllbz", 
			&host, &host_len, &tcp_port, &persistent, &weight, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|lblllbz", &mmc_object, memcache_ce, 
			&host, &host_len, &tcp_port, &persistent, &weight, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}

	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!zend_is_callable(failure_callback, 0, NULL)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, 0, weight, persistent, timeout, retry_interval, status, &pool TSRMLS_CC);
	if (mmc == NULL) {
		RETURN_FALSE;
	}
	
	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		php_mmc_set_failure_callback(pool, mmc_object, failure_callback TSRMLS_CC);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_set_server_params( string host [, int port [, int timeout [, int retry_interval [, bool status [, callback failure_callback ] ] ] ] ])
   Changes server parameters at runtime */
PHP_FUNCTION(memcache_set_server_params)
{
	zval *mmc_object = getThis(), *failure_callback = NULL;
	mmc_pool_t *pool;
	mmc_t *mmc = NULL;
	long tcp_port = MEMCACHE_G(default_port), timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
	zend_bool status = 1;
	int host_len, i;
	char *host;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lllbz", 
			&host, &host_len, &tcp_port, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|lllbz", &mmc_object, memcache_pool_ce, 
			&host, &host_len, &tcp_port, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!strcmp(pool->servers[i]->host, host) && pool->servers[i]->tcp.port == tcp_port) {
			mmc = pool->servers[i];
			break;
		}
	}

	if (!mmc) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Server not found in pool");
		RETURN_FALSE;
	}

	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!zend_is_callable(failure_callback, 0, NULL)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	mmc->timeout = timeout;
	mmc->tcp.retry_interval = retry_interval;

	if (!status) {
		mmc->tcp.status = MMC_STATUS_FAILED;
		mmc->udp.status = MMC_STATUS_FAILED;
	}
	else {
		if (mmc->tcp.status == MMC_STATUS_FAILED) {
			mmc->tcp.status = MMC_STATUS_DISCONNECTED;
		}
		if (mmc->udp.status == MMC_STATUS_FAILED) {
			mmc->udp.status = MMC_STATUS_DISCONNECTED;
		}
	}

	if (failure_callback != NULL) {
		if (Z_TYPE_P(failure_callback) != IS_NULL) {
			php_mmc_set_failure_callback(pool, mmc_object, failure_callback TSRMLS_CC);
		}
		else {
			php_mmc_set_failure_callback(pool, mmc_object, NULL TSRMLS_CC);
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_set_failure_callback( callback failure_callback )
   Changes the failover callback */
PHP_FUNCTION(memcache_set_failure_callback)
{
	zval *mmc_object = getThis(), *failure_callback;
	mmc_pool_t *pool;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", 
			&failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oz", &mmc_object, memcache_pool_ce, 
			&failure_callback) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!zend_is_callable(failure_callback, 0, NULL)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	if (Z_TYPE_P(failure_callback) != IS_NULL) {
		php_mmc_set_failure_callback(pool, mmc_object, failure_callback TSRMLS_CC);
	}
	else {
		php_mmc_set_failure_callback(pool, mmc_object, NULL TSRMLS_CC);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int memcache_get_server_status( string host [, int port ])
   Returns server status (0 if server is failed, otherwise non-zero) */
PHP_FUNCTION(memcache_get_server_status)
{
	zval *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc = NULL;
	long tcp_port = MEMCACHE_G(default_port);
	int host_len, i;
	char *host;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &host, &host_len, &tcp_port) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|l", &mmc_object, memcache_pool_ce, &host, &host_len, &tcp_port) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!strcmp(pool->servers[i]->host, host) && pool->servers[i]->tcp.port == tcp_port) {
			mmc = pool->servers[i];
			break;
		}
	}

	if (!mmc) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Server not found in pool");
		RETURN_FALSE;
	}

	RETURN_LONG(mmc->tcp.status > MMC_STATUS_FAILED ? 1 : 0);
}
/* }}} */

static int mmc_version_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /* 
	parses the VERSION response line, param is a zval pointer to store version into {{{ */
{
	if (response != MMC_RESPONSE_ERROR) {
		char *version = emalloc(message_len + 1);
		
		if (sscanf(message, "VERSION %s", version) == 1) {
			ZVAL_STRING((zval *)param, version, 0);
		}
		else {
			efree(version);
			ZVAL_STRINGL((zval *)param, (char *)message, message_len, 1);
		}
		
		return MMC_REQUEST_DONE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

/* {{{ proto string memcache_get_version( object memcache )
   Returns server's version */
PHP_FUNCTION(memcache_get_version)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();
	int i;
	mmc_request_t *request;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_pool_ce) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	RETVAL_FALSE;
	for (i=0; i<pool->num_servers; i++) {
		/* run command and check for valid return value */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_version_handler, return_value, NULL, NULL TSRMLS_CC);
		pool->protocol->version(request);
		
		if (mmc_pool_schedule(pool, pool->servers[i], request TSRMLS_CC) == MMC_OK) {
			mmc_pool_run(pool TSRMLS_CC);
			
			if (Z_TYPE_P(return_value) == IS_STRING) {
				break;
			}
		}
	}
}
/* }}} */

/* {{{ proto bool memcache_add(object memcache, mixed key [, mixed var [, int flag [, int exptime ] ] ])
   Adds new item. Item with such key should not exist. */
PHP_FUNCTION(memcache_add)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_ADD);
}
/* }}} */

/* {{{ proto bool memcache_set(object memcache, mixed key [, mixed var [, int flag [, int exptime ] ] ])
   Sets the value of an item. Item may exist or not */
PHP_FUNCTION(memcache_set)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_SET);
}
/* }}} */

/* {{{ proto bool memcache_replace(object memcache, mixed key [, mixed var [, int flag [, int exptime ] ] )
   Replaces existing item. Returns false if item doesn't exist */
PHP_FUNCTION(memcache_replace)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_REPLACE);
}
/* }}} */

/* {{{ proto bool memcache_cas(object memcache, mixed key [, mixed var [, int flag [, int exptime [, long cas ] ] ] ])
   Sets the value of an item if the CAS value is the same (Compare-And-Swap)  */
PHP_FUNCTION(memcache_cas)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_CAS);
}
/* }}} */

/* {{{ proto bool memcache_prepend(object memcache, mixed key [, mixed var [, int flag [, int exptime ] ] ])
   Appends a value to the stored value, value must exist */
PHP_FUNCTION(memcache_append)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_APPEND);
}
/* }}} */

/* {{{ proto bool memcache_prepend(object memcache, mixed key [, mixed var [, int flag [, int exptime ] ] ])
   Prepends a value to the stored value, value must exist */
PHP_FUNCTION(memcache_prepend)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, MMC_OP_PREPEND);
}
/* }}} */

int mmc_value_handler_multi(
	const char *key, unsigned int key_len, zval *value, 
	unsigned int flags, unsigned long cas, void *param TSRMLS_DC) /* 
	receives a multiple values, param is a zval** array to store value and flags in {{{ */
{
	zval *arrval, **result = (zval **)param;
	ALLOC_ZVAL(arrval);
	*((zval *)arrval) = *value;
	
	/* add value to result */
	if (Z_TYPE_P(result[0]) != IS_ARRAY) {
		array_init(result[0]);
	}
	add_assoc_zval_ex(result[0], (char *)key, key_len + 1, arrval);
	
	/* add flags to result */
	if (result[1] != NULL) {
		if (Z_TYPE_P(result[1]) != IS_ARRAY) {
			array_init(result[1]);
		}
		add_assoc_long_ex(result[1], (char *)key, key_len + 1, flags);
	}

	/* add CAS value to result */
	if (result[2] != NULL) {
		if (Z_TYPE_P(result[2]) != IS_ARRAY) {
			array_init(result[2]);
		}
		add_assoc_long_ex(result[2], (char *)key, key_len + 1, cas);
	}
	
	return MMC_REQUEST_DONE;
}
/* }}} */

int mmc_value_handler_single(
	const char *key, unsigned int key_len, zval *value, 
	unsigned int flags, unsigned long cas, void *param TSRMLS_DC) /* 
	receives a single value, param is a zval pointer to store value to {{{ */
{
	zval **result = (zval **)param;
	*(result[0]) = *value;
	
	if (result[1] != NULL) {
		ZVAL_LONG(result[1], flags);
	}

	if (result[2] != NULL) {
		ZVAL_LONG(result[2], cas);
	}

	return MMC_REQUEST_DONE;
}
/* }}} */

static int mmc_value_failover_handler(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param TSRMLS_DC) /* 
	uses keys and return value to reschedule requests to other servers, param is a zval ** pointer {{{ */
{
	zval **key, *keys = ((zval **)param)[0], **value_handler_param = (zval **)((void **)param)[1];
	HashPosition pos;

	if (!MEMCACHE_G(allow_failover) || request->failed_servers.len >= MEMCACHE_G(max_failover_attempts)) {
		mmc_pool_release(pool, request);
		return MMC_REQUEST_FAILURE;
	}
	
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);
	
	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **)&key, &pos) == SUCCESS) {
		zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);
		
		/* re-schedule key if it does not exist in return value array */
		if (Z_TYPE_P(value_handler_param[0]) != IS_ARRAY ||
			!zend_hash_exists(Z_ARRVAL_P(value_handler_param[0]), Z_STRVAL_PP(key), Z_STRLEN_PP(key) + 1)) 
		{
			mmc_pool_schedule_get(pool, MMC_PROTO_UDP, 
				value_handler_param[2] != NULL ? MMC_OP_GETS : MMC_OP_GET, *key,
				request->value_handler, request->value_handler_param,
				request->failover_handler, request->failover_handler_param, request TSRMLS_CC);
		}
	}

	mmc_pool_release(pool, request);
	return MMC_OK;
}
/* }}}*/

/* {{{ proto mixed memcache_get( object memcache, mixed key [, mixed &flags [, mixed &cas ] ] )
   Returns value of existing item or false */
PHP_FUNCTION(memcache_get)
{
	mmc_pool_t *pool;
	zval *keys, *flags = NULL, *cas = NULL, *mmc_object = getThis();
	void *value_handler_param[3], *failover_handler_param[2];

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oz|zz", &mmc_object, memcache_pool_ce, &keys, &flags, &cas) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zz", &keys, &flags, &cas) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	ZVAL_FALSE(return_value);
	value_handler_param[0] = return_value;
	value_handler_param[1] = flags;
	value_handler_param[2] = cas;
	
	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zval **key;
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);

		failover_handler_param[0] = keys;
		failover_handler_param[1] = value_handler_param;
		
		while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **)&key, &pos) == SUCCESS) {
			zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);
			
			/* schedule request */
			mmc_pool_schedule_get(pool, MMC_PROTO_UDP, 
				cas != NULL ? MMC_OP_GETS : MMC_OP_GET, *key, 
				mmc_value_handler_multi, value_handler_param, 
				mmc_value_failover_handler, failover_handler_param, NULL TSRMLS_CC);
		}
	}
	else {
		mmc_request_t *request;
		
		/* allocate request */
		request = mmc_pool_request_get(
			pool, MMC_PROTO_UDP,
			mmc_value_handler_single, value_handler_param, 
			mmc_pool_failover_handler, NULL TSRMLS_CC);

		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
			return;
		}

		pool->protocol->get(request, cas != NULL ? MMC_OP_GETS : MMC_OP_GET, keys, request->key, request->key_len);
		
		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, 1 TSRMLS_CC) != MMC_OK) {
			return;
		}
	}
	
	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);
}
/* }}} */

static int mmc_stats_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /* 
	parses the stats response line, param is a zval pointer to store stats into {{{ */
{
	if (response != MMC_RESPONSE_ERROR) 
	{
		char *line = (char *)message;
		
		if(!message_len) {
			return MMC_REQUEST_DONE;
		}
		
		if (mmc_str_left(line, "RESET", message_len, sizeof("RESET")-1)) {
			ZVAL_TRUE((zval *)param);
			return MMC_REQUEST_DONE;
		}
		else if (mmc_str_left(line, "STAT ", message_len, sizeof("STAT ")-1)) {
			if (mmc_stats_parse_stat(line + sizeof("STAT ")-1, line + message_len - 1, (zval *)param TSRMLS_CC)) {
				return MMC_REQUEST_AGAIN;
			}
		}
		else if (mmc_str_left(line, "ITEM ", message_len, sizeof("ITEM ")-1)) {
			if (mmc_stats_parse_item(line + sizeof("ITEM ")-1, line + message_len - 1, (zval *)param TSRMLS_CC)) {
				return MMC_REQUEST_AGAIN;
			}
		}
		else if (mmc_str_left(line, "END", message_len, sizeof("END")-1)) {
			return MMC_REQUEST_DONE;
		}
		else if (mmc_stats_parse_generic(line, line + message_len, (zval *)param TSRMLS_CC)) {
			return MMC_REQUEST_AGAIN;
		}

		zval_dtor((zval *)param);
		ZVAL_FALSE((zval *)param);
		return MMC_REQUEST_FAILURE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

static int mmc_stats_checktype(const char *type) { /* {{{ */
	return type == NULL ||
		!strcmp(type, "reset") || 
		!strcmp(type, "malloc") || 
		!strcmp(type, "slabs") ||
		!strcmp(type, "cachedump") || 
		!strcmp(type, "items") || 
		!strcmp(type, "sizes");	
}
/* }}} */

/* {{{ proto array memcache_get_stats( object memcache [, string type [, int slabid [, int limit ] ] ])
   Returns server's statistics */
PHP_FUNCTION(memcache_get_stats)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();

	char *type = NULL;
	int i, type_len = 0;
	long slabid = 0, limit = MMC_DEFAULT_CACHEDUMP_LIMIT;
	mmc_request_t *request;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|sll", &mmc_object, memcache_pool_ce, &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sll", &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}
	
	if (!mmc_stats_checktype(type)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid stats type");
		RETURN_FALSE;
	}

	ZVAL_FALSE(return_value);
	
	for (i=0; i<pool->num_servers; i++) {
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stats_handler, return_value, NULL, NULL TSRMLS_CC);
		pool->protocol->stats(request, type, slabid, limit);
		
		/* run command and check for valid return value */
		if (mmc_pool_schedule(pool, pool->servers[i], request TSRMLS_CC) == MMC_OK) {
			mmc_pool_run(pool TSRMLS_CC);
			
			if (Z_TYPE_P(return_value) != IS_BOOL || Z_BVAL_P(return_value)) {
				break;
			}
		}
	}
	
	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);
}
/* }}} */

/* {{{ proto array memcache_get_extended_stats( object memcache [, string type [, int slabid [, int limit ] ] ])
   Returns statistics for each server in the pool */
PHP_FUNCTION(memcache_get_extended_stats)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis(), *stats;

	char *host, *type = NULL;
	int i, host_len, type_len = 0;
	long slabid = 0, limit = MMC_DEFAULT_CACHEDUMP_LIMIT;
	mmc_request_t *request;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|sll", &mmc_object, memcache_pool_ce, &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sll", &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	if (!mmc_stats_checktype(type)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid stats type");
		RETURN_FALSE;
	}

	array_init(return_value);
	
	for (i=0; i<pool->num_servers; i++) {
		MAKE_STD_ZVAL(stats);
		ZVAL_FALSE(stats);

		host_len = spprintf(&host, 0, "%s:%u", pool->servers[i]->host, pool->servers[i]->tcp.port);
		add_assoc_zval_ex(return_value, host, host_len + 1, stats);
		efree(host);

		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stats_handler, stats, NULL, NULL TSRMLS_CC);
		pool->protocol->stats(request, type, slabid, limit);
		
		mmc_pool_schedule(pool, pool->servers[i], request TSRMLS_CC);
	}

	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);
}
/* }}} */

/* {{{ proto array memcache_set_compress_threshold( object memcache, int threshold [, float min_savings ] )
   Set automatic compress threshold */
PHP_FUNCTION(memcache_set_compress_threshold)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();
	long threshold;
	double min_savings = MMC_DEFAULT_SAVINGS;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Ol|d", &mmc_object, memcache_pool_ce, &threshold, &min_savings) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|d", &threshold, &min_savings) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (threshold < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "threshold must be a positive integer");
		RETURN_FALSE;
	}
	pool->compress_threshold = threshold;

	if (min_savings != MMC_DEFAULT_SAVINGS) {
		if (min_savings < 0 || min_savings > 1) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "min_savings must be a float in the 0..1 range");
			RETURN_FALSE;
		}
		pool->min_compress_savings = min_savings;
	}
	else {
		pool->min_compress_savings = MMC_DEFAULT_SAVINGS;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_delete(object memcache, mixed key [, int exptime ])
   Deletes existing item */
PHP_FUNCTION(memcache_delete)
{
	php_mmc_numeric(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

/* {{{ proto mixed memcache_increment(object memcache, mixed key [, int value [, int defval [, int exptime ] ] ])
   Increments existing variable */
PHP_FUNCTION(memcache_increment)
{
	php_mmc_numeric(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ proto mixed memcache_decrement(object memcache, mixed key [, int value [, int defval [, int exptime ] ] ])
   Decrements existing variable */
PHP_FUNCTION(memcache_decrement)
{
	php_mmc_numeric(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 1);
}
/* }}} */

/* {{{ proto bool memcache_close( object memcache )
   Closes connection to memcached */
PHP_FUNCTION(memcache_close)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_pool_ce) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	mmc_pool_close(pool TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

static int mmc_flush_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param TSRMLS_DC) /* 
	parses the OK response line, param is an int pointer to increment on success {{{ */
{
	if (response == MMC_OK) {
		(*((int *)param))++;
		return MMC_REQUEST_DONE;
	}
	
	return mmc_request_failure(mmc, request->io, message, message_len, 0 TSRMLS_CC);
}
/* }}} */

/* {{{ proto bool memcache_flush( object memcache [, int delay ] )
   Flushes cache, optionally at after the specified delay */
PHP_FUNCTION(memcache_flush)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();
	
	mmc_request_t *request;
	unsigned int i, responses = 0;
	long delay = 0;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|l", &mmc_object, memcache_pool_ce, &delay) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &delay) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_flush_handler, &responses, NULL, NULL TSRMLS_CC);
		pool->protocol->flush(request, delay);
		
		if (mmc_pool_schedule(pool, pool->servers[i], request TSRMLS_CC) == MMC_OK) {
			/* begin sending requests immediatly */
			mmc_pool_select(pool, 0 TSRMLS_CC);
		}
	}

	/* execute all requests */
	mmc_pool_run(pool TSRMLS_CC);
	
	if (responses < pool->num_servers) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_debug( bool onoff ) */
PHP_FUNCTION(memcache_debug)
{
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache_debug() is deprecated, please use a debugger (like Eclipse + CDT)");
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
