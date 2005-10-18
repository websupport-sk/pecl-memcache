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

/* TODO
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

#include <zlib.h>
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "php_network.h"
#include "php_memcache.h"

/* True global resources - no need for thread safety here */
static int le_memcache, le_pmemcache;
static zend_class_entry *memcache_class_entry_ptr;

ZEND_DECLARE_MODULE_GLOBALS(memcache)

/* {{{ memcache_functions[]
 */
zend_function_entry memcache_functions[] = {
	PHP_FE(memcache_connect,		NULL)
	PHP_FE(memcache_pconnect,		NULL)
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
	PHP_FE(memcache_close,			NULL)
	PHP_FE(memcache_flush,			NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_memcache_class_functions[] = {
	PHP_FALIAS(connect,			memcache_connect,			NULL)
	PHP_FALIAS(pconnect,		memcache_pconnect,			NULL)
	PHP_FALIAS(getversion,		memcache_get_version,		NULL)
	PHP_FALIAS(add,				memcache_add,				NULL)
	PHP_FALIAS(set,				memcache_set,				NULL)
	PHP_FALIAS(replace,			memcache_replace,			NULL)
	PHP_FALIAS(get,				memcache_get,				NULL)
	PHP_FALIAS(delete,			memcache_delete,			NULL)
	PHP_FALIAS(getstats,		memcache_get_stats,			NULL)	
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
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMCACHE
ZEND_GET_MODULE(memcache)
#endif

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
static void _mmc_server_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static void _mmc_pserver_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static int mmc_compress(char **, int *, char *, int TSRMLS_DC);
static int mmc_uncompress(char **, long *, char *, int);
static int mmc_get_connection(zval *, mmc_t ** TSRMLS_DC);
static mmc_t* mmc_open(const char *, int, short, long, int, char **, int * TSRMLS_DC);
static int mmc_close(mmc_t * TSRMLS_DC);
static int mmc_readline(mmc_t * TSRMLS_DC);
static char * mmc_get_version(mmc_t * TSRMLS_DC);
static int mmc_str_left(char *, char *, int, int);
static int mmc_sendcmd(mmc_t *, const char *, int TSRMLS_DC);
static int mmc_exec_storage_cmd(mmc_t *, char *, int, char *, int, int, int, char *, int TSRMLS_DC);
static int mmc_parse_response(char *, char **, int, int *, int *);
static int mmc_exec_retrieval_cmd(mmc_t *, char *, int, char *, int, int *, char **, int * TSRMLS_DC);
static int mmc_exec_retrieval_cmd_multi(mmc_t *, zval *, zval ** TSRMLS_DC);
static int mmc_delete(mmc_t *, char *, int, int TSRMLS_DC);
static int mmc_flush(mmc_t * TSRMLS_DC);
static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *, int);
static int mmc_get_stats (mmc_t *, zval ** TSRMLS_DC);
static int mmc_incr_decr (mmc_t *, int, char *, int, int TSRMLS_DC);
static void php_mmc_incr_decr (INTERNAL_FUNCTION_PARAMETERS, int);
static void php_mmc_connect (INTERNAL_FUNCTION_PARAMETERS, int);
/* }}} */

/* {{{ php_memcache_init_globals()
*/
static void php_memcache_init_globals(zend_memcache_globals *memcache_globals_p TSRMLS_DC)
{ 
	MEMCACHE_G(debug_mode)		  = 0;
	MEMCACHE_G(default_port)	  = MMC_DEFAULT_PORT;
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

	le_memcache = zend_register_list_destructors_ex(_mmc_server_list_dtor, NULL, "memcache connection", module_number);
	le_pmemcache = zend_register_list_destructors_ex(NULL, _mmc_pserver_list_dtor, "persistent memcache connection", module_number);

#ifdef ZTS
	ts_allocate_id(&memcache_globals_id, sizeof(zend_memcache_globals), (ts_allocate_ctor) php_memcache_init_globals, NULL);
#else
	php_memcache_init_globals(&memcache_globals TSRMLS_CC);
#endif
	
	REGISTER_LONG_CONSTANT("MEMCACHE_COMPRESSED",MMC_COMPRESSED, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(memcache)
{
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
}
/* }}} */

/* ------------------
   internal functions 
   ------------------ */

static void _mmc_server_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc = (mmc_t *)rsrc->ptr;
	mmc_close(mmc TSRMLS_CC);
	efree(mmc);
}
/* }}} */

static void _mmc_pserver_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc = (mmc_t *)rsrc->ptr;
	mmc_close(mmc TSRMLS_CC);
	free(mmc);
	MEMCACHE_G(num_persistent)--;
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

static int mmc_get_connection(zval *id, mmc_t **mmc TSRMLS_DC) /* {{{ */
{
	zval	**connection;
	int		resource_type;

	if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "connection", sizeof("connection"), (void **)&connection) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find connection identifier");
		return 0;
	}

	*mmc = (mmc_t *) zend_list_find(Z_LVAL_PP(connection), &resource_type);

	if (!*mmc || (resource_type != le_pmemcache && resource_type != le_memcache )) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "connection identifier not found");
		return 0;
	}

	if (!(*mmc)->stream) {
		/* found previously closed connection */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "using previously closed connection");
		return 0;
	}
	
	return Z_LVAL_PP(connection);
}
/* }}} */

