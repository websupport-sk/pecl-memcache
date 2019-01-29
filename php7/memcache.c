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

/* True global resources - no need for thread safety here */
static int le_memcache_pool, le_memcache_server;
static zend_class_entry *memcache_pool_ce;
static zend_class_entry *memcache_ce;

ZEND_EXTERN_MODULE_GLOBALS(memcache)

/* {{{ memcache_functions[]
 */
ZEND_BEGIN_ARG_INFO(arginfo_memcache_get, 1)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_memcache_object_get, 1)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO()

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
	PHP_FE(memcache_get,					arginfo_memcache_get)
	PHP_FE(memcache_delete,					NULL)
	PHP_FE(memcache_debug,					NULL)
	PHP_FE(memcache_get_stats,				NULL)
	PHP_FE(memcache_get_extended_stats,		NULL)
	PHP_FE(memcache_set_compress_threshold,	NULL)
	PHP_FE(memcache_increment,				NULL)
	PHP_FE(memcache_decrement,				NULL)
	PHP_FE(memcache_close,					NULL)
	PHP_FE(memcache_flush,					NULL)
	PHP_FE(memcache_set_sasl_auth_data,		NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_pool_class_functions[] = {
	PHP_NAMED_FE(connect,				zif_memcache_pool_connect,			NULL)
	PHP_NAMED_FE(addserver,				zif_memcache_pool_addserver,		NULL)
	PHP_FALIAS(setserverparams,			memcache_set_server_params,			NULL)
	PHP_FALIAS(setfailurecallback,		memcache_set_failure_callback,		NULL)
	PHP_FALIAS(getserverstatus,			memcache_get_server_status,			NULL)
	PHP_NAMED_FE(findserver,			zif_memcache_pool_findserver,		NULL)
	PHP_FALIAS(getversion,				memcache_get_version,				NULL)
	PHP_FALIAS(add,						memcache_add,						NULL)
	PHP_FALIAS(set,						memcache_set,						NULL)
	PHP_FALIAS(replace,					memcache_replace,					NULL)
	PHP_FALIAS(cas,						memcache_cas,						NULL)
	PHP_FALIAS(append,					memcache_append,					NULL)
	PHP_FALIAS(prepend,					memcache_prepend,					NULL)
	PHP_FALIAS(get,						memcache_get,						arginfo_memcache_object_get)
	PHP_FALIAS(delete,					memcache_delete,					NULL)
	PHP_FALIAS(getstats,				memcache_get_stats,					NULL)
	PHP_FALIAS(getextendedstats,		memcache_get_extended_stats,		NULL)
	PHP_FALIAS(setcompressthreshold,	memcache_set_compress_threshold,	NULL)
	PHP_FALIAS(increment,				memcache_increment,					NULL)
	PHP_FALIAS(decrement,				memcache_decrement,					NULL)
	PHP_FALIAS(close,					memcache_close,						NULL)
	PHP_FALIAS(flush,					memcache_flush,						NULL)
	PHP_FALIAS(setSaslAuthData,			memcache_set_sasl_auth_data,				NULL)
	
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
	STANDARD_MODULE_HEADER,
	"memcache",
	memcache_functions,
	PHP_MINIT(memcache),
	PHP_MSHUTDOWN(memcache),
	PHP_RINIT(memcache),
	NULL,
	PHP_MINFO(memcache),
	PHP_MEMCACHE_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHE
ZEND_GET_MODULE(memcache)
#endif

static PHP_INI_MH(OnUpdateChunkSize) /* {{{ */
{
	zend_long val;
	char *endptr = NULL;

	val = ZEND_STRTOL(ZSTR_VAL(new_value), &endptr, 10);
	if (!endptr || (*endptr != '\0') || val <= 0) {
		php_error_docref(NULL, E_WARNING, "memcache.chunk_size must be a positive integer ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
/* }}} */

static PHP_INI_MH(OnUpdateFailoverAttempts) /* {{{ */
{
	zend_long val;
	char *endptr = NULL;

	val = ZEND_STRTOL(ZSTR_VAL(new_value), &endptr, 10);
	if (!endptr || (*endptr != '\0') || val <= 0) {
		php_error_docref(NULL, E_WARNING, "memcache.max_failover_attempts must be a positive integer ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
/* }}} */

static PHP_INI_MH(OnUpdateProtocol) /* {{{ */
{
	if (!strcasecmp(ZSTR_VAL(new_value), "ascii")) {
		MEMCACHE_G(protocol) = MMC_ASCII_PROTOCOL;
	}
	else if (!strcasecmp(ZSTR_VAL(new_value), "binary")) {
		MEMCACHE_G(protocol) = MMC_BINARY_PROTOCOL;
	}
	else {
		php_error_docref(NULL, E_WARNING, "memcache.protocol must be in set {ascii, binary} ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateHashStrategy) /* {{{ */
{
	if (!strcasecmp(ZSTR_VAL(new_value), "standard")) {
		MEMCACHE_G(hash_strategy) = MMC_STANDARD_HASH;
	}
	else if (!strcasecmp(ZSTR_VAL(new_value), "consistent")) {
		MEMCACHE_G(hash_strategy) = MMC_CONSISTENT_HASH;
	}
	else {
		php_error_docref(NULL, E_WARNING, "memcache.hash_strategy must be in set {standard, consistent} ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateHashFunction) /* {{{ */
{
	if (!strcasecmp(ZSTR_VAL(new_value), "crc32")) {
		MEMCACHE_G(hash_function) = MMC_HASH_CRC32;
	}
	else if (!strcasecmp(ZSTR_VAL(new_value), "fnv")) {
		MEMCACHE_G(hash_function) = MMC_HASH_FNV1A;
	}
	else {
		php_error_docref(NULL, E_WARNING, "memcache.hash_function must be in set {crc32, fnv} ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

static PHP_INI_MH(OnUpdateRedundancy) /* {{{ */
{
	zend_long val;
	char *endptr = NULL;

	val = ZEND_STRTOL(ZSTR_VAL(new_value), &endptr, 10);
	if (!endptr || (*endptr != '\0') || val <= 0) {
		php_error_docref(NULL, E_WARNING, "memcache.redundancy must be a positive integer ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
/* }}} */

static PHP_INI_MH(OnUpdateCompressThreshold) /* {{{ */
{
	zend_long val;
	char *endptr = NULL;

	val = ZEND_STRTOL(ZSTR_VAL(new_value), &endptr, 10);
	if (!endptr || (*endptr != '\0') || val < 0) {
		php_error_docref(NULL, E_WARNING, "memcache.compress_threshold must be a positive integer ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
/* }}} */

static PHP_INI_MH(OnUpdateLockTimeout) /* {{{ */
{
	zend_long val;
	char *endptr = NULL;

	val = ZEND_STRTOL(ZSTR_VAL(new_value), &endptr, 10);
	if (!endptr || (*endptr != '\0') || val <= 0) {
		php_error_docref(NULL, E_WARNING, "memcache.lock_timeout must be a positive integer ('%s' given)", ZSTR_VAL(new_value));
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
}
/* }}} */

static PHP_INI_MH(OnUpdatePrefixStaticKey) /* {{{ */
{
	int i;

	if (new_value) {
		for (i=0 ; i<ZSTR_LEN(new_value) ; i++) {
			if (ZSTR_VAL(new_value)[i]=='.') {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.session_prefix_static_key cannot have dot inside (.)");
				return FAILURE;
			}
		}
	}

	return OnUpdateString(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);
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
	STD_PHP_INI_ENTRY("memcache.compress_threshold",	"20000",		PHP_INI_ALL, OnUpdateCompressThreshold,	compress_threshold,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.lock_timeout",			"15",			PHP_INI_ALL, OnUpdateLockTimeout,		lock_timeout,		zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_prefix_host_key",       			"0",			PHP_INI_ALL, OnUpdateBool, session_prefix_host_key, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_prefix_host_key_remove_www",    	"1",			PHP_INI_ALL, OnUpdateBool, session_prefix_host_key_remove_www, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_prefix_host_key_remove_subdomain",  "0",			PHP_INI_ALL, OnUpdateBool, session_prefix_host_key_remove_subdomain, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_prefix_static_key",         		NULL,			PHP_INI_ALL, OnUpdatePrefixStaticKey, session_prefix_static_key, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.session_save_path",         				NULL,			PHP_INI_ALL, OnUpdateString, session_save_path, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.prefix_host_key",       					"0",			PHP_INI_ALL, OnUpdateBool, prefix_host_key, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.prefix_host_key_remove_www",    			"1",			PHP_INI_ALL, OnUpdateBool, prefix_host_key_remove_www, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.prefix_host_key_remove_subdomain",  		"0",			PHP_INI_ALL, OnUpdateBool, prefix_host_key_remove_subdomain, zend_memcache_globals, memcache_globals)
	STD_PHP_INI_ENTRY("memcache.prefix_static_key",         				NULL,			PHP_INI_ALL, OnUpdatePrefixStaticKey, prefix_static_key, zend_memcache_globals, memcache_globals)
PHP_INI_END()
/* }}} */

/* {{{ internal function protos */
static void _mmc_pool_list_dtor(zend_resource*);
static void _mmc_server_list_dtor(zend_resource*);
static void php_mmc_set_failure_callback(mmc_pool_t *, zval *, zval *);
static void php_mmc_failure_callback(mmc_pool_t *, mmc_t *, zval *);
/* }}} */

/* {{{ php_memcache_init_globals()
*/
static void php_memcache_init_globals(zend_memcache_globals *memcache_globals_p)
{
	MEMCACHE_G(hash_strategy)	  = MMC_STANDARD_HASH;
	MEMCACHE_G(hash_function)	  = MMC_HASH_CRC32;
}
/* }}} */

/* {{{ get_session_key_prefix
 */
static char *get_session_key_prefix() {
	char *server_name=NULL, *prefix=NULL;
	int static_key_len=0, server_name_len=0, i;
	zval *array, *token;

	if (MEMCACHE_G(session_prefix_static_key)) {
		static_key_len=strlen(MEMCACHE_G(session_prefix_static_key));
	}

	zend_is_auto_global_str("_SERVER", sizeof("_SERVER")-1);

	if (MEMCACHE_G(session_prefix_host_key)) {
		if ((array = zend_hash_str_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER")-1)) &&
				Z_TYPE_P(array) == IS_ARRAY &&
				(token = zend_hash_str_find(Z_ARRVAL_P(array), "SERVER_NAME", sizeof("SERVER_NAME")-1)) &&
				Z_TYPE_P(token) == IS_STRING) {

			if (MEMCACHE_G(session_prefix_host_key_remove_www) && !strncasecmp("www.",Z_STRVAL_P(token),4)) {
				server_name=Z_STRVAL_P(token)+4;
			} else {
				server_name=Z_STRVAL_P(token);
			}

			if(MEMCACHE_G(session_prefix_host_key_remove_subdomain) && server_name) {
				int dots=0;
				char *dots_ptr[3]={NULL,NULL,NULL};

				for (i=strlen(server_name) ; i>0 ; i--) {
					if (dots==sizeof(dots_ptr)) {
						break;
					}

					if (server_name[i]=='.') {
						dots_ptr[dots]=&server_name[i];
						dots++;
					}
				}

				if (dots_ptr[1] && *(dots_ptr[1]+1)) {
					server_name=dots_ptr[1]+1;
				}

			}

			server_name_len=(strlen(server_name));
		}
	}

	if (!static_key_len && !server_name_len) {
		return NULL;
	}

	prefix=emalloc(static_key_len + server_name_len + 1);

	if (static_key_len)
		memcpy(prefix, MEMCACHE_G(session_prefix_static_key), static_key_len);

	if (server_name_len)
		memcpy(prefix + static_key_len, server_name, server_name_len);

	prefix[static_key_len + server_name_len]='\0';

	return prefix;
}
/* }}} */

/* get_key_prefix
 */
static char *get_key_prefix() {
	char *server_name=NULL, *prefix=NULL;
	int static_key_len=0, server_name_len=0, i;
	zval *array, *token;

	if (MEMCACHE_G(prefix_static_key)) {
		static_key_len=strlen(MEMCACHE_G(prefix_static_key));
	}

	if (MEMCACHE_G(prefix_host_key)) {

		if ((array = zend_hash_str_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER")-1)) &&
						Z_TYPE_P(array) == IS_ARRAY &&
						(token = zend_hash_str_find(Z_ARRVAL_P(array), "SERVER_NAME", sizeof("SERVER_NAME")-1)) &&
						Z_TYPE_P(token) == IS_STRING) {

			if (MEMCACHE_G(prefix_host_key_remove_www) && !strncasecmp("www.",Z_STRVAL_P(token),4)) {
				server_name=Z_STRVAL_P(token)+4;
			} else {
				server_name=Z_STRVAL_P(token);
			}

			if(MEMCACHE_G(prefix_host_key_remove_subdomain) && server_name) {
				int dots=0;
				char *dots_ptr[3]={NULL,NULL,NULL};

				for (i=strlen(server_name) ; i>0 ; i--) {
					if (dots==sizeof(dots_ptr)) {
						break;
					}
					if (server_name[i]=='.') {
						dots_ptr[dots]=&server_name[i];
						dots++;
					}
				}

				if (dots_ptr[1] && *(dots_ptr[1]+1)) {
					server_name=dots_ptr[1]+1;
				}

			}

			server_name_len=(strlen(server_name));
		}
	}

	if (!static_key_len && !server_name_len) {
		return NULL;
	}

	prefix=emalloc(static_key_len + server_name_len + 1);

	if (static_key_len)
		memcpy(prefix, MEMCACHE_G(prefix_static_key), static_key_len);

	if (server_name_len)
		memcpy(prefix + static_key_len, server_name, server_name_len);

	prefix[static_key_len + server_name_len]='\0';

	return prefix;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(memcache)
{
	MEMCACHE_G(session_key_prefix) = get_session_key_prefix(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memcache)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "MemcachePool", php_memcache_pool_class_functions);
	memcache_pool_ce = zend_register_internal_class(&ce);

	INIT_CLASS_ENTRY(ce, "Memcache", php_memcache_class_functions);
	memcache_ce = zend_register_internal_class_ex(&ce, memcache_pool_ce);

	le_memcache_pool = zend_register_list_destructors_ex(_mmc_pool_list_dtor, NULL, "memcache connection", module_number);
	le_memcache_server = zend_register_list_destructors_ex(NULL, _mmc_server_list_dtor, "persistent memcache connection", module_number);

#ifdef ZTS
	ts_allocate_id(&memcache_globals_id, sizeof(zend_memcache_globals), (ts_allocate_ctor) php_memcache_init_globals, NULL);
#else
	php_memcache_init_globals(&memcache_globals);
#endif

	REGISTER_LONG_CONSTANT("MEMCACHE_COMPRESSED", MMC_COMPRESSED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MEMCACHE_USER1", MMC_RESERVED_APPLICATIONDEFINEDFLAG_12, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MEMCACHE_USER2", MMC_RESERVED_APPLICATIONDEFINEDFLAG_13, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MEMCACHE_USER3", MMC_RESERVED_APPLICATIONDEFINEDFLAG_14, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MEMCACHE_USER4", MMC_RESERVED_APPLICATIONDEFINEDFLAG_15, CONST_CS | CONST_PERSISTENT);
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

static void _mmc_pool_list_dtor(zend_resource *rsrc) /* {{{ */
{
	mmc_pool_t *pool = (mmc_pool_t *)rsrc->ptr;

	if (!Z_ISUNDEF(pool->failure_callback_param)) {
		Z_DELREF(pool->failure_callback_param);
		ZVAL_UNDEF(&pool->failure_callback_param);
	}

	mmc_pool_free(pool);
}
/* }}} */

static void _mmc_server_list_dtor(zend_resource *rsrc) /* {{{ */
{
	mmc_server_free((mmc_t *)rsrc->ptr);
}
/* }}} */

static int mmc_get_pool(zval *id, mmc_pool_t **pool) /* {{{ */
{
	zval *zv;

	if (Z_TYPE_P(id) != IS_OBJECT || (zv = zend_hash_str_find(Z_OBJPROP_P(id), "connection", sizeof("connection")-1)) == NULL) {
		php_error_docref(NULL, E_WARNING, "No servers added to memcache connection");
		return 0;
	}

	if (Z_TYPE_P(zv) != IS_RESOURCE || (*pool = zend_fetch_resource_ex(zv, "connection", le_memcache_pool)) == NULL) {
			php_error_docref(NULL, E_WARNING, "Invalid MemcachePool->connection member variable");
			return 0;
	}

	return 1;
}
/* }}} */

int mmc_stored_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	handles SET/ADD/REPLACE response, param is a zval pointer to store result into {{{ */
{
	zval *result = (zval *)param;

	if (response == MMC_OK) {
		if (Z_TYPE_P(result) == IS_NULL) {
			ZVAL_TRUE(result);
		}

		return MMC_REQUEST_DONE;
	}

	/* return FALSE or catch memory errors without failover */
	if (response == MMC_RESPONSE_EXISTS || response == MMC_RESPONSE_OUT_OF_MEMORY || response == MMC_RESPONSE_TOO_LARGE
			|| response == MMC_RESPONSE_CLIENT_ERROR) {
		ZVAL_FALSE(result);

		if (response != MMC_RESPONSE_EXISTS) {
			/* trigger notice but no need for failover */
			php_error_docref(NULL, E_NOTICE, "Server %s (tcp %d, udp %d) failed with: %s (%d)",
				mmc->host, mmc->tcp.port, mmc->udp.port, message, response);
		}

		return MMC_REQUEST_DONE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0);
}
/* }}} */

static void php_mmc_store(INTERNAL_FUNCTION_PARAMETERS, int op) /* {{{ */
{
	mmc_pool_t *pool;
	mmc_request_t *request;
	zval *keys, *value = 0, *mmc_object = getThis();
	zend_long flags = 0, exptime = 0, cas = 0;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oz|zlll", &mmc_object, memcache_pool_ce, &keys, &value, &flags, &exptime, &cas) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zlll", &keys, &value, &flags, &exptime, &cas) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	RETVAL_NULL();

	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zend_string *key;
		zval *val;
		zend_ulong index;
		int release;

		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(keys), index, key, val ) {
			if (key == NULL) {
				key = strpprintf(0, ZEND_ULONG_FMT, index);
				release = 1;
			} else {
				release = 0;
			}

			/* allocate request */
			request = mmc_pool_request(pool, MMC_PROTO_TCP,
					mmc_stored_handler, return_value, mmc_pool_failover_handler, NULL);

			if (mmc_prepare_key_ex(ZSTR_VAL(key), ZSTR_LEN(key), request->key, &(request->key_len), MEMCACHE_G(key_prefix)) != MMC_OK) {
				php_error_docref(NULL, E_WARNING, "Invalid key");
				mmc_pool_release(pool, request);
				if (release) {
					zend_string_release(key);
				}
				continue;
			}

			if (release) {
				zend_string_release(key);
			}

			/* assemble command */
			if (pool->protocol->store(pool, request, op, request->key, request->key_len, flags, exptime, cas, val) != MMC_OK) {
				mmc_pool_release(pool, request);
				continue;
			}

			/* schedule request */
			if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy)) != MMC_OK) {
				continue;
			}

			/* begin sending requests immediatly */
			mmc_pool_select(pool);
		} ZEND_HASH_FOREACH_END();
	}
	else if (value) {
		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stored_handler, return_value, mmc_pool_failover_handler, NULL);

		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			php_error_docref(NULL, E_WARNING, "Invalid key");
			mmc_pool_release(pool, request);
			RETURN_FALSE;
		}

		/* assemble command */
		if (pool->protocol->store(pool, request, op, request->key, request->key_len, flags, exptime, cas, value) != MMC_OK) {
			mmc_pool_release(pool, request);
			RETURN_FALSE;
		}

		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy)) != MMC_OK) {
			RETURN_FALSE;
		}
	}
	else {
		WRONG_PARAM_COUNT;
	}

	/* execute all requests */
	mmc_pool_run(pool);

	if (Z_TYPE_P(return_value) == IS_NULL) {
		RETVAL_FALSE;
	}
}
/* }}} */

int mmc_numeric_response_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	handles a mutate response line, param is a zval pointer to store result into {{{ */
{
	zval *result = (zval *)param;

	if (response == MMC_OK) {
		if (Z_TYPE_P(result) == IS_ARRAY) {
			add_assoc_bool_ex(result, request->key, request->key_len + 1, 1);
		}
		else if (Z_TYPE_P(result) == IS_NULL) {
			/* switch only from null to true, not from false to true */
			ZVAL_TRUE(result);
		}

		return MMC_REQUEST_DONE;
	}

	if (response == MMC_RESPONSE_NOT_FOUND || response == MMC_RESPONSE_CLIENT_ERROR) {
		if (Z_TYPE_P(result) == IS_ARRAY) {
			add_assoc_bool_ex(result, request->key, request->key_len + 1, 0);
		}
		else {
			ZVAL_FALSE(result);
		}

		if (response != MMC_RESPONSE_NOT_FOUND) {
			php_error_docref(NULL,
					E_NOTICE, "Server %s (tcp %d, udp %d) failed with: %s (%d)",
					mmc->host, mmc->tcp.port,
					mmc->udp.port, message, response);
		}

		return MMC_REQUEST_DONE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0);
}
/* }}} */

static void php_mmc_numeric(INTERNAL_FUNCTION_PARAMETERS, int deleted, int invert) /*
	sends one or several commands which have a single optional numeric parameter (incr, decr, delete) {{{ */
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();

	zval *keys;
	zend_long value = 1, defval = 0, exptime = 0;
	mmc_request_t *request;
	void *value_handler_param[3];
	int defval_used = 0;

	if (mmc_object == NULL) {
		if (deleted) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oz|l", &mmc_object, memcache_pool_ce, &keys, &value) == FAILURE) {
				return;
			}
		}
		else {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oz|lll", &mmc_object, memcache_pool_ce, &keys, &value, &defval, &exptime) == FAILURE) {
				return;
			}

			defval_used = ZEND_NUM_ARGS() >= 4;
		}
	}
	else {
		if (deleted) {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|l", &keys, &value) == FAILURE) {
				return;
			}
		}
		else {
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|lll", &keys, &value, &defval, &exptime) == FAILURE) {
				return;
			}

			defval_used = ZEND_NUM_ARGS() >= 3;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	value_handler_param[0] = return_value;
	value_handler_param[1] = NULL;
	value_handler_param[2] = NULL;

	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zval *key;

		if (deleted) {
			/* changed to true/false by mmc_numeric_response_handler */
			RETVAL_NULL();
		}
		else {
			/* populated with responses by mmc_numeric_response_handler and mmc_value_handler_multi */
			array_init(return_value);
		}

		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(keys), key) {
			/* allocate request */
			request = mmc_pool_request(
					pool, MMC_PROTO_TCP, mmc_numeric_response_handler, return_value,
					mmc_pool_failover_handler, NULL);

			request->value_handler = mmc_value_handler_multi;
			request->value_handler_param = value_handler_param;

			if (mmc_prepare_key(key, request->key, &(request->key_len)) != MMC_OK) {
				mmc_pool_release(pool, request);
				php_error_docref(NULL, E_WARNING, "Invalid key");
				continue;
			}

			if (deleted) {
				pool->protocol->delete(request, request->key, request->key_len, exptime);
			}
			else {
				pool->protocol->mutate(request, key, request->key, request->key_len, invert ? -value : value, defval, defval_used, exptime);
			}

			/* schedule request */
			if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy)) != MMC_OK) {
				continue;
			}

			/* begin sending requests immediatly */
			mmc_pool_select(pool);
		} ZEND_HASH_FOREACH_END();

	}
	else {
		/* changed to true/false by mmc_numeric_response_handler or set to a value
		 * by mmc_value_handler_single if incr/decr returns one */
		RETVAL_NULL();

		/* allocate request */
		request = mmc_pool_request(pool, MMC_PROTO_TCP,
			mmc_numeric_response_handler, return_value, mmc_pool_failover_handler, NULL);

		request->value_handler = mmc_value_handler_single;
		request->value_handler_param = value_handler_param;

		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			php_error_docref(NULL, E_WARNING, "Invalid key");
			RETURN_FALSE;
		}

		if (deleted) {
			pool->protocol->delete(request, request->key, request->key_len, exptime);
		}
		else {
			pool->protocol->mutate(request, keys, request->key, request->key_len, invert ? -value : value, defval, defval_used, exptime);
		}

		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy)) != MMC_OK) {
			RETURN_FALSE;
		}
	}

	/* execute all requests */
	mmc_pool_run(pool);
}
/* }}} */


	/*TODO: in php73, we should use zend_register_persistent_resource , e.g.:

	char *persistent_id;
	persistent_id = pemalloc(key_len + 1, 1);
	memcpy((char *)persistent_id, key, key_len+1);
	if (zend_register_persistent_resource ( (char*) persistent_id, key_len, mmc, le_memcache_server) == NULL) ;

	then not forget to pefree, check refcounts in _mmc_server_free / _mmc_server_list_dtor ,  etc. 
	otherwise we will leak mem with persistent connections /run into other trouble with later versions
	*/
mmc_t *mmc_find_persistent(const char *host, int host_len, unsigned short port, unsigned short udp_port, double timeout, int retry_interval) /* {{{ */
{
	mmc_t *mmc;
	zend_resource *le;
	char *key;
	int key_len;

	key_len = spprintf(&key, 0, "memcache:server:%s:%u:%u", host, port, udp_port);

	if ((le = zend_hash_str_find_ptr(&EG(persistent_list), key, key_len)) == NULL) {
		mmc = mmc_server_new(host, host_len, port, udp_port, 1, timeout, retry_interval);


		le = zend_register_resource(mmc, le_memcache_server);

		/* register new persistent connection */
		if (zend_hash_str_update_mem(&EG(persistent_list), key, key_len, le, sizeof(*le)) == NULL) {
			mmc_server_free(mmc);
			mmc = NULL;
		} else {
			MEMCACHE_LIST_INSERT(mmc, le_memcache_server);
		}
	}
	else if (le->type != le_memcache_server || le->ptr == NULL) {
		zend_hash_str_del(&EG(persistent_list), key, key_len);

		mmc = mmc_server_new(host, host_len, port, udp_port, 1, timeout, retry_interval);
		le->type = le_memcache_server;
		le->ptr  = mmc;

#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(le) = 1;
#else
		GC_SET_REFCOUNT(le, 1);
#endif


		/* register new persistent connection */
		if (zend_hash_str_update_mem(&EG(persistent_list), key, key_len, le, sizeof(*le)) == NULL) {
			mmc_server_free(mmc);
			mmc = NULL;
		}
		else {
			MEMCACHE_LIST_INSERT(mmc, le_memcache_server);
		}
	}
	else {
		mmc = (mmc_t *)le->ptr;
		mmc->timeout = double_to_timeval(timeout);
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
	zend_bool persistent, double timeout, long retry_interval, zend_bool status, mmc_pool_t **pool_result) /* {{{ */
{
	zval *connection;
	mmc_pool_t *pool;
	mmc_t *mmc;
	zend_resource *list_res;

	if (weight < 1) {
		php_error_docref(NULL, E_WARNING, "weight must be a positive integer");
		return NULL;
	}

	if (tcp_port > 65635 || tcp_port < 0) {
		php_error_docref(NULL, E_WARNING, "invalid tcp port number");
		return NULL;
	}
	if (udp_port > 65635 || udp_port < 0) {
		php_error_docref(NULL, E_WARNING, "invalid udp port number");
		return NULL;
	}
	/* initialize pool if need be */
	if ((connection = zend_hash_str_find(Z_OBJPROP_P(mmc_object), "connection", sizeof("connection")-1)) == NULL) {
		pool = mmc_pool_new();
		pool->failure_callback = (mmc_failure_callback) &php_mmc_failure_callback;
		list_res = zend_register_resource(pool, le_memcache_pool);
		add_property_resource(mmc_object, "connection", list_res);

#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(list_res)++;
#else
		GC_ADDREF(list_res);
#endif

	}
	else {
		pool = zend_fetch_resource_ex(connection, "connection", le_memcache_pool);
		if (!pool) {
			php_error_docref(NULL, E_WARNING, "Unknown connection identifier");
			return NULL;
		}
	}

	/* binary protocol isn't support over UDP yet */
	if (udp_port && pool->protocol == &mmc_binary_protocol) {
		php_error_docref(NULL, E_NOTICE, "binary protocol isn't support over UDP, defaulting to TCP");
		udp_port = 0;
	}

	/* lazy initialization of server struct */
	if (persistent && status) {
		mmc = mmc_find_persistent(host, host_len, (unsigned short) tcp_port, (unsigned short) udp_port, timeout, retry_interval);
	}
	else {
		mmc = mmc_server_new(host, host_len, (unsigned short) tcp_port, (unsigned short) udp_port, 0, timeout, retry_interval);
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

	if (pool->protocol == &mmc_binary_protocol) {
		zval rv1, rv2;
		zval *username = zend_read_property(memcache_ce, mmc_object, "username", strlen("username"), 1, &rv1);
		zval *password = zend_read_property(memcache_ce, mmc_object, "password", strlen("password"), 1, &rv2);
		if (Z_TYPE_P(username) == IS_STRING && Z_TYPE_P(password) == IS_STRING) {
			if (Z_STRLEN_P(username) > 1  && Z_STRLEN_P(password) > 1) {
				mmc_request_t *request;
				zval sasl_value;

				/* allocate request */
				request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stored_handler, &sasl_value, mmc_pool_failover_handler, NULL);
				pool->protocol->set_sasl_auth_data(pool, request,  Z_STRVAL_P(username),  Z_STRVAL_P(password));

				/* schedule request */
				if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, MEMCACHE_G(redundancy)) != MMC_OK) {
					return NULL;
				}
			}
		}
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
	size_t host_len;
	zend_long tcp_port = MEMCACHE_G(default_port);
	double timeout = MMC_DEFAULT_TIMEOUT;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|ld", &host, &host_len, &tcp_port, &timeout) == FAILURE) {
		return;
	}

	/* initialize pool and object if need be */
	if (!mmc_object) {
		zend_resource *list_res;
		mmc_pool_t *pool = mmc_pool_new();
		pool->failure_callback = (mmc_failure_callback) &php_mmc_failure_callback;
		list_res = zend_register_resource(pool, le_memcache_pool);
		mmc_object = return_value;
		object_init_ex(mmc_object, memcache_ce);
		add_property_resource(mmc_object, "connection", list_res);
#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(list_res)++;
#else
		GC_ADDREF(list_res);
#endif
	} else {
		RETVAL_TRUE;
	}

	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, 0, 1, persistent, timeout, MMC_DEFAULT_RETRY, 1, NULL);
	if (mmc == NULL) {
		RETURN_FALSE;
	}

	/* force a reconnect attempt if stream EOF */
	if (mmc->tcp.stream != NULL && php_stream_eof(mmc->tcp.stream)) {
		mmc_server_disconnect(mmc, &(mmc->tcp));
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	/* force a tcp connect (if not persistently connected) */
	if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0) != MMC_OK) {
		php_error_docref(NULL, E_WARNING, "Can't connect to %s:%d, %s (%d)", host, mmc->tcp.port, mmc->error ? mmc->error : "Unknown error", mmc->errnum);
		RETURN_FALSE;
	}
}
/* }}} */

/*
 * STAT 6:chunk_size 64
 */
static int mmc_stats_parse_stat(char *start, char *end, zval *result)  /* {{{ */
{
	char *key;
	const char *space, *colon;
	zend_long index = 0;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	/* find space delimiting key and value */
	if ((space = php_memnstr(start, " ", 1, end)) == NULL) {
		return 0;
	}

	/* find colon delimiting subkeys */
	if ((colon = php_memnstr(start, ":", 1, space - 1)) != NULL) {
		zval *element, *elem;
		zval new_element;
		key = estrndup(start, colon - start);

		/* find existing or create subkey array in result */
		if ((is_numeric_string(key, colon - start, &index, NULL, 0) &&
			(elem = zend_hash_index_find(Z_ARRVAL_P(result), index)) != NULL) ||
			(elem = zend_hash_str_find(Z_ARRVAL_P(result), key, colon - start)) != NULL) {
			element = elem;
		}
		else {
			array_init(&new_element);
			add_assoc_zval_ex(result, key, colon - start, &new_element);
			element = &new_element;
		}

		efree(key);
		return mmc_stats_parse_stat(((char *) colon) + 1, end, element);
	}

	/* no more subkeys, add value under last subkey */
	key = estrndup(start, space - start);
	add_assoc_stringl_ex(result, key, ((char *) space) - start, ((char *) space) + 1, end - ((char *) space));
	efree(key);

	return 1;
}
/* }}} */

/*
 * ITEM test_key [3 b; 1157099416 s]
 */
static int mmc_stats_parse_item(char *start, char *end, zval *result)  /* {{{ */
{
	char *key;
	const char *space, *value, *value_end;
	zval element;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	/* find space delimiting key and value */
	if ((space = php_memnstr(start, " ", 1, end)) == NULL) {
		return 0;
	}

	array_init(&element);

	/* parse each contained value */
	for (value = php_memnstr(space, "[", 1, end); value != NULL && value <= end; value = php_memnstr(value + 1, ";", 1, end)) {
		do {
			value++;
		} while (*value == ' ' && value <= end);

		if (value <= end && (value_end = php_memnstr(value, " ", 1, end)) != NULL && value_end <= end) {
			add_next_index_stringl(&element, value, value_end - value);
		}
	}

	/* add parsed values under key */
	key = estrndup(start, space - start);
	add_assoc_zval_ex(result, key, space - start, &element);
	efree(key);

	return 1;
}
/* }}} */

static int mmc_stats_parse_generic(char *start, char *end, zval *result)  /* {{{ */
{
	const char *space;
	char *key;

	if (Z_TYPE_P(result) != IS_ARRAY) {
		array_init(result);
	}

	if (start < end) {
		if ((space = php_memnstr(start, " ", 1, end)) != NULL) {
			key = estrndup(start, space - start);
			add_assoc_stringl_ex(result, key, ((char *) space) - start + 1, ((char *) space) + 1, end - ((char *) space));
			efree(key);
		}
		else {
			add_next_index_stringl(result, start, end - start);
		}
	}
	else {
		return 0;
	}

	return 1;
}
/* }}} */

static void php_mmc_failure_callback(mmc_pool_t *pool, mmc_t *mmc, zval *param) /* {{{ */
{
	zval *callback;

	/* check for userspace callback */
	if (!Z_ISUNDEF_P(param) && (callback = zend_hash_str_find(Z_OBJPROP_P((zval *)param), "_failureCallback", sizeof("_failureCallback")-1)) != NULL && Z_TYPE_P(callback) != IS_NULL) {
		if (MEMCACHE_IS_CALLABLE(callback, 0, NULL)) {
			zval retval;
			zval *host, *tcp_port, *udp_port, *error, *errnum;
			zval params[5];

			ZVAL_UNDEF(&retval);

			host = &params[0];
			tcp_port = &params[1];
			udp_port = &params[2];
			error = &params[3];
			errnum = &params[4];

			ZVAL_STRING(host, mmc->host);
			ZVAL_LONG(tcp_port, mmc->tcp.port); ZVAL_LONG(udp_port, mmc->udp.port);

			if (mmc->error != NULL) {
				ZVAL_STRING(error, mmc->error);
			}
			else {
				ZVAL_NULL(error);
			}
			ZVAL_LONG(errnum, mmc->errnum);

			call_user_function_ex(EG(function_table), NULL, callback, &retval, 5, params, 0, NULL);

			zval_ptr_dtor(host);
			zval_ptr_dtor(tcp_port); zval_ptr_dtor(udp_port);
			zval_ptr_dtor(error); zval_ptr_dtor(errnum);

			if (Z_TYPE(retval) != IS_UNDEF) {
				zval_ptr_dtor(&retval);
			}
		}
		else {
			php_mmc_set_failure_callback(pool, (zval *)param, NULL);
			php_error_docref(NULL, E_WARNING, "Invalid failure callback");
		}
	}
	else {
		php_error_docref(NULL, E_NOTICE, "Server %s (tcp %d, udp %d) failed with: %s (%d)",
			mmc->host, mmc->tcp.port, mmc->udp.port, mmc->error, mmc->errnum);
	}
}
/* }}} */

static void php_mmc_set_failure_callback(mmc_pool_t *pool, zval *mmc_object, zval *callback)  /* {{{ */
{
	// Decrease refcount of old mmc_object
	if (!Z_ISUNDEF(pool->failure_callback_param)) {
		Z_DELREF(pool->failure_callback_param);
	}

	if (callback != NULL) {
		zval callback_tmp;

		callback_tmp = *callback;
		zval_copy_ctor(&callback_tmp);

		add_property_zval(mmc_object, "_failureCallback", &callback_tmp);

		zval_ptr_dtor(&callback_tmp);

		pool->failure_callback_param = *mmc_object;
		Z_ADDREF_P(mmc_object);
	}
	else {
		add_property_null(mmc_object, "_failureCallback");
		ZVAL_UNDEF(&pool->failure_callback_param);
	}
}
/* }}} */

/* ----------------
   module functions
   ---------------- */

/* {{{ proto bool MemcachePool::connect(string host [, int tcp_port [, int udp_port [, bool persistent [, int weight [, double timeout [, int retry_interval] ] ] ] ] ])
   Connects to server and returns a Memcache object */
PHP_NAMED_FUNCTION(zif_memcache_pool_connect)
{
	zval *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc;

	char *host;
	size_t host_len;
	zend_long tcp_port = MEMCACHE_G(default_port), udp_port = 0, weight = 1, retry_interval = MMC_DEFAULT_RETRY;
	double timeout = MMC_DEFAULT_TIMEOUT;
	zend_bool persistent = 1;

	MEMCACHE_G(key_prefix)=get_key_prefix();

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|llbldl",
		&host, &host_len, &tcp_port, &udp_port, &persistent, &weight, &timeout, &retry_interval) == FAILURE) {
		return;
	}

	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, udp_port, weight, persistent, timeout, retry_interval, 1, NULL);
	if (mmc == NULL) {
		RETURN_FALSE;
	}

	/* force a reconnect attempt if stream EOF */
	if (mmc->tcp.stream != NULL && php_stream_eof(mmc->tcp.stream)) {
		mmc_server_disconnect(mmc, &(mmc->tcp));
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	/* force a tcp connect (if not persistently connected) */
	if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0) != MMC_OK) {
		php_error_docref(NULL, E_WARNING, "Can't connect to %s:%d, %s (%d)", host, mmc->tcp.port, mmc->error ? mmc->error : "Unknown error", mmc->errnum);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object memcache_connect(string host [, int port [, double timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_connect)
{
	MEMCACHE_G(key_prefix)=get_key_prefix();
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto object memcache_pconnect(string host [, int port [, double timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_pconnect)
{
	MEMCACHE_G(key_prefix)=get_key_prefix();
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto bool MemcachePool::addServer(string host [, int tcp_port [, int udp_port [, bool persistent [, int weight [, double timeout [, int retry_interval [, bool status] ] ] ] ])
   Adds a server to the pool */
PHP_NAMED_FUNCTION(zif_memcache_pool_addserver)
{
	zval *mmc_object = getThis();
	mmc_t *mmc;

	char *host;
	size_t host_len;
	zend_long tcp_port = MEMCACHE_G(default_port), udp_port = 0, weight = 1, retry_interval = MMC_DEFAULT_RETRY;
	double timeout = MMC_DEFAULT_TIMEOUT;
	zend_bool persistent = 1, status = 1;

	MEMCACHE_G(key_prefix)=get_key_prefix();

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|llbldlb",
		&host, &host_len, &tcp_port, &udp_port, &persistent, &weight, &timeout, &retry_interval, &status) == FAILURE) {
		return;
	}

	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, udp_port, weight, persistent, timeout, retry_interval, status, NULL);
	if (mmc == NULL) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string MemcachePool::findServer(string key)
	Returns the server corresponding to a key
*/
PHP_NAMED_FUNCTION(zif_memcache_pool_findserver)
{
	zval *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc;

	zval *zkey;
	char key[MMC_MAX_KEY_LEN + 1];
	unsigned int key_len;
	zend_string *hostname;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zkey) == FAILURE) {
		return;
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	if (mmc_prepare_key(zkey, key, &key_len) != MMC_OK) {
		php_error_docref(NULL, E_WARNING, "Invalid key");
		RETURN_FALSE;
	}

	mmc = mmc_pool_find(pool, key, key_len);
	hostname = strpprintf(0, "%s:%d", mmc->host, mmc->tcp.port);
	RETURN_STR(hostname);
}
/* }}} */

/* {{{ proto bool memcache_add_server(string host [, int port [, bool persistent [, int weight [, double timeout [, int retry_interval [, bool status [, callback failure_callback ] ] ] ] ] ] ])
   Adds a connection to the pool. The order in which this function is called is significant */
PHP_FUNCTION(memcache_add_server)
{
	zval *mmc_object = getThis(), *failure_callback = NULL;
	mmc_pool_t *pool;
	mmc_t *mmc;

	char *host;
	size_t host_len;
	zend_long tcp_port = MEMCACHE_G(default_port), weight = 1, retry_interval = MMC_DEFAULT_RETRY;
	double timeout = MMC_DEFAULT_TIMEOUT;
	zend_bool persistent = 1, status = 1;

	MEMCACHE_G(key_prefix)=get_key_prefix();

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|lbldlbz",
			&host, &host_len, &tcp_port, &persistent, &weight, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Os|lbldlbz", &mmc_object, memcache_ce,
			&host, &host_len, &tcp_port, &persistent, &weight, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}

	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!MEMCACHE_IS_CALLABLE(failure_callback, 0, NULL)) {
			php_error_docref(NULL, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	mmc = php_mmc_pool_addserver(mmc_object, host, host_len, tcp_port, 0, weight, persistent, timeout, retry_interval, status, &pool);
	if (mmc == NULL) {
		RETURN_FALSE;
	}

	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		php_mmc_set_failure_callback(pool, mmc_object, failure_callback);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_set_server_params( string host [, int port [, double timeout [, int retry_interval [, bool status [, callback failure_callback ] ] ] ] ])
   Changes server parameters at runtime */
PHP_FUNCTION(memcache_set_server_params)
{
	zval *mmc_object = getThis(), *failure_callback = NULL;
	mmc_pool_t *pool;
	mmc_t *mmc = NULL;
	zend_long tcp_port = MEMCACHE_G(default_port), retry_interval = MMC_DEFAULT_RETRY;
	double timeout = MMC_DEFAULT_TIMEOUT;
	zend_bool status = 1;
	size_t host_len;
	int i;
	char *host;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|ldlbz",
			&host, &host_len, &tcp_port, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Os|ldlbz", &mmc_object, memcache_pool_ce,
			&host, &host_len, &tcp_port, &timeout, &retry_interval, &status, &failure_callback) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!strcmp(pool->servers[i]->host, host) && pool->servers[i]->tcp.port == tcp_port) {
			mmc = pool->servers[i];
			break;
		}
	}

	if (!mmc) {
		php_error_docref(NULL, E_WARNING, "Server not found in pool");
		RETURN_FALSE;
	}

	if (failure_callback != NULL && Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!MEMCACHE_IS_CALLABLE(failure_callback, 0, NULL)) {
			php_error_docref(NULL, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	mmc->timeout = double_to_timeval(timeout);
	mmc->tcp.retry_interval = retry_interval;

	/* store the smallest timeout for any server */
	if (timeval_to_double(mmc->timeout) < timeval_to_double(pool->timeout)) {
		pool->timeout = mmc->timeout;
	}

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
			php_mmc_set_failure_callback(pool, mmc_object, failure_callback);
		}
		else {
			php_mmc_set_failure_callback(pool, mmc_object, NULL);
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
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "z",
			&failure_callback) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oz", &mmc_object, memcache_pool_ce,
			&failure_callback) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(failure_callback) != IS_NULL) {
		if (!MEMCACHE_IS_CALLABLE(failure_callback, 0, NULL)) {
			php_error_docref(NULL, E_WARNING, "Invalid failure callback");
			RETURN_FALSE;
		}
	}

	if (Z_TYPE_P(failure_callback) != IS_NULL) {
		php_mmc_set_failure_callback(pool, mmc_object, failure_callback);
	}
	else {
		php_mmc_set_failure_callback(pool, mmc_object, NULL);
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
	zend_long tcp_port = MEMCACHE_G(default_port);
	size_t host_len;
	int i;
	char *host;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &host, &host_len, &tcp_port) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Os|l", &mmc_object, memcache_pool_ce, &host, &host_len, &tcp_port) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!strcmp(pool->servers[i]->host, host) && pool->servers[i]->tcp.port == tcp_port) {
			mmc = pool->servers[i];
			break;
		}
	}

	if (!mmc) {
		php_error_docref(NULL, E_WARNING, "Server not found in pool");
		RETURN_FALSE;
	}

	RETURN_LONG(mmc->tcp.status > MMC_STATUS_FAILED ? 1 : 0);
}
/* }}} */

static int mmc_version_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	parses the VERSION response line, param is a zval pointer to store version into {{{ */
{
	if (response != MMC_RESPONSE_ERROR) {
		char *version = emalloc(message_len + 1);

		if (sscanf(message, "VERSION %s", version) == 1) {
			ZVAL_STRING((zval *)param, version);
			efree(version);
		}
		else {
			efree(version);
			ZVAL_STRINGL((zval *)param, (char *)message, message_len);
		}

		return MMC_REQUEST_DONE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0);
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
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &mmc_object, memcache_pool_ce) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	RETVAL_FALSE;
	for (i=0; i<pool->num_servers; i++) {
		/* run command and check for valid return value */
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_version_handler, return_value, NULL, NULL);
		pool->protocol->version(request);

		if (mmc_pool_schedule(pool, pool->servers[i], request) == MMC_OK) {
			mmc_pool_run(pool);

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
	unsigned int flags, unsigned long cas, void *param) /*
	receives a multiple values, param is a zval** array to store value and flags in {{{ */
{
	zval **result = (zval **)param;

	/* add value to result */
	if (Z_TYPE_P(result[0]) != IS_ARRAY) {
		array_init(result[0]);
	}
	add_assoc_zval_ex(result[0], (char *)key, key_len, value);

	/* add flags to result */
	if (result[1] != NULL) {
		if (Z_TYPE_P(result[1]) != IS_ARRAY) {
			array_init(result[1]);
		}
		add_assoc_long_ex(result[1], (char *)key, key_len, flags);
	}

	/* add CAS value to result */
	if (result[2] != NULL) {
		if (Z_TYPE_P(result[2]) != IS_ARRAY) {
			array_init(result[2]);
		}
		add_assoc_long_ex(result[2], (char *)key, key_len, cas);
	}

	return MMC_REQUEST_DONE;
}
/* }}} */

int mmc_value_handler_single(
	const char *key, unsigned int key_len, zval *value,
	unsigned int flags, unsigned long cas, void *param) /*
	receives a single value, param is a zval pointer to store value to {{{ */
{
	zval **result = (zval **)param;
	ZVAL_ZVAL(result[0], value, 1, 1);

	if (result[1] != NULL) {
		ZVAL_LONG(result[1], flags);
	}

	if (result[2] != NULL) {
		ZVAL_LONG(result[2], cas);
	}

	return MMC_REQUEST_DONE;
}
/* }}} */

static int mmc_value_failover_handler(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param) /*
	uses keys and return value to reschedule requests to other servers, param is a zval ** pointer {{{ */
{
	zval *keys = ((zval **)param)[0], **value_handler_param = (zval **)((void **)param)[1];
	zval *key;

	if (!MEMCACHE_G(allow_failover) || request->failed_servers.len >= MEMCACHE_G(max_failover_attempts)) {
		mmc_pool_release(pool, request);
		return MMC_REQUEST_FAILURE;
	}

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(keys), key) {
		if (Z_TYPE_P(value_handler_param[0]) != IS_ARRAY ||
					!zend_hash_str_exists(Z_ARRVAL_P(value_handler_param[0]), Z_STRVAL_P(key), Z_STRLEN_P(key)))
				{
					mmc_pool_schedule_get(pool, MMC_PROTO_UDP,
						value_handler_param[2] != NULL ? MMC_OP_GETS : MMC_OP_GET, key,
						request->value_handler, request->value_handler_param,
						request->failover_handler, request->failover_handler_param, request);
				}
	} ZEND_HASH_FOREACH_END();

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
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oz|z/z/", &mmc_object, memcache_pool_ce, &keys, &flags, &cas) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z/z/", &keys, &flags, &cas) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	value_handler_param[0] = return_value;
	value_handler_param[1] = flags;
	value_handler_param[2] = cas;

	if (Z_TYPE_P(keys) == IS_ARRAY) {
		zval *zv;
		/* return empty array if no keys found */
		array_init(return_value);

		failover_handler_param[0] = keys;
		failover_handler_param[1] = value_handler_param;

		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(keys), zv) {
			/* schedule request */
			mmc_pool_schedule_get(pool, MMC_PROTO_UDP,
				cas != NULL ? MMC_OP_GETS : MMC_OP_GET, zv,
				mmc_value_handler_multi, value_handler_param,
				mmc_value_failover_handler, failover_handler_param, NULL);
		} ZEND_HASH_FOREACH_END();
	}
	else {
		mmc_request_t *request;

		/* return false if key isn't found */
		ZVAL_FALSE(return_value);

		/* allocate request */
		request = mmc_pool_request_get(
			pool, MMC_PROTO_TCP,
			mmc_value_handler_single, value_handler_param,
			mmc_pool_failover_handler, NULL);

		if (mmc_prepare_key(keys, request->key, &(request->key_len)) != MMC_OK) {
			mmc_pool_release(pool, request);
			php_error_docref(NULL, E_WARNING, "Invalid key");
			return;
		}

		pool->protocol->get(request, cas != NULL ? MMC_OP_GETS : MMC_OP_GET, keys, request->key, request->key_len);

		/* schedule request */
		if (mmc_pool_schedule_key(pool, request->key, request->key_len, request, 1) != MMC_OK) {
			return;
		}
	}

	/* execute all requests */
	mmc_pool_run(pool);
}
/* }}} */

static int mmc_stats_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
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
			if (mmc_stats_parse_stat(line + sizeof("STAT ")-1, line + message_len - 1, (zval *)param)) {
				return MMC_REQUEST_AGAIN;
			}
		}
		else if (mmc_str_left(line, "ITEM ", message_len, sizeof("ITEM ")-1)) {
			if (mmc_stats_parse_item(line + sizeof("ITEM ")-1, line + message_len - 1, (zval *)param)) {
				return MMC_REQUEST_AGAIN;
			}
		}
		else if (mmc_str_left(line, "END", message_len, sizeof("END")-1)) {
			return MMC_REQUEST_DONE;
		}
		else if (mmc_stats_parse_generic(line, line + message_len, (zval *)param)) {
			return MMC_REQUEST_AGAIN;
		}

		zval_dtor((zval *)param);
		ZVAL_FALSE((zval *)param);
		return MMC_REQUEST_FAILURE;
	}

	return mmc_request_failure(mmc, request->io, message, message_len, 0);
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
	int i;
	size_t type_len = 0;
	zend_long slabid = 0, limit = MMC_DEFAULT_CACHEDUMP_LIMIT;
	mmc_request_t *request;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|sll", &mmc_object, memcache_pool_ce, &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sll", &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	if (!mmc_stats_checktype(type)) {
		php_error_docref(NULL, E_WARNING, "Invalid stats type");
		RETURN_FALSE;
	}

	ZVAL_FALSE(return_value);

	for (i=0; i<pool->num_servers; i++) {
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stats_handler, return_value, NULL, NULL);
		pool->protocol->stats(request, type, slabid, limit);

		/* run command and check for valid return value */
		if (mmc_pool_schedule(pool, pool->servers[i], request) == MMC_OK) {
			mmc_pool_run(pool);

			if (Z_TYPE_P(return_value) != IS_FALSE) {
				break;
			}
		}
	}

	/* execute all requests */
	mmc_pool_run(pool);
}
/* }}} */

/* {{{ proto array memcache_get_extended_stats( object memcache [, string type [, int slabid [, int limit ] ] ])
   Returns statistics for each server in the pool */
PHP_FUNCTION(memcache_get_extended_stats)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();

	char *host, *type = NULL;
	int i;
	size_t host_len, type_len = 0;
	zend_long slabid = 0, limit = MMC_DEFAULT_CACHEDUMP_LIMIT;
	mmc_request_t *request;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|sll", &mmc_object, memcache_pool_ce, &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sll", &type, &type_len, &slabid, &limit) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool) || !pool->num_servers) {
		RETURN_FALSE;
	}

	if (!mmc_stats_checktype(type)) {
		php_error_docref(NULL, E_WARNING, "Invalid stats type");
		RETURN_FALSE;
	}

	array_init(return_value);

	for (i=0; i<pool->num_servers; i++) {
		zval new_stats;
		zval *stats;
		ZVAL_FALSE(&new_stats);

		host_len = spprintf(&host, 0, "%s:%u", pool->servers[i]->host, pool->servers[i]->tcp.port);
		stats = zend_symtable_str_update(Z_ARRVAL_P(return_value), host, host_len, &new_stats);
		efree(host);

		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_stats_handler, stats, NULL, NULL);
		pool->protocol->stats(request, type, slabid, limit);

		if (mmc_pool_schedule(pool, pool->servers[i], request) == MMC_OK) {
			mmc_pool_run(pool);
		}
	}

	/* execute all requests */
	mmc_pool_run(pool);
}
/* }}} */

