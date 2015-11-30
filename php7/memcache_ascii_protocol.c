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

#include "memcache_pool.h"
#include "ext/standard/php_smart_string.h"

typedef struct mmc_ascii_request {
	mmc_request_t	base;							/* enable cast to mmc_request_t */
	struct {										/* stores value info while the body is being read */
		char			key[MMC_MAX_KEY_LEN + 1];
		unsigned int	flags;
		unsigned long	length;
		unsigned long	cas;						/* CAS counter */
	} value;
} mmc_ascii_request_t;

static int mmc_server_read_value(mmc_t *, mmc_request_t *);

static int mmc_stream_get_line(mmc_stream_t *io, char **line) /* 
	attempts to read a line from server, returns the line size or 0 if no complete line was available {{{ */
{
	size_t returned_len = 0;
	io->readline(io, io->input.value + io->input.idx, MMC_BUFFER_SIZE - io->input.idx, &returned_len);
	io->input.idx += returned_len;
	
	if (io->input.idx && io->input.value[io->input.idx - 1] == '\n') {
		int result = io->input.idx;
		*line = io->input.value;
		io->input.idx = 0;
		return result;
	}
	
	return 0;
}	
/* }}} */

static int mmc_request_check_response(const char *line, int line_len) /* 
	checks for response status and error codes {{{ */
{
	int response;

	if (mmc_str_left(line, "OK", line_len, sizeof("OK")-1) ||
		mmc_str_left(line, "STORED", line_len, sizeof("STORED")-1) ||
		mmc_str_left(line, "DELETED", line_len, sizeof("DELETED")-1)) 
	{
		response = MMC_OK;
	}
	else if (mmc_str_left(line, "NOT_FOUND", line_len, sizeof("NOT_FOUND")-1)) {
		response = MMC_RESPONSE_NOT_FOUND;
	}
	else if (
		mmc_str_left(line, "NOT_STORED", line_len, sizeof("NOT_STORED")-1) ||
		mmc_str_left(line, "EXISTS", line_len, sizeof("EXISTS")-1)) 
	{
		response = MMC_RESPONSE_EXISTS;
	}
	else if (mmc_str_left(line, "SERVER_ERROR out of memory", line_len, sizeof("SERVER_ERROR out of memory")-1)) {
		response = MMC_RESPONSE_OUT_OF_MEMORY;
	}
	else if (mmc_str_left(line, "SERVER_ERROR object too large", line_len, sizeof("SERVER_ERROR object too large")-1)) {
		response = MMC_RESPONSE_TOO_LARGE;
	}
	else if (
		mmc_str_left(line, "ERROR", line_len, sizeof("ERROR")-1) ||
		mmc_str_left(line, "SERVER_ERROR", line_len, sizeof("SERVER_ERROR")-1)) 
	{
		response = MMC_RESPONSE_ERROR;
	}
	else if (mmc_str_left(line, "CLIENT_ERROR", line_len, sizeof("CLIENT_ERROR")-1)) {
		response = MMC_RESPONSE_CLIENT_ERROR;
	}
	else {
		response = MMC_RESPONSE_UNKNOWN;
	}
	
	return response;
}