static mmc_t* mmc_open(const char *host, int host_len, short port, long timeout, int persistent, char **error_string, int *errnum TSRMLS_DC) /* {{{ */
{
	mmc_t			*mmc;
	struct timeval	tv;
	char			*hostname = NULL, *hash_key = NULL, *errstr = NULL, *version;
	int				hostname_len, err;
	
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	
	hostname = emalloc(host_len + MAX_LENGTH_OF_LONG + 1 + 1);
	hostname_len = sprintf(hostname, "%s:%d", host, port);

	if (persistent) {
		mmc = malloc( sizeof(mmc_t) );

		hash_key = emalloc(sizeof("mmc_open___") - 1 + hostname_len + 1);
		sprintf(hash_key, "mmc_open___%s", hostname);
	}
	else {
		mmc = emalloc( sizeof(mmc_t) );
	}

	mmc->stream = NULL;
	mmc->persistent = persistent ? 1 : 0;
	
#if PHP_API_VERSION > 20020918	
	mmc->stream = php_stream_xport_create( hostname, hostname_len, 
										   ENFORCE_SAFE_MODE | REPORT_ERRORS,
										   STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
										   hash_key, &tv, NULL, &errstr, &err);
#else
	if (persistent) {
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
		mmc->stream = php_stream_sock_open_host(host, (unsigned short)port, socktype, &tv, hash_key);
	}
	
#endif
	
	efree(hostname);

	if (hash_key) {
		efree(hash_key);
	}
	
	if (!mmc->stream) {
		MMC_DEBUG(("mmc_open: can't open socket to host"));
		if (persistent) {
			free(mmc);
		}
		else {
			efree(mmc);
		}
		if (error_string && errstr) {
			*error_string = errstr;
		}
		if (errnum) {
			*errnum = err;
		}
		return NULL;
	}
	
    php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &tv);
	php_stream_set_option(mmc->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
		
	mmc->timeout = timeout;
	
	if ((version = mmc_get_version(mmc TSRMLS_CC)) == NULL) {
		MMC_DEBUG(("mmc_open: failed to get server's version"));
		if (persistent) {
			php_stream_pclose(mmc->stream);
			free(mmc);
		}
		else {
			php_stream_close(mmc->stream);
			efree(mmc);
		}
		return NULL;
	}
	efree(version);
	return mmc;
}
/* }}} */

static int mmc_close(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	MMC_DEBUG(("mmc_close: closing connection to server"));

	if (mmc == NULL) {
		return 0;
	}

	if (!mmc->persistent && mmc->stream) {
		mmc_sendcmd(mmc,"quit", 4 TSRMLS_CC);
		php_stream_close(mmc->stream);
	}

	mmc->stream = NULL;
	
	return 1;
}
/* }}} */

static int mmc_readline(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	char *buf;

	if (mmc->stream == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "cannot read data from already closed socket");
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
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to read the server's response");
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
		version_str = estrndup(mmc->inbuf + sizeof("VERSION ") - 1, len - sizeof("VERSION ") - 1 - sizeof("\r\n") - 1);
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
		efree(command);
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to send command to the server");		
		MMC_DEBUG(("mmc_sendcmd: write failed"));
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

	MMC_PREPARE_KEY(key, key_len);

	size = sprintf(real_command, "%s %s %d %d %d\r\n", command, key, flags, expire, data_len);

	memcpy(real_command + size, data, data_len);
	memcpy(real_command + size + data_len, "\r\n", sizeof("\r\n") - 1);
	size = size + data_len + sizeof("\r\n") - 1;
	real_command[size] = '\0';
	
	MMC_DEBUG(("mmc_exec_storage_cmd: store cmd is '%s'", real_command));
	MMC_DEBUG(("mmc_exec_storage_cmd: trying to store '%s', %d bytes", data, data_len));
	
	/* send command & data */
	if (php_stream_write(mmc->stream, real_command, size) != size) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to send command and data to the server");
		efree(real_command);
		return -1;
	}
	efree(real_command);
	
	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0) {
		return -1;
	}

	MMC_DEBUG(("mmc_exec_storage_cmd: response is '%s'", mmc->inbuf));

	/* stored or not? */
	if(mmc_str_left(mmc->inbuf,"STORED", response_buf_size, sizeof("STORED") - 1)) {
		return 1;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "an error occured while trying to store the item on the server");
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

