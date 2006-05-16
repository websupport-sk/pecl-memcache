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

#include "php.h"
#include "php_ini.h"
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#if HAVE_MEMCACHE

#include <zlib.h>
#include <time.h>
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/crc32.h"
#include "php_network.h"
#include "php_memcache.h"

#ifndef ZEND_ENGINE_2
#define OnUpdateLong OnUpdateInt
#endif

/* True global resources - no need for thread safety here */
static int le_memcache_pool, le_pmemcache;
static zend_class_entry *memcache_class_entry_ptr;

ZEND_DECLARE_MODULE_GLOBALS(memcache)

/* {{{ memcache_functions[]
 */
zend_function_entry memcache_functions[] = {
	PHP_FE(memcache_connect,		NULL)
	PHP_FE(memcache_pconnect,		NULL)
	PHP_FE(memcache_add_server,		NULL)
	PHP_FE(memcache_get_version,	NULL)
	PHP_FE(memcache_add,			NULL)
	PHP_FE(memcache_set,			NULL)
	PHP_FE(memcache_replace,		NULL)
	PHP_FE(memcache_get,			NULL)
	PHP_FE(memcache_delete,			NULL)
	PHP_FE(memcache_debug,			NULL)
	PHP_FE(memcache_get_stats,		NULL)
	PHP_FE(memcache_get_extended_stats,		NULL)
	PHP_FE(memcache_set_compress_threshold,	NULL)
	PHP_FE(memcache_increment,		NULL)
	PHP_FE(memcache_decrement,		NULL)
	PHP_FE(memcache_close,			NULL)
	PHP_FE(memcache_flush,			NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_class_functions[] = {
	PHP_FALIAS(connect,			memcache_connect,			NULL)
	PHP_FALIAS(pconnect,		memcache_pconnect,			NULL)
	PHP_FALIAS(addserver,		memcache_add_server,		NULL)
	PHP_FALIAS(getversion,		memcache_get_version,		NULL)
	PHP_FALIAS(add,				memcache_add,				NULL)
	PHP_FALIAS(set,				memcache_set,				NULL)
	PHP_FALIAS(replace,			memcache_replace,			NULL)
	PHP_FALIAS(get,				memcache_get,				NULL)
	PHP_FALIAS(delete,			memcache_delete,			NULL)
	PHP_FALIAS(getstats,		memcache_get_stats,			NULL)
	PHP_FALIAS(getextendedstats,		memcache_get_extended_stats,		NULL)
	PHP_FALIAS(setcompressthreshold,	memcache_set_compress_threshold,	NULL)
	PHP_FALIAS(increment,		memcache_increment,			NULL)
	PHP_FALIAS(decrement,		memcache_decrement,			NULL)
	PHP_FALIAS(close,			memcache_close,				NULL)
	PHP_FALIAS(flush,			memcache_flush,				NULL)
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
	PHP_RINIT(memcache),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(memcache),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(memcache),
#if ZEND_MODULE_API_NO >= 20010901
	NO_VERSION_YET, 			/* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHE
ZEND_GET_MODULE(memcache)
#endif

static PHP_INI_MH(OnUpdateChunkSize) /* {{{ */
{
	char *endptr = NULL;
	long int lval;

	lval = strtol(new_value, &endptr, 10);
	if (NULL != endptr && new_value + new_value_length != endptr || lval <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache.chunk_size must be a positive integer ('%s' given)", new_value);
		return FAILURE;
	}

	return OnUpdateLong(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("memcache.allow_failover",	"1",		PHP_INI_ALL, OnUpdateLong,		allow_failover,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.default_port",		"11211",	PHP_INI_ALL, OnUpdateLong,		default_port,	zend_memcache_globals,	memcache_globals)
	STD_PHP_INI_ENTRY("memcache.chunk_size",		"8192",		PHP_INI_ALL, OnUpdateChunkSize,	chunk_size,		zend_memcache_globals,	memcache_globals)
PHP_INI_END()
/* }}} */

/* {{{ macros */
#define MMC_PREPARE_KEY(key, key_len) \
	php_strtr(key, key_len, "\t\r\n ", "____", 4); \

#if ZEND_DEBUG

#define MMC_DEBUG(info) \
{\
	mmc_debug info; \
}\

#else

#define MMC_DEBUG(info) \
{\
}\

#endif


/* }}} */

/* {{{ internal function protos */
static void _mmc_pool_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static void _mmc_pserver_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static mmc_pool_t *mmc_pool_new();
static void mmc_pool_add(mmc_pool_t *, mmc_t *, unsigned int);
static mmc_t *mmc_server_new(char *, int, unsigned short, int, int, int TSRMLS_DC);
static void mmc_server_free(mmc_t * TSRMLS_DC);
static void mmc_server_disconnect(mmc_t * TSRMLS_DC);
static void mmc_server_deactivate(mmc_t * TSRMLS_DC);
static int mmc_server_failure(mmc_t * TSRMLS_DC);
static mmc_t *mmc_server_find(mmc_pool_t *, char *, int TSRMLS_DC);
static unsigned int mmc_hash(char *, int);
static int mmc_compress(char **, int *, char *, int TSRMLS_DC);
static int mmc_uncompress(char **, long *, char *, int);
static int mmc_get_pool(zval *, mmc_pool_t ** TSRMLS_DC);
static int mmc_open(mmc_t *, int, char **, int * TSRMLS_DC);
static mmc_t *mmc_find_persistent(char *, int, int, int, int TSRMLS_DC);
static int mmc_close(mmc_t * TSRMLS_DC);
static int mmc_readline(mmc_t * TSRMLS_DC);
static char * mmc_get_version(mmc_t * TSRMLS_DC);
static int mmc_str_left(char *, char *, int, int);
static int mmc_sendcmd(mmc_t *, const char *, int TSRMLS_DC);
static int mmc_exec_storage_cmd(mmc_t *, char *, int, char *, int, int, int, char *, int TSRMLS_DC);
static int mmc_parse_response(char *, char **, int, int *, int *);
static int mmc_exec_retrieval_cmd(mmc_pool_t *, zval *, zval ** TSRMLS_DC);
static int mmc_exec_retrieval_cmd_multi(mmc_pool_t *, zval *, zval ** TSRMLS_DC);
static int mmc_read_value(mmc_t *, char **, zval ** TSRMLS_DC);
static int mmc_delete(mmc_t *, char *, int, int TSRMLS_DC);
static int mmc_flush(mmc_t * TSRMLS_DC);
static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *, int);
static int mmc_get_stats (mmc_t *, zval ** TSRMLS_DC);
static int mmc_incr_decr (mmc_t *, int, char *, int, int, long * TSRMLS_DC);
static void php_mmc_incr_decr (INTERNAL_FUNCTION_PARAMETERS, int);
static void php_mmc_connect (INTERNAL_FUNCTION_PARAMETERS, int);
/* }}} */

/* {{{ php_memcache_init_globals()
*/
static void php_memcache_init_globals(zend_memcache_globals *memcache_globals_p TSRMLS_DC)
{
	MEMCACHE_G(debug_mode)		  = 0;
	MEMCACHE_G(num_persistent)	  = 0;
	MEMCACHE_G(compression_level) = Z_DEFAULT_COMPRESSION;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memcache)
{
	zend_class_entry memcache_class_entry;
	INIT_CLASS_ENTRY(memcache_class_entry, "Memcache", php_memcache_class_functions);
	memcache_class_entry_ptr = zend_register_internal_class(&memcache_class_entry TSRMLS_CC);

	le_memcache_pool = zend_register_list_destructors_ex(_mmc_pool_list_dtor, NULL, "memcache connection", module_number);
	le_pmemcache = zend_register_list_destructors_ex(NULL, _mmc_pserver_list_dtor, "persistent memcache connection", module_number);

#ifdef ZTS
	ts_allocate_id(&memcache_globals_id, sizeof(zend_memcache_globals), (ts_allocate_ctor) php_memcache_init_globals, NULL);
#else
	php_memcache_init_globals(&memcache_globals TSRMLS_CC);
#endif

	REGISTER_LONG_CONSTANT("MEMCACHE_COMPRESSED",MMC_COMPRESSED, CONST_CS | CONST_PERSISTENT);
	REGISTER_INI_ENTRIES();

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

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(memcache)
{
	MEMCACHE_G(debug_mode) = 0;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(memcache)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(memcache)
{
	char buf[16];

	sprintf(buf, "%ld", MEMCACHE_G(num_persistent));

	php_info_print_table_start();
	php_info_print_table_header(2, "memcache support", "enabled");
	php_info_print_table_row(2, "Active persistent connections", buf);
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
	mmc_pool_t *pool = (mmc_pool_t *)rsrc->ptr;
	int i;
	for (i=0; i<pool->num_servers; i++) {
		if (!pool->servers[i]->persistent) {
			mmc_server_free(pool->servers[i] TSRMLS_CC);
		}
	}

	if (pool->num_servers) {
		efree(pool->servers);
		efree(pool->requests);
	}

	if (pool->num_buckets) {
		efree(pool->buckets);
	}

	efree(pool);
}
/* }}} */

static void _mmc_pserver_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	mmc_server_free((mmc_t *)rsrc->ptr TSRMLS_CC);
}
/* }}} */

static mmc_t *mmc_server_new(char *host, int host_len, unsigned short port, int persistent, int timeout, int retry_interval TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;

	if (persistent) {
		mmc = malloc(sizeof(mmc_t));
		mmc->host = malloc(host_len + 1);
	}
	else {
		mmc = emalloc(sizeof(mmc_t));
		mmc->host = emalloc(host_len + 1);
	}

	mmc->stream = NULL;
	mmc->status = MMC_STATUS_DISCONNECTED;
	memset(&(mmc->outbuf), 0, sizeof(smart_str));

	strncpy(mmc->host, host, host_len);
	mmc->host[host_len] = '\0';
	mmc->port = port;

	mmc->persistent = persistent;
	if (persistent) {
		MEMCACHE_G(num_persistent)++;
	}

	mmc->timeout = timeout;
	mmc->retry_interval = retry_interval;

	return mmc;
}
/* }}} */

static void mmc_server_free(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	if (mmc->persistent) {
		free(mmc->host);
		free(mmc);
		MEMCACHE_G(num_persistent)--;
	}
	else {
		if (mmc->stream != NULL) {
			php_stream_close(mmc->stream);
		}
		efree(mmc->host);
		efree(mmc);
	}
}
/* }}} */

static mmc_pool_t *mmc_pool_new() /* {{{ */
{
	mmc_pool_t *pool = emalloc(sizeof(mmc_pool_t));
	pool->num_servers = 0;
	pool->num_buckets = 0;
	pool->compress_threshold = 0;
	pool->min_compress_savings = MMC_DEFAULT_SAVINGS;
	return pool;
}
/* }}} */

static void mmc_pool_add(mmc_pool_t *pool, mmc_t *mmc, unsigned int weight) /* {{{ */
{
	int i;

	/* add server and a preallocated request pointer */
	if (pool->num_servers) {
		pool->servers = erealloc(pool->servers, sizeof(mmc_t *) * (pool->num_servers + 1));
		pool->requests = erealloc(pool->requests, sizeof(mmc_t *) * (pool->num_servers + 1));
	}
	else {
		pool->servers = emalloc(sizeof(mmc_t *));
		pool->requests = emalloc(sizeof(mmc_t *));
	}

	pool->servers[pool->num_servers] = mmc;
	pool->num_servers++;

	/* add weight number of buckets for this server */
	if (pool->num_buckets) {
		pool->buckets = erealloc(pool->buckets, sizeof(mmc_t *) * (pool->num_buckets + weight));
	}
	else {
		pool->buckets = emalloc(sizeof(mmc_t *) * (pool->num_buckets + weight));
	}

	for (i=0; i<weight; i++) {
		pool->buckets[pool->num_buckets + i] = mmc;
	}
	pool->num_buckets += weight;
}
/* }}} */

static int mmc_compress(char **result_data, int *result_len, char *data, int data_len TSRMLS_DC) /* {{{ */
{
	int   status, level = MEMCACHE_G(compression_level);

	*result_len = data_len + (data_len / 1000) + 15 + 1; /* some magic from zlib.c */
	*result_data = (char *) emalloc(*result_len);

	if (!*result_data) {
		return 0;
	}

	if (level >= 0) {
		status = compress2(*result_data, (unsigned long *)result_len, data, data_len, level);
	} else {
		status = compress(*result_data, (unsigned long *)result_len, data, data_len);
	}

	if (status == Z_OK) {
		*result_data = erealloc(*result_data, *result_len + 1);
		(*result_data)[*result_len] = '\0';
		return 1;
	} else {
		efree(*result_data);
		return 0;
	}
}
/* }}}*/

static int mmc_uncompress(char **result_data, long *result_len, char *data, int data_len) /* {{{ */
{
	int status;
	unsigned int factor=1, maxfactor=16;
	char *tmp1=NULL;

	do {
		*result_len = (unsigned long)data_len * (1 << factor++);
		*result_data = (char *) erealloc(tmp1, *result_len);
		status = uncompress(*result_data, result_len, data, data_len);
		tmp1 = *result_data;
	} while ((status == Z_BUF_ERROR) && (factor < maxfactor));

	if (status == Z_OK) {
		*result_data = erealloc(*result_data, *result_len + 1);
		(*result_data)[ *result_len ] = '\0';
		return 1;
	} else {
		efree(*result_data);
		return 0;
	}
}
/* }}}*/

static void mmc_debug( const char *format, ...) /* {{{ */
{
	TSRMLS_FETCH();

	if (MEMCACHE_G(debug_mode)) {
		char buffer[1024];
		va_list args;

		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer)-1, format, args);
		va_end(args);
		buffer[sizeof(buffer)-1] = '\0';
		php_printf("%s<br />\n", buffer);
	}
}
/* }}} */

/**
 * New style crc32 hash, compatible with other clients
 */
static unsigned int mmc_hash(char *key, int key_len) { /* {{{ */
	unsigned int crc = ~0;
	int i;

	for (i=0; i<key_len; i++) {
		CRC32(crc, key[i]);
	}

	crc = (~crc >> 16) & 0x7fff;
  	return crc ? crc : 1;
}
/* }}} */

static int mmc_get_pool(zval *id, mmc_pool_t **pool TSRMLS_DC) /* {{{ */
{
	zval **connection;
	int resource_type;

	if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find connection identifier");
		return 0;
	}

	*pool = (mmc_pool_t *) zend_list_find(Z_LVAL_PP(connection), &resource_type);

	if (!*pool || resource_type != le_memcache_pool) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "connection identifier not found");
		return 0;
	}

	return Z_LVAL_PP(connection);
}
/* }}} */

static int _mmc_open(mmc_t *mmc, char **error_string, int *errnum TSRMLS_DC) /* {{{ */
{
	struct timeval tv;
	char *hostname = NULL, *hash_key = NULL, *errstr = NULL;
	int	hostname_len, err = 0;

	/* close open stream */
	if (mmc->stream != NULL) {
		mmc_server_disconnect(mmc TSRMLS_CC);
	}

	tv.tv_sec = mmc->timeout;
	tv.tv_usec = 0;

	hostname = emalloc(strlen(mmc->host) + MAX_LENGTH_OF_LONG + 1 + 1);
	hostname_len = sprintf(hostname, "%s:%d", mmc->host, mmc->port);

	if (mmc->persistent) {
		hash_key = emalloc(sizeof("mmc_open___") - 1 + hostname_len + 1);
		sprintf(hash_key, "mmc_open___%s", hostname);
	}

#if PHP_API_VERSION > 20020918
	mmc->stream = php_stream_xport_create( hostname, hostname_len,
										   ENFORCE_SAFE_MODE | REPORT_ERRORS,
										   STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
										   hash_key, &tv, NULL, &errstr, &err);
#else
	if (mmc->persistent) {
		switch(php_stream_from_persistent_id(hash_key, &(mmc->stream) TSRMLS_CC)) {
			case PHP_STREAM_PERSISTENT_SUCCESS:
				if (php_stream_eof(mmc->stream)) {
					php_stream_pclose(mmc->stream);
					mmc->stream = NULL;
					break;
				}
			case PHP_STREAM_PERSISTENT_FAILURE:
				break;
		}
	}

	if (!mmc->stream) {
		int socktype = SOCK_STREAM;
		mmc->stream = php_stream_sock_open_host(mmc->host, mmc->port, socktype, &tv, hash_key);
	}

#endif

	efree(hostname);
	if (mmc->persistent) {
		efree(hash_key);
	}

	if (!mmc->stream) {
		MMC_DEBUG(("_mmc_open: can't open socket to host"));
		mmc_server_deactivate(mmc TSRMLS_CC);

		if (errstr) {
			if (error_string) {
				*error_string = errstr;
			}
			else {
				efree(errstr);
			}
		}
		if (errnum) {
			*errnum = err;
		}

		return 0;
	}

	php_stream_auto_cleanup(mmc->stream);
	php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &tv);
	php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
	php_stream_set_chunk_size(mmc->stream, MEMCACHE_G(chunk_size));

	mmc->status = MMC_STATUS_CONNECTED;
	return 1;
}
/* }}} */

