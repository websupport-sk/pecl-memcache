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

/*
 * XXX fix all error messages
 */

/* TODO
 * - should we add compression support ? bzip || gzip ?
 * - increment/decrement funcs
 * - stats
 * - more and better error messages
 * - we should probably do a cleanup if some call failed, 
 *   because it can cause further calls to fail
 * */

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

#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "php_network.h"
#include "php_memcache.h"

/* True global resources - no need for thread safety here */
static int le_memcache;
static zend_class_entry *memcache_class_entry_ptr;

#ifdef ZTS
int memcache_globals_id;
#else
PHP_MEMCACHE_API php_memcache_globals memcache_globals;
#endif

/* {{{ memcache_functions[]
 */
zend_function_entry memcache_functions[] = {
	PHP_FE(memcache_connect,		NULL)
	PHP_FE(memcache_get_version,	NULL)
	PHP_FE(memcache_add,			NULL)
	PHP_FE(memcache_set,			NULL)
	PHP_FE(memcache_replace,		NULL)
	PHP_FE(memcache_get,			NULL)
	PHP_FE(memcache_delete,			NULL)
	PHP_FE(memcache_debug,			NULL)
	PHP_FE(memcache_get_stats,		NULL)
	PHP_FE(memcache_increment,		NULL)
	PHP_FE(memcache_decrement,		NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_class_functions[] = {
	PHP_FALIAS(connect,			memcache_connect,			NULL)
	PHP_FALIAS(getversion,		memcache_get_version,		NULL)
	PHP_FALIAS(add,				memcache_add,				NULL)
	PHP_FALIAS(set,				memcache_set,				NULL)
	PHP_FALIAS(replace,			memcache_replace,			NULL)
	PHP_FALIAS(get,				memcache_get,				NULL)
	PHP_FALIAS(delete,			memcache_delete,			NULL)
	PHP_FALIAS(getstats,		memcache_get_stats,			NULL)	
	PHP_FALIAS(increment,		memcache_increment,			NULL)	
	PHP_FALIAS(decrement,		memcache_decrement,			NULL)	
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
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHE
ZEND_GET_MODULE(memcache)
#endif

/* {{{ macros */
#define PREPARE_KEY(key, key_len) \
	php_strtr(key, key_len, "\t\r\n ", "____", 4); \

/* }}} */

/* {{{ internal function protos */
static void _mmc_server_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static int mmc_get_connection(zval *, mmc_t ** TSRMLS_DC);
static mmc_t* mmc_open(const char *, short, long TSRMLS_DC);
static int mmc_close(mmc_t *);
static int mmc_write(mmc_t *, const char *, int);
static int mmc_readline(mmc_t *);
static int mmc_read(mmc_t *, char *, int);
static char * mmc_get_version(mmc_t *);
static int mmc_str_left(char *, char *, int, int);
static int mmc_sendcmd(mmc_t *, const char *, int);
static int mmc_exec_storage_cmd(mmc_t *, char *, char *, int, int, char *, int);
static int mmc_parse_response(char *, int *, int *);
static int mmc_exec_retrieval_cmd(mmc_t *, char *, char *, int *, char **, int *);
static int mmc_delete(mmc_t *, char *, int);
static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *);
static int mmc_get_stats (mmc_t *, zval **);
static int mmc_incr_decr (mmc_t *, int, char *, int);
static void php_mmc_incr_decr (INTERNAL_FUNCTION_PARAMETERS, int);
/* }}} */

/* {{{ php_memcache_init_globals()
*/
static void php_memcache_init_globals(php_memcache_globals *memcache_globals_p TSRMLS_DC)
{ 
	MEMCACHE_G(debug_mode)	= 0;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memcache)
{
	zend_class_entry memcache_class_entry;	
	INIT_CLASS_ENTRY(memcache_class_entry, "Memcache", php_memcache_class_functions);
	memcache_class_entry_ptr = zend_register_internal_class(&memcache_class_entry TSRMLS_CC);	

	le_memcache = zend_register_list_destructors_ex(_mmc_server_list_dtor, NULL, "memcache connection", module_number);

#ifdef ZTS
	ts_allocate_id(&memcache_globals_id, sizeof(php_memcache_globals), (ts_allocate_ctor) php_memcache_init_globals, NULL);
#else
	php_memcache_init_globals(&memcache_globals TSRMLS_CC);
#endif
	
	REGISTER_LONG_CONSTANT("MEMCACHE_SERIALIZED",MMC_SERIALIZED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MEMCACHE_COMPRESSED",MMC_COMPRESSED, CONST_CS | CONST_PERSISTENT);
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(memcache)
{
	MEMCACHE_G(debug_mode) = 0;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(memcache)
{
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
	php_info_print_table_start();
	php_info_print_table_header(2, "memcache support", "enabled");
	php_info_print_table_header(2, "Revision", "$Revision$");
	php_info_print_table_end();
}
/* }}} */

/* {{{ _mmc_server_list_dtor() */
static void _mmc_server_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	mmc_t *mmc = (mmc_t *)rsrc->ptr;
	mmc_close(mmc);
}
/* }}} */

/* {{{ mmc_debug() */
static void mmc_debug( const char *format, ...)
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

/* {{{ mmc_get_connection()
*/
static int mmc_get_connection(zval *id, mmc_t **mmc TSRMLS_DC)
{
	zval	**connection;
	int		resource_type;

	if (zend_hash_find(Z_OBJPROP_P(id), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find connection identifier");
		return 0;
	}

	*mmc = (mmc_t *) zend_list_find(Z_LVAL_PP(connection), &resource_type);
	
	if (!*mmc || resource_type != le_memcache) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "connection identifier not found");
		return 0;
	}
	
	return Z_LVAL_PP(connection);
}
/* }}} */

/* {{{ mmc_open() */
static mmc_t* mmc_open(const char *host, short port, long timeout TSRMLS_DC) 
{
	mmc_t			*mmc;
	struct timeval	tv;

	if ( !(mmc = emalloc( sizeof(mmc_t) )) ) {
		return NULL;
	}

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	
	mmc->stream = php_stream_sock_open_host( host, (unsigned short) port, SOCK_STREAM, 0, 0);
	if (mmc->stream == NULL) {
		mmc_debug("mmc_open: can't open socket to host");
		efree(mmc);
		return NULL;
	}
	
    php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &tv);
	php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
		
	mmc->timeout = timeout;
	
	if (mmc_get_version(mmc) == NULL) {
		mmc_debug("mmc_open: failed to get server's version");
		mmc_close(mmc);
		efree(mmc);
		return NULL;
	}

	mmc->id = zend_list_insert(mmc,le_memcache);

	return mmc;
}
/* }}} */

/* {{{ mmc_close() */
static int mmc_close(mmc_t *mmc) 
{
	mmc_debug("mmc_close: closing connection to server");

	if (mmc == NULL) {
		return 0;
	}

	if (mmc->stream != NULL) {
		mmc_sendcmd(mmc,"quit", 4);
		php_stream_close(mmc->stream);
	}

	return 1;
}
/* }}} */

/* {{{ mmc_write() */
static int mmc_write(mmc_t *mmc, const char *buf, int len) 
{
	int size;
	
	mmc_debug("mmc_write: sending to server:");
	mmc_debug("mmc_write:---");
	mmc_debug("%s", buf);
	mmc_debug("mmc_write:---");

	size = php_stream_write(mmc->stream, buf, len);
	if (size != len) {
		mmc_debug("mmc_write: sent length is less, than data length");
		return -1;
	}
	return len;
}
/* }}} */

/* {{{ mmc_readline() */
static int mmc_readline(mmc_t *mmc) 
{
	char *buf;
	buf = php_stream_gets(mmc->stream, mmc->inbuf, MMC_BUF_SIZE);
	if (buf) {
		mmc_debug("mmc_readline: read data:");
		mmc_debug("mmc_readline:---");
		mmc_debug("%s", buf);
		mmc_debug("mmc_readline:---");
		return strlen(buf);
	}
	else {
		mmc_debug("mmc_readline: cannot read a line from server");
		return -1;
	}
}
/* }}} */

/* {{{ mmc_read() */
static int mmc_read(mmc_t *mmc, char *buf, int len) 
{
	int size;

	mmc_debug("mmc_read: reading data from server");
	size = php_stream_read(mmc->stream, buf, len);
	buf[size] = '\0';
	
	mmc_debug("mmc_read: read %d bytes", size);
	mmc_debug("mmc_read:---");
	mmc_debug("%s", buf);
	mmc_debug("mmc_read:---");
	
	return size;	
}
/* }}} */

/* {{{ mmc_get_version() */
static char *mmc_get_version(mmc_t *mmc) 
{
	char *version_str;
	int len;
	
	if (mmc_sendcmd(mmc, "version", 7) < 0) {
		return NULL;
	}

	if ((len = mmc_readline(mmc)) < 0) {
		return NULL;
	}

	if (mmc_str_left(mmc->inbuf,"VERSION ", len, 8)) {
		version_str = estrndup(mmc->inbuf + 8, strlen(mmc->inbuf) - 8 - 2);
		return version_str;
	}
	
	mmc_debug("mmc_get_version: data is not valid version string");	
	return NULL;
}
/* }}} */

/* {{{ mmc_str_left() */
static int mmc_str_left(char *haystack, char *needle, int haystack_len, int needle_len) 
{
	char *found;
	
	found = php_memnstr(haystack, needle, needle_len, haystack + haystack_len);
	if ((found - haystack) == 0) {
		return 1;
	}
	return 0;
}
/* }}} */

/* {{{ mmc_sendcmd ()*/
static int mmc_sendcmd(mmc_t *mmc, const char *cmd, int cmdlen)
{
	if (!mmc || !cmd) {
		return -1;
	}

	mmc_debug("mmc_sendcmd: sending command '%s'", cmd);

	if (mmc_write(mmc, cmd, cmdlen) != cmdlen) {
		mmc_debug("mmc_sendcmd: write failed");
		return -1;
	}

	if (mmc_write(mmc, "\r\n", 2) != 2) {
		mmc_debug("mmc_sendcmd: write failed");
		return -1;
	}
	
	return 1;
}
/* }}}*/

/* {{{ mmc_exec_storage_cmd () */
static int mmc_exec_storage_cmd(mmc_t *mmc, char *command, char *key, int flags, int expire, char *data, int data_len) 
{
	char *real_command, *real_data;
	char *flags_tmp, *expire_tmp, *datalen_tmp;
	int size, flags_size, expire_size, datalen_size = 0;
	int response_buf_size;
	
	flags_tmp = emalloc(MAX_LENGTH_OF_LONG + 1);
	flags_size = sprintf(flags_tmp, "%d", flags);
	flags_tmp[flags_size] = '\0';
	
	expire_tmp = emalloc(MAX_LENGTH_OF_LONG + 1);
	expire_size = sprintf(expire_tmp, "%d", expire);
	expire_tmp[expire_size] = '\0';

	datalen_tmp = emalloc(MAX_LENGTH_OF_LONG + 1);
	datalen_size = sprintf(datalen_tmp, "%d", data_len);
	datalen_tmp[datalen_size] = '\0';

	real_command = emalloc(
							  strlen(command) 
							+ 1				/* space */ 
							+ strlen(key) 
							+ 1				/* space */ 
							+ flags_size
							+ 1 			/* space */
							+ expire_size 
							+ 1 			/* space */
							+ datalen_size 
							+ 1
							);

	PREPARE_KEY(key, strlen(key));

	size = sprintf(real_command, "%s ", command);
	size += sprintf(real_command + size, "%s ", key);
	size += sprintf(real_command + size, "%s ", flags_tmp);
	size += sprintf(real_command + size, "%s ", expire_tmp);
	size += sprintf(real_command + size, "%s", datalen_tmp);

	real_command [size] = '\0';
	efree(flags_tmp);
	efree(expire_tmp);
	efree(datalen_tmp);

	mmc_debug("mmc_exec_storage_cmd: store cmd is '%s'", real_command);
	
	/* tell server, that we're going to store smthng */
	if (mmc_sendcmd(mmc, real_command, size) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);
	
	real_data = emalloc (data_len + 1);
	
	if (data_len > 0) {
		size = sprintf(real_data, "%s", data);
	}
	else {
		size = 0;		
	}
	
	real_data[size] = '\0';

	mmc_debug("mmc_exec_storage_cmd: sending data to server", real_data);

	/* send data */
	if (mmc_sendcmd(mmc, real_data, size) < 0) {
		efree(real_data);
		return -1;
	}
	efree(real_data);
	
	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc)) < 0) {
		return -1;
	}

	mmc_debug("mmc_exec_storage_cmd: response is '%s'", mmc->inbuf);

	/* stored or not? */
	if(mmc_str_left(mmc->inbuf,"STORED", response_buf_size, 6)) {
		return 1;
	}

	return -1;
}
/* }}} */