static int mmc_exec_retrieval_cmd(mmc_t *mmc, char *command, int command_len, char *key, int key_len, int *flags, char **data, int *data_len TSRMLS_DC) /* {{{ */
{
	char *real_command, *tmp;
	int size, response_buf_size;
	
	real_command = emalloc(
							  command_len
							+ 1				/* space */ 
							+ key_len 
							+ 1
							);

	MMC_PREPARE_KEY(key, key_len);

	size = sprintf(real_command, "%s %s", command, key);

	real_command [size] = '\0';

	MMC_DEBUG(("mmc_exec_retrieval_cmd: trying to get '%s'", key));
	
	/* gimme! =) */
	if (mmc_sendcmd(mmc, real_command, size TSRMLS_CC) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);
	
	/* get server's response */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0){
		return -1;
	}

	MMC_DEBUG(("mmc_exec_retrieval_cmd: got response '%s'", mmc->inbuf));
	
	/* what's this? */

	if (mmc_str_left(mmc->inbuf,"END", response_buf_size, sizeof("END") - 1)) {
		/* not found */
		return -1;
	}
	
	if(mmc_str_left(mmc->inbuf,"VALUE", response_buf_size, sizeof("VALUE") - 1) <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "got invalid server's response header");
		return -1;
	}
	
	tmp = estrdup(mmc->inbuf);
	if (mmc_parse_response(tmp, NULL, response_buf_size, flags, data_len) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "got invalid server's response");
		efree(tmp);
		return -1;
	}
	efree(tmp);

	MMC_DEBUG(("mmc_exec_retrieval_cmd: data len is %d bytes", *data_len));
	
	{
		int data_to_be_read = *data_len + 2;
		int offset = 0;

		*data = emalloc(data_to_be_read + 1);
		
		while (data_to_be_read > 0) {
		    size = php_stream_read(mmc->stream, *data + offset, data_to_be_read);
		    if (size == 0) break;
		    offset += size, data_to_be_read -= size;
		}
		
		if (data_to_be_read > 0) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "incomplete data block (expected %d, got %d)", (*data_len + 2), offset);
			efree(*data);
			return -1;
		}
		
		(*data) [*data_len] = '\0';
		MMC_DEBUG(("mmc_exec_retrieval_cmd: data '%s'", *data));
	}

	/* read "END" */
	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) < 0) {
		efree(*data);
		return -1;
	}

	if(mmc_str_left(mmc->inbuf,"END", response_buf_size, sizeof("END") - 1) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "got invalid data end delimiter");
		efree(*data);
		return -1;
	}
	
	return 1;
}
/* }}} */