static int mmc_open(mmc_t *mmc, int force_connect, char **error_string, int *errnum TSRMLS_DC) /* {{{ */
{
	switch (mmc->status) {
		case MMC_STATUS_DISCONNECTED:
			return _mmc_open(mmc, error_string, errnum TSRMLS_CC);

		case MMC_STATUS_CONNECTED:
			return 1;

		case MMC_STATUS_UNKNOWN:
			/* check connection if needed */
			if (force_connect) {
				char *version;
				if ((version = mmc_get_version(mmc TSRMLS_CC)) == NULL && !_mmc_open(mmc, error_string, errnum TSRMLS_CC)) {
					break;
				}
				if (version) {
					efree(version);
				}
				mmc->status = MMC_STATUS_CONNECTED;
			}
			return 1;

		case MMC_STATUS_FAILED:
			/* retry failed server, possibly stale cache should be flushed if connect ok
			 * TODO: use client callback on successful reconnect to allow user to specify behaviour
			 */
			if (mmc->retry_interval >= 0 && (long)time(NULL) >= mmc->failed + mmc->retry_interval) {
				if (_mmc_open(mmc, error_string, errnum TSRMLS_CC) /*&& mmc_flush(mmc TSRMLS_CC) > 0*/) {
					return 1;
				}
				mmc_server_deactivate(mmc TSRMLS_CC);
			}
			break;
	}
	return 0;
}
/* }}} */