/* {{{ mmc_parse_response ()*/
static int mmc_parse_response(char *response, int *flags, int *value_len) 
{
	int i=0, n=0;
	int spaces[3];
	int response_len;

	if (!response || (response_len = strlen(response)) <= 0) {
		return -1;
	}

	mmc_debug("mmc_parse_response: got response '%s'", response);
	
	for (i = 0; i < response_len; i++) {
		if (response[i] == ' ') {
			spaces[n] = i;
			n++;
			if (n == 3) {
				break;
			}
		}
	}

	mmc_debug("mmc_parse_response: found %d spaces", n);

	if (n < 3) {
		return -1;
	}

	*flags = atoi(response + spaces[1]);
	*value_len = atoi(response + spaces[2]);

	mmc_debug("mmc_parse_response: 1st space is at %d position", spaces[1]);
	mmc_debug("mmc_parse_response: 2nd space is at %d position", spaces[2]);
	mmc_debug("mmc_parse_response: flags = %d", *flags);
	mmc_debug("mmc_parse_response: value_len = %d ", *value_len);
	
	return 1;
}

/* }}} */

/* {{{ mmc_exec_retrieval_cmd () */
static int mmc_exec_retrieval_cmd(mmc_t *mmc, char *command, char *key, int *flags, char **data, int *data_len) 
{
	char *real_command, *tmp;
	int size, response_buf_size;
	
	real_command = emalloc(
							  strlen(command) 
							+ 1				/* space */ 
							+ strlen(key) 
							+ 1
							);

	PREPARE_KEY(key, strlen(key));

	size = sprintf(real_command, "%s ", command);
	size += sprintf(real_command + size, "%s", key);

	real_command [size] = '\0';

	mmc_debug("mmc_exec_retrieval_cmd: trying to get '%s'", key);
	
	/* gimme! =) */
	if (mmc_sendcmd(mmc, real_command, size) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);
	
	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc)) < 0){
		return -1;
	}

	mmc_debug("mmc_exec_retrieval_cmd: got response '%s'", mmc->inbuf);
	
	/* what's this? */
	if(mmc_str_left(mmc->inbuf,"VALUE", response_buf_size, 5) < 0) {
		return -1;
	}
	
	tmp = estrdup(mmc->inbuf);
	if (mmc_parse_response(tmp, flags, data_len) < 0) {
		efree(tmp);
		return -1;
	}
	efree(tmp);

	mmc_debug("mmc_exec_retrieval_cmd: data len is %d bytes", *data_len);
	
	if (*data_len) {
		*data = emalloc(*data_len);
		
		if ((size = mmc_read(mmc, *data, *data_len)) != *data_len) {
			return -1;
		}
		mmc_debug("mmc_exec_retrieval_cmd: data '%s'", *data);
	}
	
	/* clean those stupid \r\n's */
	mmc_readline(mmc);
	
	/* read "END" */
	if ((response_buf_size = mmc_readline(mmc)) < 0) {
		return -1;
	}

	if(mmc_str_left(mmc->inbuf,"END", response_buf_size, 3) < 0) {
		return -1;
	}
	
	return 1;
}
/* }}} */