static int mmc_exec_retrieval_cmd_multi(mmc_t *mmc, zval *keys, zval **result TSRMLS_DC) /* {{{ */
{
	HashPosition pos;
	zval	**tmp_zval, *tmp_serialize;
	int		flags, data_len;
	char	*data = NULL;
	char	*real_command = NULL, *tmp_c, *tmp_name;
	int		size, response_buf_size;
	smart_str	implstr = {0};

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(keys), &pos);
	
	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(keys), (void **) &tmp_zval, &pos) == SUCCESS) {
		if (Z_TYPE_PP(tmp_zval) != IS_STRING) {
			SEPARATE_ZVAL(tmp_zval);
			convert_to_string(*tmp_zval);
		} 

		MMC_PREPARE_KEY(Z_STRVAL_PP(tmp_zval), Z_STRLEN_PP(tmp_zval));

		smart_str_appendl(&implstr, Z_STRVAL_PP(tmp_zval), Z_STRLEN_PP(tmp_zval));
		smart_str_appendl(&implstr, " ", 1);
		zend_hash_move_forward_ex(Z_ARRVAL_P(keys), &pos);
	}
	smart_str_0(&implstr);

	real_command = emalloc(
							  sizeof("get") - 1
							+ 1				/* space */ 
							+ implstr.len 
							+ 1
							);

	if (!real_command) {
		smart_str_free(&implstr);
		return -1;
	}
	
	size = sprintf(real_command, "get %s", implstr.c);
	real_command [size] = '\0';

	MMC_DEBUG(("mmc_exec_retrieval_cmd_multi: trying to get '%s'", implstr.c));
	
	smart_str_free(&implstr);

	if (mmc_sendcmd(mmc, real_command, size TSRMLS_CC) < 0) {
		efree(real_command);
		return -1;
	}
	efree(real_command);

	array_init(*result);

	while ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) > 0) {

		tmp_c = estrdup(mmc->inbuf);
		if(mmc_str_left(mmc->inbuf,"END", response_buf_size, sizeof("END") - 1)) {
			/* reached the end of the data */
			efree(tmp_c);
			return 1;
		}
		else if (mmc_parse_response(tmp_c, &tmp_name, response_buf_size, &flags, &data_len) < 0) {
			efree(tmp_name);
			efree(tmp_c);
			return -1;
		}
		efree(tmp_c);

		MMC_DEBUG(("mmc_exec_retrieval_cmd: data len is %d bytes", data_len));
		
		{
			int data_to_be_read = data_len + 2;
			int offset = 0;

			data = emalloc(data_to_be_read + 1);
			
			while (data_to_be_read > 0) {
				size = php_stream_read(mmc->stream, data + offset, data_to_be_read);
				if (size == 0) break;
				offset += size, data_to_be_read -= size;
			}

			if (data_to_be_read > 0) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "incomplete data block (expected %d, got %d)", (data_len + 2), offset);
				efree(tmp_name);
				efree(data);
				return -1;
			}

			data [data_len] = '\0';
			MMC_DEBUG(("mmc_exec_retrieval_cmd: data '%s'", data));
		}

		if (!data) {
			add_assoc_bool(*result, tmp_name, 0);
			efree(tmp_name);
			continue;
		}
		
		if (flags & MMC_COMPRESSED) {
			const char *tmp;
			long result_len = 0;
			char *result_data; 

			if (!mmc_uncompress(&result_data, &result_len, data, data_len)) {
				add_assoc_bool(*result, tmp_name, 0);
				efree(tmp_name);
				efree(data);
				continue;
			}
			
			tmp = result_data;
			if (flags & MMC_SERIALIZED) {
			    php_unserialize_data_t var_hash;
				
				MAKE_STD_ZVAL(tmp_serialize);
				PHP_VAR_UNSERIALIZE_INIT(var_hash);
				if (!php_var_unserialize(&tmp_serialize, (const unsigned char **) &tmp, tmp + result_len,  &var_hash TSRMLS_CC)) {
					PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
					
					php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %d of %d bytes", tmp - data, data_len);
					efree(data);
					efree(result_data);
					add_assoc_bool(*result, tmp_name, 0);
				}
				else {
					efree(data);
					efree(result_data);
					PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
					add_assoc_zval(*result, tmp_name, tmp_serialize);
				}
			}
			else {
				efree(data);
				add_assoc_stringl(*result, tmp_name, result_data, result_len, 1);
			}
		}
		else if (flags & MMC_SERIALIZED) {
			const char *tmp = data;
		    php_unserialize_data_t var_hash;
			
			MAKE_STD_ZVAL(tmp_serialize);
			PHP_VAR_UNSERIALIZE_INIT(var_hash);
			if (!php_var_unserialize(&tmp_serialize, (const unsigned char **) &tmp, tmp + data_len,  &var_hash TSRMLS_CC)) {
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %d of %d bytes", tmp - data, data_len);
				efree(data);
				add_assoc_bool(*result, tmp_name, 0);
			}
			else {
				efree(data);
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				add_assoc_zval(*result, tmp_name, tmp_serialize);
			}
		}
		else {
			add_assoc_stringl(*result, tmp_name, data, data_len, 1);
			efree(data);
		}
		efree(tmp_name);
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

	MMC_PREPARE_KEY(key, key_len);
	
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
	
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to delete item");
	
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
	
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to flush server's cache");
	
	/* hmm.. */
	return -1;
}
/* }}} */

static int mmc_get_stats (mmc_t *mmc, zval **stats TSRMLS_DC) /* {{{ */
{
	int response_buf_size, stats_tmp_len, space_len, i = 0;
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
			i++;
		}
		else if (mmc_str_left(mmc->inbuf, "END", response_buf_size, sizeof("END") - 1)) {
			/* END of stats*/
			break;
		}
		else {
			/* unknown error */
			break;
		}
	}

	if (i == 0) {
		efree(*stats);
	}

	return 1;
}
/* }}} */