static void mmc_server_disconnect(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	if (mmc->stream != NULL) {
		if (mmc->persistent) {
			php_stream_pclose(mmc->stream);
		}
		else {
			php_stream_close(mmc->stream);
		}
		mmc->stream = NULL;
	}
	mmc->status = MMC_STATUS_DISCONNECTED;
}
/* }}} */

static void mmc_server_deactivate(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	mmc_server_disconnect(mmc TSRMLS_CC);
	mmc->status = MMC_STATUS_FAILED;
	mmc->failed = (long)time(NULL);
}
/* }}} */

/**
 * Indicate a server failure
 * @return	bool	True if the server was actually deactivated
 */
static int mmc_server_failure(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	switch (mmc->status) {
		case MMC_STATUS_DISCONNECTED:
			return 0;

		/* attempt reconnect of sockets in unknown state */
		case MMC_STATUS_UNKNOWN:
			mmc->status = MMC_STATUS_DISCONNECTED;
			return 0;
	}
	mmc_server_deactivate(mmc TSRMLS_CC);
	return 1;
}
/* }}} */

static mmc_t *mmc_server_find(mmc_pool_t *pool, char *key, int key_len TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;

	if (pool->num_servers > 1) {
		unsigned int hash = mmc_hash(key, key_len), i;
		mmc = pool->buckets[hash % pool->num_buckets];

		/* perform failover if needed */
		for (i=0; !mmc_open(mmc, 0, NULL, NULL TSRMLS_CC) && (i<20 || i<pool->num_buckets) && MEMCACHE_G(allow_failover); i++) {
			char *next_key = emalloc(key_len + MAX_LENGTH_OF_LONG + 1);
			int next_len = sprintf(next_key, "%d%s", i+1, key);
			MMC_DEBUG(("mmc_server_find: failed to connect to server '%s:%d' status %d, trying next", mmc->host, mmc->port, mmc->status));

			hash += mmc_hash(next_key, next_len);
			mmc = pool->buckets[hash % pool->num_buckets];

			efree(next_key);
		}
	}
	else {
		mmc = pool->servers[0];
		mmc_open(mmc, 0, NULL, NULL TSRMLS_CC);
	}

	return mmc->status != MMC_STATUS_FAILED ? mmc : NULL;
}
/* }}} */

static int mmc_close(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	MMC_DEBUG(("mmc_close: closing connection to server"));
	if (!mmc->persistent) {
		mmc_server_disconnect(mmc TSRMLS_CC);
	}
	return 1;
}
/* }}} */

static int mmc_readline(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	char *buf;

	if (mmc->stream == NULL) {
		MMC_DEBUG(("mmc_readline: socket is already closed"));
		return -1;
	}

	buf = php_stream_gets(mmc->stream, mmc->inbuf, MMC_BUF_SIZE);
	if (buf) {
		MMC_DEBUG(("mmc_readline: read data:"));
		MMC_DEBUG(("mmc_readline:---"));
		MMC_DEBUG(("%s", buf));
		MMC_DEBUG(("mmc_readline:---"));
		return strlen(buf);
	}
	else {
		MMC_DEBUG(("mmc_readline: cannot read a line from the server"));
		return -1;
	}
}
/* }}} */