/* {{{ mmc_delete () */
static int mmc_delete(mmc_t *mmc, char *key, int time) 
{
	char *real_command, *time_tmp;
	int size, response_buf_size;
	
	time_tmp = emalloc(MAX_LENGTH_OF_LONG + 1);
	size = sprintf(time_tmp, "%d", time);
	time_tmp[size] = '\0';
	
	real_command = emalloc(
							  6				/* strlen(delete) */ 
							+ 1				/* space */ 
							+ strlen(key)
							+ 1				/* space */ 
							+ size
							+ 1
							);

	PREPARE_KEY(key, strlen(key));
	size = sprintf(real_command, "%s ", "delete");
	size += sprintf(real_command + size, "%s ", key);
	size += sprintf(real_command + size, "%s", time_tmp);
	
	real_command [size] = '\0';

	efree(time_tmp);

	mmc_debug("mmc_delete: trying to delete '%s'", key);
	
	/* drop it! =) */
	if (mmc_sendcmd(mmc, real_command, size) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);
	
	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc)) < 0){
		return -1;
	}
	
	mmc_debug("mmc_delete: server's response is '%s'", mmc->inbuf);

	/* ok? */
	if(mmc_str_left(mmc->inbuf,"DELETED", response_buf_size, 7)) {
		return 1;
	}
	
	if(mmc_str_left(mmc->inbuf,"NOT_FOUND", response_buf_size, 9)) {
		/* return 0, if such wasn't found */
		return 0;
	}
	
	/* hmm.. */
	return -1;
}
/* }}} */