static int mmc_incr_decr (mmc_t *mmc, int cmd, char *key, int key_len, int value TSRMLS_DC) /* {{{ */
{
	char *command, *command_name;
	int  cmd_len, response_buf_size;
	
	/* 4 is for strlen("incr") or strlen("decr"), doesn't matter */
	command = emalloc(4 + key_len + MAX_LENGTH_OF_LONG + 1); 

	MMC_PREPARE_KEY(key, key_len);
	
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

	if ((response_buf_size = mmc_readline(mmc TSRMLS_CC)) > 0) {
		MMC_DEBUG(("mmc_incr_decr: server's answer is: '%s'", mmc->inbuf));
		if (mmc_str_left(mmc->inbuf, "NOT_FOUND", response_buf_size, sizeof("NOT_FOUND") - 1)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to %sement variable - item with such key not found", command_name);
			efree(command_name);
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "ERROR", response_buf_size, sizeof("ERROR") - 1)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to %sement variable - an error occured", command_name);
			efree(command_name);
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "CLIENT_ERROR", response_buf_size, sizeof("CLIENT_ERROR") - 1)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to %sement variable - client error occured", command_name);
			efree(command_name);
			return -1;
		}
		else if (mmc_str_left(mmc->inbuf, "SERVER_ERROR", response_buf_size, sizeof("SERVER_ERROR") - 1)) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to %sement variable - server error occured", command_name);
			efree(command_name);
			return -1;
		}
		else {
			efree(command_name);
			return atoi(mmc->inbuf);
		}
	}

	efree(command_name);
	return -1;

}
/* }}} */