/* {{{ proto array memcache_set_compress_threshold( object memcache, int threshold [, float min_savings ] )
   Set automatic compress threshold */
PHP_FUNCTION(memcache_set_compress_threshold)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();
	zend_long threshold;
	double min_savings = MMC_DEFAULT_SAVINGS;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Ol|d", &mmc_object, memcache_pool_ce, &threshold, &min_savings) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|d", &threshold, &min_savings) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	if (threshold < 0) {
		php_error_docref(NULL, E_WARNING, "threshold must be a positive integer");
		RETURN_FALSE;
	}
	pool->compress_threshold = threshold;

	if (min_savings != MMC_DEFAULT_SAVINGS) {
		if (min_savings < 0 || min_savings > 1) {
			php_error_docref(NULL, E_WARNING, "min_savings must be a float in the 0..1 range");
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
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &mmc_object, memcache_pool_ce) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	mmc_pool_close(pool);
	RETURN_TRUE;
}
/* }}} */

static int mmc_flush_handler(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	parses the OK response line, param is an int pointer to increment on success {{{ */
{
	if (response == MMC_OK) {
		(*((int *)param))++;
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

/* {{{ proto bool memcache_flush( object memcache [, int delay ] )
   Flushes cache, optionally at after the specified delay */
PHP_FUNCTION(memcache_flush)
{
	mmc_pool_t *pool;
	zval *mmc_object = getThis();

	mmc_request_t *request;
	unsigned int i, responses = 0;
	zend_long delay = 0;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|l", &mmc_object, memcache_pool_ce, &delay) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &delay) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		request = mmc_pool_request(pool, MMC_PROTO_TCP, mmc_flush_handler, &responses, NULL, NULL);
		pool->protocol->flush(request, delay);

		if (mmc_pool_schedule(pool, pool->servers[i], request) == MMC_OK) {
			/* begin sending requests immediatly */
			mmc_pool_select(pool);
		}
	}

	/* execute all requests */
	mmc_pool_run(pool);

	if (responses < pool->num_servers) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string memcache_set_sasl_data(object memcache, string username, string password)
   Set credentials for sals authentification */
PHP_FUNCTION(memcache_set_sasl_auth_data)
{
	zval *mmc_object = getThis();
	char *user;
	size_t user_length;
	char *password;
	size_t password_length;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "Oss", &mmc_object, memcache_pool_ce, &user, &user_length, &password, &password_length) == FAILURE) {
			return;
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &user, &user_length, &password, &password_length) == FAILURE) {
			return;
		}
	}
	if (user_length < 1 || password_length < 1) {
		RETURN_FALSE;
	}
	zend_update_property_stringl(memcache_pool_ce, mmc_object, "username", strlen("username"), user, user_length);
	zend_update_property_stringl(memcache_pool_ce, mmc_object, "password", strlen("password"), password, password_length);
	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool memcache_debug( bool onoff ) */
PHP_FUNCTION(memcache_debug)
{
	php_error_docref(NULL, E_WARNING, "memcache_debug() is deprecated, please use a debugger (like Eclipse + CDT)");
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