static char *mmc_get_version(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	char *version_str;
	int len;

	if (mmc_sendcmd(mmc, "version", sizeof("version") - 1 TSRMLS_CC) < 0) {
		return NULL;
	}

	if ((len = mmc_readline(mmc TSRMLS_CC)) < 0) {
		return NULL;
	}

	if (mmc_str_left(mmc->inbuf,"VERSION ", len, sizeof("VERSION ") - 1)) {
		version_str = estrndup(mmc->inbuf + sizeof("VERSION ") - 1, len - (sizeof("VERSION ") - 1) - (sizeof("\r\n") - 1) );
		return version_str;
	}

	MMC_DEBUG(("mmc_get_version: data is not valid version string"));
	return NULL;
}
/* }}} */

static int mmc_str_left(char *haystack, char *needle, int haystack_len, int needle_len) /* {{{ */
{
	char *found;

	found = php_memnstr(haystack, needle, needle_len, haystack + haystack_len);
	if ((found - haystack) == 0) {
		return 1;
	}
	return 0;
}
/* }}} */

static int mmc_sendcmd(mmc_t *mmc, const char *cmd, int cmdlen TSRMLS_DC) /* {{{ */
{
	char *command;
	int command_len;

	if (!mmc || !cmd) {
		return -1;
	}

	MMC_DEBUG(("mmc_sendcmd: sending command '%s'", cmd));

	command = emalloc(cmdlen + sizeof("\r\n"));
	memcpy(command, cmd, cmdlen);
	memcpy(command + cmdlen, "\r\n", sizeof("\r\n") - 1);
	command_len = cmdlen + sizeof("\r\n") - 1;
	command[command_len] = '\0';

	if (php_stream_write(mmc->stream, command, command_len) != command_len) {
		MMC_DEBUG(("mmc_sendcmd: write failed"));
		efree(command);
		return -1;
	}
	efree(command);

	return 1;
}
/* }}}*/

static int mmc_exec_storage_cmd(mmc_t *mmc, char *command, int command_len, char *key, int key_len, int flags, int expire, char *data, int data_len TSRMLS_DC) /* {{{ */
{
	char *real_command;
	int size;
	int response_buf_size;

	real_command = emalloc(
							  command_len
							+ 1				/* space */
							+ key_len
							+ 1				/* space */
							+ MAX_LENGTH_OF_LONG
							+ 1 			/* space */
							+ MAX_LENGTH_OF_LONG
							+ 1 			/* space */
							+ MAX_LENGTH_OF_LONG
							+ sizeof("\r\n") - 1
							+ data_len
							+ sizeof("\r\n") - 1
							+ 1
							);

	size = sprintf(real_command, "%s %s %d %d %d\r\n", command, key, flags, expire, data_len);

	memcpy(real_command + size, data, data_len);
	memcpy(real_command + size + data_len, "\r\n", sizeof("\r\n") - 1);
	size = size + data_len + sizeof("\r\n") - 1;
	real_command[size] = '\0';

	MMC_DEBUG(("mmc_exec_storage_cmd: store cmd is '%s'", real_command));
	MMC_DEBUG(("mmc_exec_storage_cmd: trying to store '%s', %d bytes", data, data_len));

	/* send command & data */
	if (php_stream_write(mmc->stream, real_command, size) != size) {
		MMC_DEBUG(("failed to send command and data to the server"));
		efree(real_command);
		return -1;
	}
	efree(real_command);

	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0) {
		MMC_DEBUG(("failed to read the server's response"));
		return -1;
	}

	MMC_DEBUG(("mmc_exec_storage_cmd: response is '%s'", mmc->inbuf));

	/* stored or not? */
	if(mmc_str_left(mmc->inbuf,"STORED", response_buf_size, sizeof("STORED") - 1)) {
		return 1;
	}

	/* not stored, return FALSE */
	if(mmc_str_left(mmc->inbuf,"NOT_STORED", response_buf_size, sizeof("NOT_STORED") - 1)) {
		return 0;
	}

	/* return FALSE without failover */
	if (mmc_str_left(mmc->inbuf, "SERVER_ERROR out of memory", response_buf_size, sizeof("SERVER_ERROR out of memory") - 1)) {
		return 0;
	}

	MMC_DEBUG(("an error occured while trying to store the item on the server"));
	return -1;
}
/* }}} */

static int mmc_parse_response(char *response, char **item_name, int response_len, int *flags, int *value_len) /* {{{ */
{
	int i=0, n=0;
	int spaces[3];

	if (!response || response_len <= 0) {
		return -1;
	}

	MMC_DEBUG(("mmc_parse_response: got response '%s'", response));

	for (i = 0; i < response_len; i++) {
		if (response[i] == ' ') {
			spaces[n] = i;
			n++;
			if (n == 3) {
				break;
			}
		}
	}

	MMC_DEBUG(("mmc_parse_response: found %d spaces", n));

	if (n < 3) {
		return -1;
	}

	if (item_name != NULL) {
		int item_name_len = spaces[1] - spaces[0] - 1;

		*item_name = emalloc(item_name_len + 1);
		memcpy(*item_name, response + spaces[0] + 1, item_name_len);
		(*item_name)[item_name_len] = '\0';
	}

	*flags = atoi(response + spaces[1]);
	*value_len = atoi(response + spaces[2]);

	if (*flags < 0 || *value_len < 0) {
		return -1;
	}

	MMC_DEBUG(("mmc_parse_response: 1st space is at %d position", spaces[1]));
	MMC_DEBUG(("mmc_parse_response: 2nd space is at %d position", spaces[2]));
	MMC_DEBUG(("mmc_parse_response: flags = %d", *flags));
	MMC_DEBUG(("mmc_parse_response: value_len = %d ", *value_len));

	return 1;
}
/* }}} */