/* {{{ php_mmc_store ()*/
static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *command) 
{
	zval *id, **key, **var, **expire;
	mmc_t *mmc;
	int inx, flags, real_expire, data_len;
	char *data, *real_key;
	int ac = ZEND_NUM_ARGS();

    php_serialize_data_t var_hash;
    smart_str buf = {0};

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}
		if (ac < 2 || ac > 3 || zend_get_parameters_ex(ac, &key, &var, &expire) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	
		convert_to_string_ex(key);

		if (Z_STRLEN_PP(key) > MMC_KEY_MAX_SIZE) {
			real_key = estrndup(Z_STRVAL_PP(key), MMC_KEY_MAX_SIZE);
		}
		else {
			real_key = estrdup(Z_STRVAL_PP(key));
		}
		
		/* default flags & expire */
		flags = 0;
		real_expire = 0;

		if (ac > 2) {
			convert_to_long_ex(expire);
			real_expire = Z_LVAL_PP(expire);
		}
		
		switch (Z_TYPE_PP(var)) {
			case IS_STRING:
			case IS_LONG:
			case IS_DOUBLE:
			case IS_BOOL:
				convert_to_string_ex(var);
				data = Z_STRVAL_PP(var);
				data_len = Z_STRLEN_PP(var);
				break;
			default:
				PHP_VAR_SERIALIZE_INIT(var_hash);
				php_var_serialize(&buf, var, &var_hash TSRMLS_CC);
				PHP_VAR_SERIALIZE_DESTROY(var_hash);
				
				if (!buf.c) {
					/* you're trying to save null or smthing went really wrong */
					RETURN_FALSE;
				}
				
				flags |= MMC_SERIALIZED;

				data = buf.c;
				data_len = buf.len;
				break;
		}

		if (mmc_exec_storage_cmd(mmc, command, real_key, flags, real_expire, data, data_len) > 0) {
			efree(real_key);
			RETURN_TRUE;
		}
		efree(real_key);
		
		RETURN_FALSE;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "memcache_%s() should not be called like this. Use $memcache->%s() to %s item", command, command, command);
	RETURN_FALSE;
}
/* }}} */