static int mmc_request_parse_response(mmc_t *mmc, mmc_request_t *request) /* 
	reads a generic response header and delegates it to response_handler {{{ */
{
	char *line;
	int line_len = mmc_stream_get_line(request->io, &line);
	
	if (line_len > 0) {
		int response = mmc_request_check_response(line, line_len);
		return request->response_handler(mmc, request, response, line, line_len - (sizeof("\r\n")-1), request->response_handler_param);
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_mutate(mmc_t *mmc, mmc_request_t *request) /* 
	reads and parses the <long-value> response header {{{ */
{
	char *line;
	int line_len;
	
	line_len = mmc_stream_get_line(request->io, &line);
	if (line_len > 0) {
		long lval;
		zval value;

		int response = mmc_request_check_response(line, line_len);
		if (response != MMC_RESPONSE_UNKNOWN) {
			return request->response_handler(mmc, request, response, line, line_len - (sizeof("\r\n")-1), request->response_handler_param);
		}

		if (sscanf(line, "%lu", &lval) < 1) {
			return mmc_server_failure(mmc, request->io, "Malformed VALUE header", 0);
		}
	
		ZVAL_LONG(&value, lval);
		return request->value_handler(request->key, request->key_len, &value, 0, 0, request->value_handler_param);
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_value(mmc_t *mmc, mmc_request_t *request) /* 
	reads and parses the VALUE <key> <flags> <size> <cas> response header and then reads the value body {{{ */
{
	char *line;
	int line_len;
	mmc_ascii_request_t *req = (mmc_ascii_request_t *)request;
	
	line_len = mmc_stream_get_line(request->io, &line);
	if (line_len > 0) {
		if (mmc_str_left(line, "END", line_len, sizeof("END")-1)) {
			return MMC_REQUEST_DONE;
		}
		
		if (sscanf(line, MMC_VALUE_HEADER, req->value.key, &(req->value.flags), &(req->value.length), &(req->value.cas)) < 3) {
			return mmc_server_failure(mmc, request->io, "Malformed VALUE header", 0);
		}
		
		/* memory for data + \r\n */
		mmc_buffer_alloc(&(request->readbuf), req->value.length + 2);

		/* allow read_value handler to read the value body */
		request->parse = mmc_server_read_value;

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_server_read_value(mmc_t *mmc, mmc_request_t *request) /* 
	read the value body into the buffer {{{ */
{
	mmc_ascii_request_t *req = (mmc_ascii_request_t *)request;
	request->readbuf.idx += 
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length + 2 - request->readbuf.idx);

	/* done reading? */
	if (request->readbuf.idx >= req->value.length + 2) {
		int result;

		/* allow parse_value to read next VALUE or END line */
		request->parse = mmc_request_parse_value;
		mmc_buffer_reset(&(request->readbuf));

		result = mmc_unpack_value(
			mmc, request, &(request->readbuf), req->value.key, strlen(req->value.key), 
			req->value.flags, req->value.cas, req->value.length);
		
		/* request more data (END line) */
		if (result == MMC_REQUEST_DONE) {
			return MMC_REQUEST_AGAIN;
		}
		
		return result;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static mmc_request_t *mmc_ascii_create_request() /* {{{ */ 
{
	mmc_ascii_request_t *request = emalloc(sizeof(mmc_ascii_request_t));
	ZEND_SECURE_ZERO(request, sizeof(*request));
	return (mmc_request_t *)request;
}
/* }}} */

static void mmc_ascii_clone_request(mmc_request_t *clone, mmc_request_t *request) /* {{{ */ 
{}
/* }}} */

static void mmc_ascii_reset_request(mmc_request_t *request) /* {{{ */ 
{
	mmc_ascii_request_t *req = (mmc_ascii_request_t *)request;
	req->value.cas = 0;
	mmc_request_reset(request);
}
/* }}} */

static void mmc_ascii_begin_get(mmc_request_t *request, int op) /* {{{ */
{
	request->parse = mmc_request_parse_value;

	if (op == MMC_OP_GETS) {
		smart_string_appendl(&(request->sendbuf.value), "gets", sizeof("gets")-1);
	}
	else {
		smart_string_appendl(&(request->sendbuf.value), "get", sizeof("get")-1);
	}
}
/* }}} */

static void mmc_ascii_append_get(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len) /* {{{ */
{
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
}
/* }}} */

static void mmc_ascii_end_get(mmc_request_t *request) /* {{{ */ 
{
	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
}
/* }}} */

static void mmc_ascii_get(mmc_request_t *request, int op, zval *zkey, const char *key, unsigned int key_len) /* {{{ */ 
{
	mmc_ascii_begin_get(request, op);
	mmc_ascii_append_get(request, zkey, key, key_len);
	mmc_ascii_end_get(request);
}
/* }}} */

static int mmc_ascii_store(
	mmc_pool_t *pool, mmc_request_t *request, int op, const char *key, unsigned int key_len, 
	unsigned int flags, unsigned int exptime, unsigned long cas, zval *value) /* {{{ */ 
{
	int status;
	mmc_buffer_t buffer;
	request->parse = mmc_request_parse_response;

	ZEND_SECURE_ZERO(&buffer, sizeof(buffer));
	status = mmc_pack_value(pool, &buffer, value, &flags);
	
	if (status != MMC_OK) {
		return status;
	}
	
	switch (op) {
		case MMC_OP_SET:
			smart_string_appendl(&(request->sendbuf.value), "set", sizeof("set")-1);
			break;
		case MMC_OP_ADD:
			smart_string_appendl(&(request->sendbuf.value), "add", sizeof("add")-1);
			break;
		case MMC_OP_REPLACE:
			smart_string_appendl(&(request->sendbuf.value), "replace", sizeof("replace")-1);
			break;
		case MMC_OP_CAS:
			smart_string_appendl(&(request->sendbuf.value), "cas", sizeof("cas")-1);
			break;
		case MMC_OP_APPEND:
			smart_string_appendl(&(request->sendbuf.value), "append", sizeof("append")-1);
			break;
		case MMC_OP_PREPEND:
			smart_string_appendl(&(request->sendbuf.value), "prepend", sizeof("prepend")-1);
			break;
		default:
			return MMC_REQUEST_FAILURE;
	}
	
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_append_unsigned(&(request->sendbuf.value), flags);
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_append_unsigned(&(request->sendbuf.value), exptime);
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_append_unsigned(&(request->sendbuf.value), buffer.value.len);

	if (op == MMC_OP_CAS) {
		smart_string_appendl(&(request->sendbuf.value), " ", 1);
		smart_string_append_unsigned(&(request->sendbuf.value), cas);
	}
	
	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
	smart_string_appendl(&(request->sendbuf.value), buffer.value.c, buffer.value.len);
	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
	
	mmc_buffer_free(&buffer);	
	return MMC_OK;
}
/* }}} */

static void mmc_ascii_delete(mmc_request_t *request, const char *key, unsigned int key_len, unsigned int exptime) /* {{{ */ 
{
	request->parse = mmc_request_parse_response;
	
	smart_string_appendl(&(request->sendbuf.value), "delete", sizeof("delete")-1);
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
	
	if (exptime > 0) {
		smart_string_appendl(&(request->sendbuf.value), " ", 1);
		smart_string_append_unsigned(&(request->sendbuf.value), exptime);
	}

	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
}
/* }}} */

static void mmc_ascii_mutate(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len, long value, long defval, int defval_used, unsigned int exptime) /* {{{ */
{
	request->parse = mmc_request_parse_mutate;
	
	if (value >= 0) {
		smart_string_appendl(&(request->sendbuf.value), "incr", sizeof("incr")-1);
	}
	else {
		smart_string_appendl(&(request->sendbuf.value), "decr", sizeof("decr")-1);
	}
	
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
	smart_string_appendl(&(request->sendbuf.value), " ", 1);
	smart_string_append_unsigned(&(request->sendbuf.value), value >= 0 ? value : -value);
	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
}
/* }}} */

static void mmc_ascii_flush(mmc_request_t *request, unsigned int exptime) /* {{{ */
{
	request->parse = mmc_request_parse_response;
	smart_string_appendl(&(request->sendbuf.value), "flush_all", sizeof("flush_all")-1);

	if (exptime > 0) {
		smart_string_appendl(&(request->sendbuf.value), " ", 1);
		smart_string_append_unsigned(&(request->sendbuf.value), exptime);
	}

	smart_string_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
}
/* }}} */

static void mmc_ascii_version(mmc_request_t *request) /* {{{ */ 
{
	request->parse = mmc_request_parse_response;
	smart_string_appendl(&(request->sendbuf.value), "version\r\n", sizeof("version\r\n")-1);
}
/* }}} */

static void mmc_ascii_stats(mmc_request_t *request, const char *type, long slabid, long limit) /* {{{ */
{
	char *cmd;
	unsigned int cmd_len;
	request->parse = mmc_request_parse_response;
	
	if (slabid) {
		cmd_len = spprintf(&cmd, 0, "stats %s %ld %ld\r\n", type, slabid, limit);
	}
	else if (type) {
		cmd_len = spprintf(&cmd, 0, "stats %s\r\n", type);
	}
	else {
		cmd_len = spprintf(&cmd, 0, "stats\r\n");
	}
	
	smart_string_appendl(&(request->sendbuf.value), cmd, cmd_len);
	efree(cmd);
}
/* }}} */

static void mmc_set_sasl_auth_data(mmc_pool_t *pool, mmc_request_t *request, const char *user,  const char *password) /* {{{ */
{
	/* stats not supported */
}
/* }}} */

mmc_protocol_t mmc_ascii_protocol = {
	mmc_ascii_create_request,
	mmc_ascii_clone_request,
	mmc_ascii_reset_request,
	mmc_request_free,
	mmc_ascii_get,
	mmc_ascii_begin_get,
	mmc_ascii_append_get,
	mmc_ascii_end_get,
	mmc_ascii_store,
	mmc_ascii_delete,
	mmc_ascii_mutate,
	mmc_ascii_flush,
	mmc_ascii_version,
	mmc_ascii_stats,
	mmc_set_sasl_auth_data
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