static int mmc_exec_retrieval_cmd(mmc_pool_t *pool, zval *key, zval **return_value TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;
	char *request;
	int result = -1, request_len, response_len;

	MMC_DEBUG(("mmc_exec_retrieval_cmd: key '%s'", Z_STRVAL_P(key)));

	convert_to_string(key);
	MMC_PREPARE_KEY(Z_STRVAL_P(key), Z_STRLEN_P(key));

	/* get + ' ' + key + \0 */
	request = emalloc(sizeof("get") + Z_STRLEN_P(key) + 1);
	request_len = sprintf(request, "get %s", Z_STRVAL_P(key));

	while (result < 0 && (mmc = mmc_server_find(pool, Z_STRVAL_P(key), Z_STRLEN_P(key) TSRMLS_CC)) != NULL) {
		MMC_DEBUG(("mmc_exec_retrieval_cmd: found server '%s:%d' for key '%s'", mmc->host, mmc->port, Z_STRVAL_P(key)));

		/* send command and read value */
		if ((result = mmc_sendcmd(mmc, request, request_len TSRMLS_CC)) > 0 &&
			(result = mmc_read_value(mmc, NULL, return_value TSRMLS_CC)) >= 0) {

			/* not found */
			if (result == 0) {
				ZVAL_FALSE(*return_value);
			}
			/* read "END" */
			else if ((response_len = mmc_readline(mmc TSRMLS_CC)) < 0 || !mmc_str_left(mmc->inbuf, "END", response_len, sizeof("END")-1)) {
				result = -1;
			}
		}

		if (result < 0 && mmc_server_failure(mmc TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", mmc->host, mmc->port);
		}
	}

	efree(request);
	return result;
}
/* }}} */

static int mmc_exec_retrieval_cmd_multi(mmc_pool_t *pool, zval *keys, zval **return_value TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;
	HashPosition pos;
	zval **key, *value;
	char *result_key;
	int	i = 0, j, num_requests, result, result_status;

	array_init(*return_value);

	/* until no retrival errors or all servers have failed */
	do {
		result_status = num_requests = 0;
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);

		/* first pass to build requests for each server */
		while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **) &key, &pos) == SUCCESS) {
			if (Z_TYPE_PP(key) != IS_STRING) {
				SEPARATE_ZVAL(key);
				convert_to_string(*key);
			}

			MMC_PREPARE_KEY(Z_STRVAL_PP(key), Z_STRLEN_PP(key));

			/* schedule key if first round or if missing from result */
			if ((!i || !zend_hash_exists(Z_ARRVAL_PP(return_value), Z_STRVAL_PP(key), Z_STRLEN_PP(key))) &&
				(mmc = mmc_server_find(pool, Z_STRVAL_PP(key), Z_STRLEN_PP(key) TSRMLS_CC)) != NULL) {
				if (!(mmc->outbuf.len)) {
					smart_str_appendl(&(mmc->outbuf), "get", sizeof("get")-1);
					pool->requests[num_requests++] = mmc;
				}

				smart_str_appendl(&(mmc->outbuf), " ", 1);
				smart_str_appendl(&(mmc->outbuf), Z_STRVAL_PP(key), Z_STRLEN_PP(key));
				MMC_DEBUG(("mmc_exec_retrieval_cmd_multi: scheduled key '%s' for '%s:%d' request length '%d'", Z_STRVAL_PP(key), mmc->host, mmc->port, mmc->outbuf.len));
			}

			zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);
		}

		/* second pass to send requests in parallel */
		for (j=0; j<num_requests; j++) {
			smart_str_0(&(pool->requests[j]->outbuf));

			if ((result = mmc_sendcmd(pool->requests[j], pool->requests[j]->outbuf.c, pool->requests[j]->outbuf.len TSRMLS_CC)) < 0) {
				if (mmc_server_failure(pool->requests[j] TSRMLS_CC)) {
					php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->requests[j]->host, pool->requests[j]->port);
				}
				result_status = result;
			}
		}

		/* third pass to read responses */
		for (j=0; j<num_requests; j++) {
			if (pool->requests[j]->status != MMC_STATUS_FAILED) {
				for (value = NULL; (result = mmc_read_value(pool->requests[j], &result_key, &value TSRMLS_CC)) > 0; value = NULL) {
					add_assoc_zval(*return_value, result_key, value);
					efree(result_key);
				}

				/* check for server failure */
				if (result < 0) {
					if (mmc_server_failure(pool->requests[j] TSRMLS_CC)) {
						php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->requests[j]->host, pool->requests[j]->port);
					}
					result_status = result;
				}
			}

			smart_str_free(&(pool->requests[j]->outbuf));
		}
	} while (result_status < 0 && i++ < 20);

	return result_status;
}
/* }}} */

static int mmc_read_value(mmc_t *mmc, char **key, zval **value TSRMLS_DC) /* {{{ */
{
	int response_len, flags, data_len, i, size;
	char *data;

	/* read "VALUE <key> <flags> <bytes>\r\n" header line */
	if ((response_len = mmc_readline(mmc TSRMLS_CC)) < 0) {
		MMC_DEBUG(("failed to read the server's response"));
		return -1;
	}

	/* reached the end of the data */
	if (mmc_str_left(mmc->inbuf, "END", response_len, sizeof("END") - 1)) {
		return 0;
	}

	if (mmc_parse_response(mmc->inbuf, key, response_len, &flags, &data_len) < 0) {
		return -1;
	}

	MMC_DEBUG(("mmc_read_value: data len is %d bytes", data_len));

	/* data_len + \r\n + \0 */
	data = emalloc(data_len + 3);

	for (i=0; i<data_len+2; i+=size) {
		if ((size = php_stream_read(mmc->stream, data + i, data_len + 2 - i)) == 0) {
			MMC_DEBUG(("incomplete data block (expected %d, got %d)", (data_len + 2), i));
			if (key) {
				efree(*key);
			}
			efree(data);
			return -1;
		}
	}

	data[data_len] = '\0';
	if (!data) {
		if (*value == NULL) {
			MAKE_STD_ZVAL(*value);
		}
		ZVAL_EMPTY_STRING(*value);
		efree(data);
		return 1;
	}

	if (flags & MMC_COMPRESSED) {
		char *result_data;
		long result_len = 0;

		if (!mmc_uncompress(&result_data, &result_len, data, data_len)) {
			MMC_DEBUG(("Error when uncompressing data"));
			if (key) {
				efree(*key);
			}
			efree(data);
			return -1;
		}

		efree(data);
		data = result_data;
		data_len = result_len;
	}

	MMC_DEBUG(("mmc_read_value: data '%s'", data));

	if (*value == NULL) {
		MAKE_STD_ZVAL(*value);
	}

	if (flags & MMC_SERIALIZED) {
		const char *tmp = data;
		php_unserialize_data_t var_hash;
		PHP_VAR_UNSERIALIZE_INIT(var_hash);

		if (!php_var_unserialize(value, (const unsigned char **)&tmp, tmp + data_len, &var_hash TSRMLS_CC)) {
			MMC_DEBUG(("Error at offset %d of %d bytes", tmp - data, data_len));
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
			if (key) {
				efree(*key);
			}
			efree(data);
			return -1;
		}

		PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		efree(data);
	}
	else {
		ZVAL_STRINGL(*value, data, data_len, 0);
	}

	return 1;
}
/* }}} */

static int mmc_delete(mmc_t *mmc, char *key, int key_len, int time TSRMLS_DC) /* {{{ */
{
	char *real_command;
	int size, response_buf_size;

	real_command = emalloc(
							  sizeof("delete") - 1
							+ 1						/* space */
							+ key_len
							+ 1						/* space */
							+ MAX_LENGTH_OF_LONG
							+ 1
							);

	size = sprintf(real_command, "delete %s %d", key, time);

	real_command [size] = '\0';

	MMC_DEBUG(("mmc_delete: trying to delete '%s'", key));

	/* drop it! =) */
	if (mmc_sendcmd(mmc, real_command, size TSRMLS_CC) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);

	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0){
		MMC_DEBUG(("failed to read the server's response"));
		return -1;
	}

	MMC_DEBUG(("mmc_delete: server's response is '%s'", mmc->inbuf));

	/* ok? */
	if(mmc_str_left(mmc->inbuf,"DELETED", response_buf_size, sizeof("DELETED") - 1)) {
		return 1;
	}

	if(mmc_str_left(mmc->inbuf,"NOT_FOUND", response_buf_size, sizeof("NOT_FOUND") - 1)) {
		/* return 0, if such wasn't found */
		return 0;
	}

	MMC_DEBUG(("failed to delete item"));

	/* hmm.. */
	return -1;
}
/* }}} */