/* {{{ mmc_get_stats ()*/
static int mmc_get_stats (mmc_t *mmc, zval **stats)
{
	int response_buf_size, stats_tmp_len, space_len, i = 0;
	char *stats_tmp, *space_tmp = NULL;
	char *key, *val;

	if ( mmc_sendcmd(mmc, "stats", 5) < 0) {
		return -1;
	}

	array_init(*stats);

	while ( (response_buf_size = mmc_readline(mmc)) > 0 ) {
		if (mmc_str_left(mmc->inbuf, "STAT", response_buf_size, 4)) {
			stats_tmp_len = response_buf_size - 5 - 2;
			stats_tmp = estrndup(mmc->inbuf + 5, stats_tmp_len);
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
			i++;
		}
		else {
			/* END of stats or some error */
			break;
		}
	}

	if (i == 0) {
		efree(*stats);
	}

	return 1;
}
/* }}} */

/* {{{ mmc_incr_decr () */
static int mmc_incr_decr (mmc_t *mmc, int cmd, char *key, int value) 
{
	char *command;
	int  cmd_len, response_buf_size;

	command = emalloc(4 + strlen(key) + MAX_LENGTH_OF_LONG + 1);

	PREPARE_KEY(key, strlen(key));
	
	if (cmd > 0) {
		cmd_len = sprintf(command, "incr %s %d", key, value);
	}
	else {
		cmd_len = sprintf(command, "decr %s %d", key, value);
	}
	
	if (mmc_sendcmd(mmc, command, cmd_len) < 0) {
		efree(command);
		return -1;
	}
	efree(command);

	if ((response_buf_size = mmc_readline(mmc)) > 0) {
		mmc_debug("mmc_incr_decr: server's answer is: '%s'", mmc->inbuf);
		if (mmc_str_left(mmc->inbuf, "NOT_FOUND", response_buf_size, 9)) {
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "ERROR", response_buf_size, 5)) {
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "CLIENT_ERROR", response_buf_size, 12)) {
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "SERVER_ERROR", response_buf_size, 12)) {
			return -1;
		}
		else {
			return atoi(mmc->inbuf);
		}
	}
	mmc_debug("mmc_incr_decr: failed to read line from server");
	return -1;

}
/* }}} */