static void php_mmc_store (INTERNAL_FUNCTION_PARAMETERS, char *command, int command_len) /* {{{ */
{
	mmc_t *mmc;
	int inx, data_len = 0, real_expire = 0, real_flags = 0, key_len;
	char *data = NULL, *real_key;
	int ac = ZEND_NUM_ARGS();
	zval *key, *var, *flags, *expire, *mmc_object = getThis();

    php_serialize_data_t var_hash;
    smart_str buf = {0};

	if (mmc_object == NULL) {
		if (ac < 3 || ac > 5 || zend_get_parameters(ht, ac, &mmc_object, &key, &var, &flags, &expire) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		if (ac >= 4) {
			convert_to_long(flags);
			real_flags = Z_LVAL_P(flags);
		}

		if (ac == 5) {
			convert_to_long(expire);
			real_expire = Z_LVAL_P(expire);
		}

	}
	else {
		if (ac < 2 || ac > 4 || zend_get_parameters(ht, ac, &key, &var, &flags, &expire) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		if (ac >= 3) {
			convert_to_long(flags);
			real_flags = Z_LVAL_P(flags);
		}

		if (ac == 4) {
			convert_to_long(expire);
			real_expire = Z_LVAL_P(expire);
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	convert_to_string(key);

	if (Z_STRLEN_P(key) > MMC_KEY_MAX_SIZE) {
		real_key = estrndup(Z_STRVAL_P(key), MMC_KEY_MAX_SIZE);
		key_len = MMC_KEY_MAX_SIZE;
	}
	else {
		real_key = estrdup(Z_STRVAL_P(key));
		key_len = Z_STRLEN_P(key);
	}
	
	switch (Z_TYPE_P(var)) {
		case IS_STRING:
		case IS_LONG:
		case IS_DOUBLE:
		case IS_BOOL:
			convert_to_string(var);
			if (real_flags & MMC_COMPRESSED) {
				if (!mmc_compress(&data, &data_len, Z_STRVAL_P(var), Z_STRLEN_P(var) TSRMLS_CC)) {
					RETURN_FALSE;
				}
			}
			else {
				data = Z_STRVAL_P(var);
				data_len = Z_STRLEN_P(var);
			}
			break;
		default:
			PHP_VAR_SERIALIZE_INIT(var_hash);
			php_var_serialize(&buf, &var, &var_hash TSRMLS_CC);
			PHP_VAR_SERIALIZE_DESTROY(var_hash);
			
			if (!buf.c) {
				/* you're trying to save null or smthing went really wrong */
				RETURN_FALSE;
			}
			
			real_flags |= MMC_SERIALIZED;

			if (real_flags & MMC_COMPRESSED) {
				if (!mmc_compress(&data, &data_len, buf.c, buf.len TSRMLS_CC)) {
					RETURN_FALSE;
				}
			}
			else {
				data = buf.c;
				data_len = buf.len;
			}
			break;
	}

	if (mmc_exec_storage_cmd(mmc, command, command_len, real_key, key_len, real_flags, real_expire, data, data_len TSRMLS_CC) > 0) {
		if (real_flags & MMC_SERIALIZED) {
			smart_str_free(&buf);
		}
		if (real_flags & MMC_COMPRESSED) {
			efree(data);
		}
		efree(real_key);
		RETURN_TRUE;
	}
	if (real_flags & MMC_SERIALIZED) {
		smart_str_free(&buf);
	}
	if (real_flags & MMC_COMPRESSED) {
		efree(data);
	}
	efree(real_key);
	RETURN_FALSE;
}
/* }}} */

static void php_mmc_incr_decr(INTERNAL_FUNCTION_PARAMETERS, int cmd) /* {{{ */
{
	mmc_t *mmc;
	int inx, result, real_value = 1;
	int ac = ZEND_NUM_ARGS();
	zval *value, *key, *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (ac < 2 || ac > 3 || zend_get_parameters(ht, ac, &mmc_object, &key, &value) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		
		if (ac > 2) {
			convert_to_long(value);
			real_value = Z_LVAL_P(value);
		}
	}
	else {
		if (ac < 1 || ac > 2 || zend_get_parameters(ht, ac, &key, &value) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		
		if (ac > 1) {
			convert_to_long(value);
			real_value = Z_LVAL_P(value);
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	convert_to_string(key);

	if ((result = mmc_incr_decr(mmc, cmd, Z_STRVAL_P(key), Z_STRLEN_P(key), real_value TSRMLS_CC)) < 0) {
		RETURN_FALSE;
	}

	RETURN_LONG(result);
}
/* }}} */

static void php_mmc_connect (INTERNAL_FUNCTION_PARAMETERS, int persistent) /* {{{ */
{
	zval **host, **port, **timeout, *mmc_object = getThis();
	mmc_t *mmc = NULL;
	int timeout_sec = MMC_DEFAULT_TIMEOUT, real_port, hash_key_len, errnum = 0;
	int ac = ZEND_NUM_ARGS();
	char *hash_key = NULL, *version, *error_string = NULL;

	if (ac < 1 || ac > 3 || zend_get_parameters_ex(ac, &host, &port, &timeout) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(host);
	
	real_port = MEMCACHE_G(default_port);
	
	if (ac > 1) {
		convert_to_long_ex(port);
		real_port = Z_LVAL_PP(port);
	}

	if (ac == 3) {
		convert_to_long_ex(timeout);
		timeout_sec = Z_LVAL_PP(timeout);
	}

	if (persistent) {
		list_entry *le;

		MMC_DEBUG(("php_mmc_connect: seeking for persistent connection"));
		
		hash_key = emalloc(sizeof("mmc_connect___") - 1 + Z_STRLEN_PP(host) + MAX_LENGTH_OF_LONG + 1);
		hash_key_len = sprintf(hash_key, "mmc_connect___%s:%d", Z_STRVAL_PP(host), real_port);
		if (zend_hash_find(&EG(persistent_list), hash_key, hash_key_len+1, (void **) &le) == FAILURE) {
			list_entry new_le;
			
			MMC_DEBUG(("php_mmc_connect: connection wasn't found in the hash"));

			if ((mmc = mmc_open(Z_STRVAL_PP(host), Z_STRLEN_PP(host), real_port, timeout_sec, persistent, &error_string, &errnum TSRMLS_CC)) == NULL) {
				MMC_DEBUG(("php_mmc_connect: connection failed"));
				efree(hash_key);
				goto connect_done;
			}

			mmc->id = zend_list_insert(mmc,le_pmemcache);
			
			new_le.type = le_pmemcache;
			new_le.ptr  = mmc;
			
			if (zend_hash_update(&EG(persistent_list), hash_key, hash_key_len+1, (void *) &new_le, sizeof(list_entry), NULL)==FAILURE) {
				efree(hash_key);
				goto connect_done;
			}
			efree(hash_key);
			MEMCACHE_G(num_persistent)++;
		}
		else {
			MMC_DEBUG(("php_mmc_connect: connection found in the hash"));
			
			if (le->type == le_pmemcache && le->ptr != NULL) {

				if ((version = mmc_get_version(le->ptr TSRMLS_CC)) != NULL) {
					mmc = (mmc_t *)le->ptr;

					MMC_DEBUG(("memcache_connect: all right"));
					mmc->id = zend_list_insert(mmc,le_pmemcache);

					/* Ok, server is alive and working */
					efree(version);
					efree(hash_key);
					goto connect_done;
				}
				else {
					list_entry new_le;
					
					MMC_DEBUG(("php_mmc_connect: server has gone away, reconnecting.."));

					/* Daemon has gone away, reconnecting */
					MEMCACHE_G(num_persistent)--;
					if ((mmc = mmc_open(Z_STRVAL_PP(host), Z_STRLEN_PP(host), real_port, timeout_sec, persistent, &error_string, &errnum TSRMLS_CC)) == NULL) {
						MMC_DEBUG(("php_mmc_connect: reconnect failed"));						
						zend_hash_del(&EG(persistent_list), hash_key, hash_key_len+1);
						efree(hash_key);
						goto connect_done;
					}

					mmc->id = zend_list_insert(mmc,le_pmemcache);
					
					new_le.type = le_pmemcache;
					new_le.ptr  = mmc;
					
					if (zend_hash_update(&EG(persistent_list), hash_key, hash_key_len+1, (void *) &new_le, sizeof(list_entry), NULL)==FAILURE) {
						efree(hash_key);
						goto connect_done;
					}
					efree(hash_key);
					MEMCACHE_G(num_persistent)++;
				}
			}
			else {
				list_entry new_le;

				MMC_DEBUG(("php_mmc_connect: smthing was wrong, reconnecting.."));
				if ((mmc = mmc_open(Z_STRVAL_PP(host), Z_STRLEN_PP(host), real_port, timeout_sec, persistent, &error_string, &errnum TSRMLS_CC)) == NULL) {
					MMC_DEBUG(("php_mmc_connect: reconnect failed"));
					zend_hash_del(&EG(persistent_list), hash_key, hash_key_len+1);
					efree(hash_key);
					goto connect_done;
				}

				mmc->id = zend_list_insert(mmc,le_pmemcache);
				
				new_le.type = le_pmemcache;
				new_le.ptr  = mmc;
				
				if (zend_hash_update(&EG(persistent_list), hash_key, hash_key_len+1, (void *) &new_le, sizeof(list_entry), NULL)==FAILURE) {
					efree(hash_key);
					goto connect_done;
				}
				efree(hash_key);
				MEMCACHE_G(num_persistent)++;
			}
		}
	}
	else {
		MMC_DEBUG(("php_mmc_connect: creating regular connection"));
		mmc = mmc_open(Z_STRVAL_PP(host), Z_STRLEN_PP(host), real_port, timeout_sec, persistent, &error_string, &errnum TSRMLS_CC);
		if (mmc != NULL) {
			mmc->id = zend_list_insert(mmc,le_memcache);
		}
	}

connect_done:
	
	if (mmc == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't connect to %s:%d, %s (%d)",Z_STRVAL_PP(host), real_port, error_string ? error_string : "Unknown error", errnum);
		if (error_string) {
			efree(error_string);
		}
		RETURN_FALSE;
	}

	if (mmc_object == NULL) {
		object_init_ex(return_value, memcache_class_entry_ptr);
		add_property_resource(return_value, "connection",mmc->id);
	}
	else {
		add_property_resource(mmc_object, "connection",mmc->id);
		RETURN_TRUE;
	}	
}
/* }}} */

/* ---------------- 
   module functions 
   ---------------- */

/* {{{ proto object memcache_connect( string host [, int port [, int timeout ] ]) 
   Connects to server and returns Memcache object */
PHP_FUNCTION(memcache_connect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto object memcache_pconnect( string host [, int port [, int timeout ] ]) 
   Connects to server and returns Memcache object */
PHP_FUNCTION(memcache_pconnect)
{
	php_mmc_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto string memcache_get_version( object memcache ) 
   Returns server's version */
PHP_FUNCTION(memcache_get_version)
{
	mmc_t *mmc;
	int inx;
	char *version;
	zval *mmc_object = getThis();
	
	if (mmc_object == NULL) {
		if (zend_get_parameters(ht, 1, &mmc_object) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	if ( (version = mmc_get_version(mmc TSRMLS_CC)) ) {
		RETURN_STRING(version, 0);
	}
	php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to get server's version");
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_add( object memcache, string key, mixed var [, int expire ] ) 
   Adds new item. Item with such key should not exist. */
PHP_FUNCTION(memcache_add)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "add", sizeof("add") - 1);
}
/* }}} */

/* {{{ proto bool memcache_set( object memcache, string key, mixed var [, int expire ] ) 
   Sets the value of an item. Item may exist or not */
PHP_FUNCTION(memcache_set)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "set", sizeof("set") - 1);
}
/* }}} */

/* {{{ proto bool memcache_replace( object memcache, string key, mixed var [, int expire ] ) 
   Replaces existing item. Returns false if item doesn't exist */
PHP_FUNCTION(memcache_replace)
{
	php_mmc_store(INTERNAL_FUNCTION_PARAM_PASSTHRU, "replace", sizeof("replace") - 1);
}
/* }}} */

/* {{{ proto mixed memcache_get( object memcache, string key ) 
   Returns value of existing item or false */
PHP_FUNCTION(memcache_get)
{
	mmc_t *mmc;
	long result_len = 0;
	int inx, flags = 0, data_len = 0;
	char *data = NULL, *result_data = NULL;
	const char *tmp = NULL;
    php_unserialize_data_t var_hash;
	zval *key, *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_get_parameters(ht, 2, &mmc_object, &key) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}
	else {
		if (zend_get_parameters(ht, 1, &key) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(key) != IS_ARRAY) {
		convert_to_string(key);

		if (mmc_exec_retrieval_cmd(mmc, "get", sizeof("get") - 1, Z_STRVAL_P(key), Z_STRLEN_P(key), &flags, &data, &data_len TSRMLS_CC) > 0) {

			if (!data) {
				RETURN_EMPTY_STRING();
			}
			
			if (flags & MMC_COMPRESSED) {
				if (!mmc_uncompress(&result_data, &result_len, data, data_len)) {
					RETURN_FALSE;
				}
				
				tmp = result_data;
				
				if (flags & MMC_SERIALIZED) {
					PHP_VAR_UNSERIALIZE_INIT(var_hash);
					if (!php_var_unserialize(&return_value, (const unsigned char **) &tmp, tmp + result_len,  &var_hash TSRMLS_CC)) {
						PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
						zval_dtor(return_value);
						php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %d of %d bytes", tmp - data, data_len);
						efree(data);
						efree(result_data);
						RETURN_FALSE;
					}
					efree(data);
					efree(result_data);
					PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
				}
				else {
					efree(data);
					RETURN_STRINGL(result_data, result_len, 0);
				}
			}
			else if (flags & MMC_SERIALIZED) {
				tmp = data;
				PHP_VAR_UNSERIALIZE_INIT(var_hash);
				if (!php_var_unserialize(&return_value, (const unsigned char **) &tmp, tmp + data_len,  &var_hash TSRMLS_CC)) {
					PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
					zval_dtor(return_value);
					php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %d of %d bytes", tmp - data, data_len);
					efree(data);
					RETURN_FALSE;
				}
				efree(data);
				PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
			}
			else {
				RETURN_STRINGL(data, data_len, 0);
			}
		}
		else {
			RETURN_FALSE;
		}		
	}
	else {
		if (mmc_exec_retrieval_cmd_multi(mmc, key, &return_value TSRMLS_CC) < 0) {
			RETURN_FALSE;	
		}
	}
}
/* }}} */

/* {{{ proto bool memcache_delete( object memcache, string key, [ int expire ]) 
   Deletes existing item */
PHP_FUNCTION(memcache_delete)
{
	mmc_t *mmc;
	int inx, result, real_time = 0;
	int ac = ZEND_NUM_ARGS();
	zval *key, *time, *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (ac < 2 || ac > 3 || zend_get_parameters(ht, ac, &mmc_object, &key, &time) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		
		if (ac > 2) {
			convert_to_long(time);
			real_time = Z_LVAL_P(time);
		}
	}
	else {
		if (ac < 1 || ac > 2 || zend_get_parameters(ht, ac, &key, &time) == FAILURE) {
			WRONG_PARAM_COUNT;
		}

		if (ac > 1) {
			convert_to_long(time);
			real_time = Z_LVAL_P(time);
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	convert_to_string(key);
	
	result = mmc_delete(mmc, Z_STRVAL_P(key), Z_STRLEN_P(key), real_time TSRMLS_CC);
	
	if (result > 0) {
		RETURN_TRUE;
	}
	else if (result == 0) {
		RETURN_FALSE;
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "error while deleting item");
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_debug( bool onoff ) 
   Turns on/off internal debugging */
PHP_FUNCTION(memcache_debug)
{
#if ZEND_DEBUG
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
#else
	RETURN_FALSE;
#endif
	
}
/* }}} */

/* {{{ proto array memcache_get_stats( object memcache )
   Returns server's statistics */
PHP_FUNCTION(memcache_get_stats)
{
	mmc_t *mmc;
	int inx;
	zval *mmc_object = getThis();

	if (mmc_object == NULL) {
		if (zend_get_parameters(ht, 1, &mmc_object) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	if (mmc_get_stats(mmc, &return_value TSRMLS_CC) < 0) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "failed to get server's statistics");
		RETURN_FALSE;
	}
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
	mmc_t *mmc;
	int inx;
	zval *mmc_object = getThis();
	
	if (mmc_object == NULL) {
		if (zend_get_parameters(ht, 1, &mmc_object) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	if (mmc->persistent) {
		RETURN_TRUE;
	}
	
	if (mmc_close(mmc TSRMLS_CC) > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool memcache_flush( object memcache ) 
   Flushes cache */
PHP_FUNCTION(memcache_flush)
{
	mmc_t *mmc;
	int inx;
	zval *mmc_object = getThis();
	
	if (mmc_object == NULL) {
		if (zend_get_parameters(ht, 1, &mmc_object) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	}

	if ((inx = mmc_get_connection(mmc_object, &mmc TSRMLS_CC)) == 0) {
		RETURN_FALSE;
	}

	if (mmc_flush(mmc TSRMLS_CC) > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
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