static int mmc_flush(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	int response_buf_size;

	MMC_DEBUG(("mmc_flush: flushing the cache"));

	if (mmc_sendcmd(mmc, "flush_all", sizeof("flush_all") - 1 TSRMLS_CC) < 0) {
		return -1;
	}

	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0){
		return -1;
	}

	MMC_DEBUG(("mmc_flush: server's response is '%s'", mmc->inbuf));

	/* ok? */
	if(mmc_str_left(mmc->inbuf,"OK", response_buf_size, sizeof("OK") - 1)) {
		return 1;
	}

	MMC_DEBUG(("failed to flush server's cache"));

	/* hmm.. */
	return -1;
}
/* }}} */

static int mmc_get_stats (mmc_t *mmc, zval **stats TSRMLS_DC) /* {{{ */
{
	int response_buf_size, stats_tmp_len, space_len;
	char *stats_tmp, *space_tmp = NULL;
	char *key, *val;

	if ( mmc_sendcmd(mmc, "stats", sizeof("stats") - 1 TSRMLS_CC) < 0) {
		return -1;
	}

	array_init(*stats);

	while ( (response_buf_size = mmc_readline(mmc TSRMLS_CC)) > 0 ) {
		if (mmc_str_left(mmc->inbuf, "STAT", response_buf_size, sizeof("STAT") - 1)) {
			stats_tmp_len = response_buf_size - (sizeof("STAT ") - 1) - (sizeof("\r\n") - 1);
			stats_tmp = estrndup(mmc->inbuf + (sizeof("STAT ") - 1), stats_tmp_len);
			space_tmp = php_memnstr(stats_tmp, " ", 1, stats_tmp + stats_tmp_len);

			if (space_tmp) {
				space_len = strlen(space_tmp);
				key = estrndup(stats_tmp, stats_tmp_len - space_len);
				val = estrndup(stats_tmp + (stats_tmp_len - space_len) + 1, space_len - 1);
				add_assoc_string(*stats, key, val, 1);

				efree(key);
				efree(val);
			}

			efree(stats_tmp);
		}
		else if (mmc_str_left(mmc->inbuf, "END", response_buf_size, sizeof("END") - 1)) {
			/* END of stats*/
			break;
		}
		else {
			/* unknown error */
			return -1;
		}
	}

	if (response_buf_size < 0) {
		return -1;
	}

	return 1;
}
/* }}} */

static int mmc_incr_decr (mmc_t *mmc, int cmd, char *key, int key_len, int value, long *number TSRMLS_DC) /* {{{ */
{
	char *command, *command_name;
	int  cmd_len, response_buf_size;

	/* 4 is for strlen("incr") or strlen("decr"), doesn't matter */
	command = emalloc(4 + key_len + MAX_LENGTH_OF_LONG + 1);

	if (cmd > 0) {
		command_name = emalloc(sizeof("incr"));
		sprintf(command_name, "incr");
		cmd_len = sprintf(command, "incr %s %d", key, value);
	}
	else {
		command_name = emalloc(sizeof("decr"));
		sprintf(command_name, "decr");
		cmd_len = sprintf(command, "decr %s %d", key, value);
	}

	if (mmc_sendcmd(mmc, command, cmd_len TSRMLS_CC) < 0) {
		efree(command);
		efree(command_name);
		return -1;
	}
	efree(command);

	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0) {
		MMC_DEBUG(("failed to read the server's response"));
		efree(command_name);
		return -1;
	}

	MMC_DEBUG(("mmc_incr_decr: server's answer is: '%s'", mmc->inbuf));
	if (mmc_str_left(mmc->inbuf, "NOT_FOUND", response_buf_size, sizeof("NOT_FOUND") - 1)) {
		MMC_DEBUG(("failed to %sement variable - item with such key not found", command_name));
		efree(command_name);
		return 0;
	}
	else if (mmc_str_left(mmc->inbuf, "ERROR", response_buf_size, sizeof("ERROR") - 1)) {
		MMC_DEBUG(("failed to %sement variable - an error occured", command_name));
		efree(command_name);
		return -1;
	}
	else if (mmc_str_left(mmc->inbuf, "CLIENT_ERROR", response_buf_size, sizeof("CLIENT_ERROR") - 1)) {
		MMC_DEBUG(("failed to %sement variable - client error occured", command_name));
		efree(command_name);
		return -1;
	}
	else if (mmc_str_left(mmc->inbuf, "SERVER_ERROR", response_buf_size, sizeof("SERVER_ERROR") - 1)) {
		MMC_DEBUG(("failed to %sement variable - server error occured", command_name));
		efree(command_name);
		return -1;
	}

	efree(command_name);
	*number = (long)atol(mmc->inbuf);
	return 1;
}
/* }}} */