/* {{{ php_mmc_incr_decr () */
static void php_mmc_incr_decr(INTERNAL_FUNCTION_PARAMETERS, int cmd)
{
	zval *id, **value, **key;
	mmc_t *mmc;
	int inx, result, real_value = 1;
	int ac = ZEND_NUM_ARGS();
	char *command;

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}

		if (ac < 1 || ac > 2 || zend_get_parameters_ex(ac, &key, &value) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		convert_to_string_ex(key);

		if (ac == 2) {
			convert_to_long_ex(value);
			real_value = Z_LVAL_PP(value);
		}

		if ((result = mmc_incr_decr(mmc, cmd, Z_STRVAL_PP(key), real_value)) < 0) {
			RETURN_FALSE;
		}
		RETURN_LONG(result);
	}

	if (cmd > 0) {
		command = estrdup("increment");
	}
	else {
		command = estrdup("decrement");
	}
	
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "memcache_%s() should not be called like this. Use $memcache->%s($key,$value) to %s item", command, command, command);
	efree(command);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto object memcache_connect( string host, int port [, int timeout ]) 
   Connects to server and returns Memcache object */
PHP_FUNCTION(memcache_connect)
{
	zval **host, **port, **timeout;
	mmc_t *mmc = NULL;
	int timeout_sec = MMC_DEFAULT_TIMEOUT;
	int ac = ZEND_NUM_ARGS();

	if (ac < 2 || ac > 3 || zend_get_parameters_ex(ac, &host, &port, &timeout) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(host);
	convert_to_long_ex(port);

	if (ac == 3) {
		convert_to_long_ex(timeout);
		timeout_sec = Z_LVAL_PP(timeout);
	}
	
	mmc = mmc_open(Z_STRVAL_PP(host), Z_LVAL_PP(port), timeout_sec);
	
	if (mmc == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't connect to %s:%ld.",Z_STRVAL_PP(host), Z_LVAL_PP(port));
		RETURN_FALSE;
	}

	object_init_ex(return_value, memcache_class_entry_ptr);
	add_property_resource(return_value, "connection",mmc->id);
}
/* }}} */

/* {{{ proto string memcache_get_version( void ) 
   Returns server's version */
PHP_FUNCTION(memcache_get_version)
{
	zval *id;
	mmc_t *mmc;
	int inx;
	char *version;

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}
		
		if ( (version = mmc_get_version(mmc)) ) {
			RETURN_STRING(version, 1);
		}
		RETURN_FALSE;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache_get_version() should not be called like this. Use $memcache->getVersion() instead to get server's version");
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_add( string key, mixed var [, int expire ] ) 
   Adds new item. Item with such key should not exist. */
PHP_FUNCTION(memcache_add)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU,"add");
}
/* }}} */