static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *command, int command_len) /* {{{ */
{
	mmc_t *mmc;
	mmc_pool_t *pool;
	int result = -1, value_len, data_len, key_len;
	char *value, *data, *key, *real_key;
	long flags = 0, expire = 0;
	zval *var, *mmc_object = getThis();

	php_serialize_data_t var_hash;
	smart_str buf = {0};

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Osz|ll", &mmc_object, memcache_class_entry_ptr, &key, &key_len, &var, &flags, &expire) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|ll", &key, &key_len, &var, &flags, &expire) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	MMC_PREPARE_KEY(key, key_len);

	if (key_len > MMC_KEY_MAX_SIZE) {
		real_key = estrndup(key, MMC_KEY_MAX_SIZE);
		key_len = MMC_KEY_MAX_SIZE;
	}
	else {
		real_key = estrdup(key);
	}

	switch (Z_TYPE_P(var)) {
		case IS_STRING:
			value = Z_STRVAL_P(var);
			value_len = Z_STRLEN_P(var);
			break;

		case IS_LONG:
		case IS_DOUBLE:
		case IS_BOOL:
			convert_to_string(var);
			value = Z_STRVAL_P(var);
			value_len = Z_STRLEN_P(var);
			break;

		default:
			PHP_VAR_SERIALIZE_INIT(var_hash);
			php_var_serialize(&buf, &var, &var_hash TSRMLS_CC);
			PHP_VAR_SERIALIZE_DESTROY(var_hash);

			if (!buf.c) {
				/* you're trying to save null or something went really wrong */
				RETURN_FALSE;
			}

			value = buf.c;
			value_len = buf.len;
			flags |= MMC_SERIALIZED;
			break;
	}

	/* autocompress large values */
	if (pool->compress_threshold && value_len >= pool->compress_threshold) {
		flags |= MMC_COMPRESSED;
	}

	if (flags & MMC_COMPRESSED) {
		if (!mmc_compress(&data, &data_len, value, value_len TSRMLS_CC)) {
			RETURN_FALSE;
		}

		MMC_DEBUG(("php_mmc_store: compressed '%s' from %d bytes to %d", key, value_len, data_len));

		/* was enough space was saved to motivate uncompress processing on get() */
		if (data_len >= value_len * (1 - pool->min_compress_savings)) {
			efree(data);
			data = value;
			data_len = value_len;
			flags &= ~MMC_COMPRESSED;
			MMC_DEBUG(("php_mmc_store: compression saving were less that '%f', clearing compressed flag", pool->min_compress_savings));
		}
	}
	else {
		data = value;
		data_len = value_len;
	}

	while (result < 0 && (mmc = mmc_server_find(pool, real_key, key_len TSRMLS_CC)) != NULL) {
		if ((result = mmc_exec_storage_cmd(mmc, command, command_len, real_key, key_len, flags, expire, data, data_len TSRMLS_CC)) < 0 && mmc_server_failure(mmc TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", mmc->host, mmc->port);
		}
	}

	if (flags & MMC_SERIALIZED) {
		smart_str_free(&buf);
	}
	if (flags & MMC_COMPRESSED) {
		efree(data);
	}
	efree(real_key);

	if (result > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

static void php_mmc_incr_decr(INTERNAL_FUNCTION_PARAMETERS, int cmd) /* {{{ */
{
	mmc_t *mmc;
	mmc_pool_t *pool;
	int result = -1, key_len;
	long value = 1, number;
	char *key;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|l", &mmc_object, memcache_class_entry_ptr, &key, &key_len, &value) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len, &value) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	MMC_PREPARE_KEY(key, key_len);

	while (result < 0 && (mmc = mmc_server_find(pool, key, key_len TSRMLS_CC)) != NULL) {
		if ((result = mmc_incr_decr(mmc, cmd, key, key_len, value, &number TSRMLS_CC)) < 0 && mmc_server_failure(mmc TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", mmc->host, mmc->port);
		}
	}

	if (result > 0) {
		RETURN_LONG(number);
	}
	RETURN_FALSE;
}
/* }}} */

static void php_mmc_connect (INTERNAL_FUNCTION_PARAMETERS, int persistent) /* {{{ */
{
	zval *mmc_object = getThis();
	mmc_t *mmc = NULL;
	mmc_pool_t *pool;
	int errnum = 0, host_len;
	char *host, *error_string = NULL;
	long port = MEMCACHE_G(default_port), timeout = MMC_DEFAULT_TIMEOUT;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &host, &host_len, &port, &timeout) == FAILURE) {
		return;
	}

	/* initialize and connect server struct */
	if (persistent) {
		mmc = mmc_find_persistent(host, host_len, port, timeout, MMC_DEFAULT_RETRY TSRMLS_CC);
	}
	else {
		MMC_DEBUG(("php_mmc_connect: creating regular connection"));
		mmc = mmc_server_new(host, host_len, port, 0, timeout, MMC_DEFAULT_RETRY TSRMLS_CC);
	}

	if (!mmc_open(mmc, 1, &error_string, &errnum TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't connect to %s:%ld, %s (%d)", host, port, error_string ? error_string : "Unknown error", errnum);
		if (error_string) {
			efree(error_string);
		}
		RETURN_FALSE;
	}

	/* initialize pool */
	MMC_DEBUG(("php_mmc_connect: initializing server pool"));
	pool = mmc_pool_new();
	pool->id = zend_list_insert(pool, le_memcache_pool);
	mmc_pool_add(pool, mmc, 1);

	if (mmc_object == NULL) {
		object_init_ex(return_value, memcache_class_entry_ptr);
		add_property_resource(return_value, "connection", pool->id);
	}
	else {
		add_property_resource(mmc_object, "connection", pool->id);
		RETURN_TRUE;
	}
}
/* }}} */

/* ----------------
   module functions
   ---------------- */

/* {{{ proto object memcache_connect( string host [, int port [, int timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_connect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto object memcache_pconnect( string host [, int port [, int timeout ] ])
   Connects to server and returns a Memcache object */
PHP_FUNCTION(memcache_pconnect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto bool memcache_add_server( string host [, int port [, bool persistent [, int weight [, int timeout [, int retry_interval ] ] ] ] ])
   Adds a connection to the pool. The order in which this function is called is significant */
PHP_FUNCTION(memcache_add_server)
{
	zval **connection, *mmc_object = getThis();
	mmc_pool_t *pool;
	mmc_t *mmc;
	long port = MEMCACHE_G(default_port), weight = 1, timeout = MMC_DEFAULT_TIMEOUT, retry_interval = MMC_DEFAULT_RETRY;
	zend_bool persistent = 1;
	int resource_type, host_len;
	char *host;

	if (mmc_object) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lblll", &host, &host_len, &port, &persistent, &weight, &timeout, &retry_interval) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|lblll", &mmc_object, memcache_class_entry_ptr, &host, &host_len, &port, &persistent, &weight, &timeout, &retry_interval) == FAILURE) {
			return;
		}
	}

	if (weight < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "weight must be a positive integer");
		RETURN_FALSE;
	}

	/* lazy initialization of server struct */
	if (persistent) {
		mmc = mmc_find_persistent(host, host_len, port, timeout, retry_interval TSRMLS_CC);
	}
	else {
		MMC_DEBUG(("memcache_add_server: initializing regular struct"));
		mmc = mmc_server_new(host, host_len, port, 0, timeout, retry_interval TSRMLS_CC);
	}

	/* initialize pool if need be */
	if (zend_hash_find(Z_OBJPROP_P(mmc_object), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		MMC_DEBUG(("mmc_add_connection: initialized and appended server to empty pool"));

		pool = mmc_pool_new();
		pool->id = zend_list_insert(pool, le_memcache_pool);

		add_property_resource(mmc_object, "connection", pool->id);
	}
	else {
		MMC_DEBUG(("memcache_add_server: appended server to pool"));

		pool = (mmc_pool_t *) zend_list_find(Z_LVAL_PP(connection), &resource_type);
		if (!pool || resource_type != le_memcache_pool) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "connection identifier not found");
			RETURN_FALSE;
		}
	}

	mmc_pool_add(pool, mmc, weight);
	RETURN_TRUE;
}
/* }}} */

static mmc_t *mmc_find_persistent(char *host, int host_len, int port, int timeout, int retry_interval TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc;
	list_entry *le;
	char *hash_key;
	int hash_key_len;

	MMC_DEBUG(("mmc_find_persistent: seeking for persistent connection"));
	hash_key = emalloc(sizeof("mmc_connect___") - 1 + host_len + MAX_LENGTH_OF_LONG + 1);
	hash_key_len = sprintf(hash_key, "mmc_connect___%s:%d", host, port);

	if (zend_hash_find(&EG(persistent_list), hash_key, hash_key_len+1, (void **) &le) == FAILURE) {
		list_entry new_le;
		MMC_DEBUG(("mmc_find_persistent: connection wasn't found in the hash"));

		mmc = mmc_server_new(host, host_len, port, 1, timeout, retry_interval TSRMLS_CC);
		new_le.type = le_pmemcache;
		new_le.ptr  = mmc;

		/* register new persistent connection */
		if (zend_hash_update(&EG(persistent_list), hash_key, hash_key_len+1, (void *) &new_le, sizeof(list_entry), NULL) == FAILURE) {
			mmc_server_free(mmc TSRMLS_CC);
			mmc = NULL;
		} else {
			zend_list_insert(mmc, le_pmemcache);
		}
	}
	else if (le->type != le_pmemcache || le->ptr == NULL) {
		list_entry new_le;
		MMC_DEBUG(("mmc_find_persistent: something was wrong, reconnecting.."));
		zend_hash_del(&EG(persistent_list), hash_key, hash_key_len+1);

		mmc = mmc_server_new(host, host_len, port, 1, timeout, retry_interval TSRMLS_CC);
		new_le.type = le_pmemcache;
		new_le.ptr  = mmc;

		/* register new persistent connection */
		if (zend_hash_update(&EG(persistent_list), hash_key, hash_key_len+1, (void *) &new_le, sizeof(list_entry), NULL) == FAILURE) {
			mmc_server_free(mmc TSRMLS_CC);
			mmc = NULL;
		}
		else {
			zend_list_insert(mmc, le_pmemcache);
		}
	}
	else {
		MMC_DEBUG(("mmc_find_persistent: connection found in the hash"));
		mmc = (mmc_t *)le->ptr;
		mmc->timeout = timeout;
		mmc->retry_interval = retry_interval;

		/* attempt to reconnect this node before failover in case connection has gone away */
		if (mmc->status == MMC_STATUS_CONNECTED) {
			mmc->status = MMC_STATUS_UNKNOWN;
		}
	}

	efree(hash_key);
	return mmc;
}
/* }}} */