/* {{{ proto bool memcache_set( string key, mixed var [, int expire ] ) 
   Sets the value of an item. Item may exist or not */
PHP_FUNCTION(memcache_set)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU,"set");
}
/* }}} */

/* {{{ proto bool memcache_replace( string key, mixed var [, int expire ] ) 
   Replaces existing item. Returns false if item doesn't exist */
PHP_FUNCTION(memcache_replace)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU,"replace");
}
/* }}} */

/* {{{ proto mixed memcache_get( string key ) 
   Returns value of existing item or false */
PHP_FUNCTION(memcache_get)
{
	zval *id, **key;
	mmc_t *mmc;
	int inx, flags = 0, data_len = 0;
	char *data = NULL;
	const char *tmp = NULL;

    php_unserialize_data_t var_hash;

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}
		if (zend_get_parameters_ex(1, &key) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	
		convert_to_string_ex(key);

		if (mmc_exec_retrieval_cmd(mmc, "get", Z_STRVAL_PP(key), &flags, &data, &data_len) > 0) {

			if (!data || !data_len) {
				RETURN_EMPTY_STRING();
			}
			
			tmp = data;
			
			if (flags & MMC_SERIALIZED) {
                PHP_VAR_UNSERIALIZE_INIT(var_hash);
                if (!php_var_unserialize(&return_value, &tmp, tmp + data_len,  &var_hash TSRMLS_CC)) {
					PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
                    zval_dtor(return_value);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %d of %d bytes", tmp - data, data_len);
                    RETURN_FALSE;
                }
                PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
			}
			else {
				RETVAL_STRINGL(data, data_len, 0);
			}
		}
		else {
			RETURN_FALSE;
		}
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "memcache_get() should not be called like this. Use $memcache->get($key) instead to get item's value");
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto bool memcache_delete( string key, [ int expire ]) 
   Deletes existing item */
PHP_FUNCTION(memcache_delete)
{
	zval *id, **key, **time;
	mmc_t *mmc;
	int inx, result, real_time;
	int ac = ZEND_NUM_ARGS();

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}
		if (ac < 1 || ac > 2 || zend_get_parameters_ex(ac, &key, &time) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		
		convert_to_string_ex(key);
		real_time = 0;
		
		if (ac > 1) {
			convert_to_long_ex(time);
			real_time = Z_LVAL_PP(time);
		}
		
		result = mmc_delete(mmc, Z_STRVAL_PP(key), real_time);
		
		if (result > 0) {
			RETURN_TRUE;
		}
		else if (result == 0) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "such key doesn't exist");
			RETURN_FALSE;
		}
		else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "error while deleting item");
		}
		RETURN_FALSE;
	}
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "memcache_delete() should not be called like this. Use $memcache->delete($key,$timeout) to delete item");
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_debug( bool onoff ) 
   Turns on/off internal debugging */
PHP_FUNCTION(memcache_debug)
{
	zval **onoff;

	if (zend_get_parameters_ex(1, &onoff) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(onoff);
	if (Z_LVAL_PP(onoff) != 0) {
		MEMCACHE_G(debug_mode) = 1;
	}
	else {
		MEMCACHE_G(debug_mode) = 0;
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array memcache_get_stats( void )
   Returns server's statistics */
PHP_FUNCTION(memcache_get_stats)
{
	zval *id;
	mmc_t *mmc;
	int inx;

	if ((id = getThis()) != 0) {
		if ((inx = mmc_get_connection(id, &mmc TSRMLS_CC)) == 0) {
			RETURN_FALSE;
		}

		if (mmc_get_stats(mmc, &return_value) < 0) {
			RETURN_FALSE;
		}
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "memcache_get_stats() should not be called like this. Use $memcache->getStats() to get server's statistics");
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int memcache_increment( string key [, int value ] ) 
   Increments existing variable */
PHP_FUNCTION(memcache_increment)
{
	php_mmc_incr_decr(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto int memcache_decrement( string key [, int value ] ) 
   Decrements existing variable */
PHP_FUNCTION(memcache_decrement)
{
	php_mmc_incr_decr(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
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