/* {{{ proto string memcache_get_version( object memcache )
   Returns server's version */
PHP_FUNCTION(memcache_get_version)
{
	mmc_pool_t *pool;
	char *version;
	int i;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_class_entry_ptr) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (mmc_open(pool->servers[i], 1, NULL, NULL TSRMLS_CC) && (version = mmc_get_version(pool->servers[i] TSRMLS_CC)) != NULL) {
			RETURN_STRING(version, 0);
		}
		else if (mmc_server_failure(pool->servers[i] TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->servers[i]->host, pool->servers[i]->port);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_add( object memcache, string key, mixed var [, int flag [, int expire ] ] )
   Adds new item. Item with such key should not exist. */
PHP_FUNCTION(memcache_add)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "add", sizeof("add") - 1);
}
/* }}} */

/* {{{ proto bool memcache_set( object memcache, string key, mixed var [, int flag [, int expire ] ] )
   Sets the value of an item. Item may exist or not */
PHP_FUNCTION(memcache_set)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "set", sizeof("set") - 1);
}
/* }}} */

/* {{{ proto bool memcache_replace( object memcache, string key, mixed var [, int flag [, int expire ] ] )
   Replaces existing item. Returns false if item doesn't exist */
PHP_FUNCTION(memcache_replace)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "replace", sizeof("replace") - 1);
}
/* }}} */

/* {{{ proto mixed memcache_get( object memcache, mixed key )
   Returns value of existing item or false */
PHP_FUNCTION(memcache_get)
{
	mmc_pool_t *pool;
	zval *key, *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oz", &mmc_object, memcache_class_entry_ptr, &key) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(key) != IS_ARRAY) {
		if (mmc_exec_retrieval_cmd(pool, key, &return_value TSRMLS_CC) < 0) {
			RETURN_FALSE;
		}
	}
	else {
		if (mmc_exec_retrieval_cmd_multi(pool, key, &return_value TSRMLS_CC) < 0) {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto bool memcache_delete( object memcache, string key [, int expire ])
   Deletes existing item */
PHP_FUNCTION(memcache_delete)
{
	mmc_t *mmc;
	mmc_pool_t *pool;
	int result = -1, key_len;
	zval *mmc_object = getThis();
	char *key;
	long time = 0;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|l", &mmc_object, memcache_class_entry_ptr, &key, &key_len, &time) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len, &time) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC) || !pool->num_servers) {
		RETURN_FALSE;
	}

	MMC_PREPARE_KEY(key, key_len);

	while (result < 0 && (mmc = mmc_server_find(pool, key, key_len TSRMLS_CC)) != NULL) {
		if ((result = mmc_delete(mmc, key, key_len, time TSRMLS_CC)) < 0 && mmc_server_failure(mmc TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", mmc->host, mmc->port);
		}
	}

	if (result > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_debug( bool onoff )
   Turns on/off internal debugging */
PHP_FUNCTION(memcache_debug)
{
#if ZEND_DEBUG
	zend_bool onoff;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &onoff) == FAILURE) {
		return;
	}

	MEMCACHE_G(debug_mode) = onoff ? 1 : 0;

	RETURN_TRUE;
#else
	RETURN_FALSE;
#endif

}
/* }}} */

/* {{{ proto array memcache_get_stats( object memcache )
   Returns server's statistics */
PHP_FUNCTION(memcache_get_stats)
{
	mmc_pool_t *pool;
	int i, failures = 0;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_class_entry_ptr) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!mmc_open(pool->servers[i], 1, NULL, NULL TSRMLS_CC) || mmc_get_stats(pool->servers[i], &return_value TSRMLS_CC) < 0) {
			if (mmc_server_failure(pool->servers[i] TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->servers[i]->host, pool->servers[i]->port);
			}
			failures++;
		}
		else {
			break;
		}
	}

	if (failures >= pool->num_servers) {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array memcache_get_extended_stats( object memcache )
   Returns statistics for each server in the pool */
PHP_FUNCTION(memcache_get_extended_stats)
{
	mmc_pool_t *pool;
	char *hostname;
	int i, hostname_len;
	zval *mmc_object = getThis(), *stats;

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_class_entry_ptr) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	array_init(return_value);
	for (i=0; i<pool->num_servers; i++) {
		MAKE_STD_ZVAL(stats);

		hostname = emalloc(strlen(pool->servers[i]->host) + MAX_LENGTH_OF_LONG + 1 + 1);
		hostname_len = sprintf(hostname, "%s:%d", pool->servers[i]->host, pool->servers[i]->port);

		if (!mmc_open(pool->servers[i], 1, NULL, NULL TSRMLS_CC) || mmc_get_stats(pool->servers[i], &stats TSRMLS_CC) < 0) {
			if (mmc_server_failure(pool->servers[i] TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->servers[i]->host, pool->servers[i]->port);
			}
			ZVAL_FALSE(stats);
		}

		add_assoc_zval(return_value, hostname, stats);
		efree(hostname);
	}
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
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Ol|d", &mmc_object, memcache_class_entry_ptr, &threshold, &min_savings) == FAILURE) {
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

/* {{{ proto int memcache_increment( object memcache, string key [, int value ] )
   Increments existing variable */
PHP_FUNCTION(memcache_increment)
{
	php_mmc_incr_decr(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto int memcache_decrement( object memcache, string key [, int value ] )
   Decrements existing variable */
PHP_FUNCTION(memcache_decrement)
{
	php_mmc_incr_decr(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto bool memcache_close( object memcache )
   Closes connection to memcached */
PHP_FUNCTION(memcache_close)
{
	mmc_pool_t *pool;
	int i, failures = 0;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_class_entry_ptr) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (mmc_close(pool->servers[i] TSRMLS_CC) < 0) {
			if (mmc_server_failure(pool->servers[i] TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->servers[i]->host, pool->servers[i]->port);
			}
			failures++;
		}
	}

	if (failures && failures >= pool->num_servers) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memcache_flush( object memcache )
   Flushes cache */
PHP_FUNCTION(memcache_flush)
{
	mmc_pool_t *pool;
	int i, failures = 0;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &mmc_object, memcache_class_entry_ptr) == FAILURE) {
			return;
		}
	}

	if (!mmc_get_pool(mmc_object, &pool TSRMLS_CC)) {
		RETURN_FALSE;
	}

	for (i=0; i<pool->num_servers; i++) {
		if (!mmc_open(pool->servers[i], 1, NULL, NULL TSRMLS_CC) || mmc_flush(pool->servers[i] TSRMLS_CC) < 0) {
			if (mmc_server_failure(pool->servers[i] TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "marked server '%s:%d' as failed", pool->servers[i]->host, pool->servers[i]->port);
			}
			failures++;
		}
	}

	if (failures && failures >= pool->num_servers) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

#endif /* HAVE_MEMCACHE */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
