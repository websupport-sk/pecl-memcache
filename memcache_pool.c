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

#include <zlib.h>
#include <arpa/inet.h>

#include "php.h"
#include "php_network.h"
#include "ext/standard/crc32.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"
#include "memcache_pool.h"

ZEND_DECLARE_MODULE_GLOBALS(memcache)

static void mmc_pool_switch(mmc_pool_t *);

static inline void mmc_buffer_alloc(mmc_buffer_t *buffer, unsigned int size)  /* {{{ */
{
	register size_t newlen;
	smart_str_alloc((&(buffer->value)), size, 0);
}
/* }}} */

static inline void mmc_buffer_free(mmc_buffer_t *buffer)  /* {{{ */
{
	if (buffer->value.c != NULL) {
		smart_str_free(&(buffer->value));
	}
	memset(buffer, 0, sizeof(*buffer));
}
/* }}} */

inline void mmc_queue_push(mmc_queue_t *queue, void *ptr) {
	if (queue->len >= queue->alloc) {
		int increase = 1 + MMC_QUEUE_PREALLOC;
		queue->alloc += increase;
		queue->items = erealloc(queue->items, sizeof(*queue->items) * queue->alloc);
		
		/* move tail segment downwards */
		if (queue->head < queue->tail) {
			memmove(queue->items + queue->tail + increase, queue->items + queue->tail, (queue->alloc - queue->tail - increase) * sizeof(*queue->items));
			queue->tail += increase;
		}
	}

	if (queue->len) {
		queue->head++;

		if (queue->head >= queue->alloc) {
			queue->head = 0;
		}
	}

	queue->items[queue->head] = ptr;
	queue->len++;
}

static inline void *mmc_queue_pop(mmc_queue_t *queue) {
	if (queue->len) {
		void *ptr;
		
		ptr = queue->items[queue->tail];
		queue->len--;

		if (queue->len) {
			queue->tail++;
	
			if (queue->tail >= queue->alloc) {
				queue->tail = 0;
			}
		}
		
		return ptr;
	}
	return NULL;
}

static inline void mmc_queue_free(mmc_queue_t *queue) {
	if (queue->items != NULL) {
		efree(queue->items);
	}
	memset(queue, 0, sizeof(*queue));
}

static inline void mmc_queue_remove(mmc_queue_t *queue, void *ptr) { 
	void *item;
	mmc_queue_t original = *queue;
	mmc_queue_release(queue);
	
	while ((item = mmc_queue_pop(&original)) != NULL) {
		if (item != ptr) {
			mmc_queue_push(queue, item);
		}
	}
	
	mmc_queue_free(&original);
}

static unsigned int mmc_hash_crc32(const char *key, int key_len) /* 
	CRC32 hash {{{ */
{
	unsigned int crc = ~0;
	int i;

	for (i=0; i<key_len; i++) {
		CRC32(crc, key[i]);
	}

  	return ~crc;
}
/* }}} */

static unsigned int mmc_hash_fnv1a(const char *key, int key_len) /* 
	FNV-1a hash {{{ */
{
	unsigned int hval = FNV_32_INIT;
	int i;

	for (i=0; i<key_len; i++) {
		hval ^= (unsigned int)key[i];
		hval *= FNV_32_PRIME;
    }

    return hval;
}
/* }}} */

static size_t mmc_stream_read_buffered(mmc_stream_t *io, char *buf, size_t count TSRMLS_DC) /* 
	attempts to reads count bytes from the stream buffer {{{ */
{
	size_t toread = io->buffer.value.len - io->buffer.idx < count ? io->buffer.value.len - io->buffer.idx : count;
	memcpy(buf, io->buffer.value.c + io->buffer.idx, toread);
	io->buffer.idx += toread;
	return toread;
}
/* }}} */

static char *mmc_stream_readline_buffered(mmc_stream_t *io, char *buf, size_t maxlen, size_t *retlen TSRMLS_DC)  /* 
	reads count bytes from the stream buffer, this implementation only detects \r\n (like memcached sends) {{{ */
{
	char *eol;
	
	eol = memchr(io->buffer.value.c + io->buffer.idx, '\n', io->buffer.value.len - io->buffer.idx);
	if (eol != NULL) {
		*retlen = eol - io->buffer.value.c - io->buffer.idx + 1;
	}
	else {
		*retlen = io->buffer.value.len - io->buffer.idx;
	}
	
	/* ensure space for data + \0 */
	if (*retlen >= maxlen) {
		*retlen = maxlen - 1;
	}
	
	memcpy(buf, io->buffer.value.c + io->buffer.idx, *retlen);
	io->buffer.idx += *retlen;
	buf[*retlen] = '\0';
	
	return buf;
}
/* }}} */

static size_t mmc_stream_read_wrapper(mmc_stream_t *io, char *buf, size_t count TSRMLS_DC)  /* {{{ */
{
	return php_stream_read(io->stream, buf, count);
}
/* }}} */

static char *mmc_stream_readline_wrapper(mmc_stream_t *io, char *buf, size_t maxlen, size_t *retlen TSRMLS_DC)  /* {{{ */
{
	return php_stream_get_line(io->stream, ZSTR(buf), maxlen, retlen);
}
/* }}} */

static int mmc_stream_get_line(mmc_stream_t *io, char **line TSRMLS_DC) /* 
	attempts to read a line from server, returns the line size or 0 if no complete line was available {{{ */
{
	size_t returned_len = 0;
	io->readline(io, io->input.value + io->input.idx, MMC_BUFFER_SIZE - io->input.idx, &returned_len TSRMLS_CC);
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

static inline mmc_request_t *mmc_request_new() /* {{{ */
{
	mmc_request_t *request = emalloc(sizeof(mmc_request_t));
	memset(request, 0, sizeof(*request));
	return request;
}
/* }}} */

static inline void mmc_request_free(mmc_request_t *request)  /* {{{ */
{
	mmc_buffer_free(&(request->sendbuf));
	mmc_buffer_free(&(request->readbuf));
	efree(request);
}
/* }}} */

static inline int mmc_request_send(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* {{{ */
{
	/* send next chunk of buffer */
	request->sendbuf.idx += php_stream_write(
		request->io->stream, request->sendbuf.value.c + request->sendbuf.idx, 
		request->sendbuf.value.len - request->sendbuf.idx);
	
	/* done sending? */
	if (request->sendbuf.idx >= request->sendbuf.value.len) {
		return MMC_REQUEST_DONE;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}} */

static int mmc_request_read_udp(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads an entire datagram into buffer and validates the udp header {{{ */
{
	size_t bytes;
	mmc_udp_header_t *header;
	
	/* reset buffer if completely consumed */
	if (request->io->buffer.idx >= request->io->buffer.value.len) {
		mmc_buffer_reset(&(request->io->buffer));
	}
	
	/* attempt to read datagram + sentinel-byte */
	mmc_buffer_alloc(&(request->io->buffer), MMC_MAX_UDP_LEN + 1);
	bytes = php_stream_read(request->io->stream, request->io->buffer.value.c + request->io->buffer.value.len, MMC_MAX_UDP_LEN + 1);
	
	if (bytes < sizeof(mmc_udp_header_t)) {
		return mmc_server_failure(mmc, request->io, "Failed te read complete UDP header from stream", 0 TSRMLS_CC);
	}
	if (bytes > MMC_MAX_UDP_LEN) {
		return mmc_server_failure(mmc, request->io, "Server sent packet larger than MMC_MAX_UDP_LEN bytes", 0 TSRMLS_CC);
	}
	
	header = (mmc_udp_header_t *)(request->io->buffer.value.c + request->io->buffer.value.len);
	
	/* initialize udp header fields */
	if (!request->udp.total) {
		request->udp.seqid = ntohs(header->seqid);
		request->udp.total = ntohs(header->total);
	}
	
	/* detect dropped packets and reschedule for tcp delivery */
	if (request->udp.reqid != ntohs(header->reqid) || request->udp.seqid != ntohs(header->seqid)) {
		/* ensure that no more udp requests are scheduled for a while */
		request->io->status = MMC_STATUS_FAILED;
		request->io->failed = (long)time(NULL);
		
		/* discard packets for previous requests */
		if (ntohs(header->reqid) < request->udp.reqid) {
			return MMC_REQUEST_MORE;
		}
		
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "UDP packet loss, expected %d:%d got %d:%d",
			(int)request->udp.reqid, (int)request->udp.seqid, (int)ntohs(header->reqid), (int)ntohs(header->seqid));
		return MMC_REQUEST_RETRY;
	}
	
	request->udp.seqid++;
	
	/* skip udp header */
	if (request->io->buffer.idx > 0) {
		memmove(
			request->io->buffer.value.c + request->io->buffer.value.len, 
			request->io->buffer.value.c + request->io->buffer.value.len + sizeof(mmc_udp_header_t),
			bytes - sizeof(mmc_udp_header_t));
	}
	else {
		request->io->buffer.idx += sizeof(mmc_udp_header_t);
	}
	
	request->io->buffer.value.len += bytes;
	return MMC_OK;
}
/* }}} */

static int mmc_compress(const char *data, unsigned long data_len, char **result, unsigned long *result_len) /* 
	compresses value into a buffer, returns MMC_OK on success {{{ */
{
	int status;
	*result_len = data_len + (data_len / 1000) + 25 + 1; /* some magic from zlib.c */
	*result = emalloc(*result_len);
	
	if (MMC_COMPRESSION_LEVEL >= 0) {
		status = compress2((unsigned char *)*result, result_len, (unsigned const char *)data, data_len, MMC_COMPRESSION_LEVEL);
	} else {
		status = compress((unsigned char *)*result, result_len, (unsigned const char *)data, data_len);
	}

	if (status == Z_OK) {
		return MMC_OK;
	}

	efree(*result);
	return MMC_REQUEST_FAILURE;
}
/* }}}*/

static int mmc_uncompress(const char *data, unsigned long data_len, char **result, unsigned long *result_len) /* {{{ */
{
	int status, factor = 1;

	do {
		*result_len = data_len * (1 << factor++);
		*result = (char *)erealloc(*result, *result_len + 1);
		status = uncompress((unsigned char *)*result, result_len, (unsigned const char *)data, data_len);
	} while (status == Z_BUF_ERROR && factor < 16);

	if (status == Z_OK) {
		return MMC_OK;
	}

	efree(*result);
	return MMC_REQUEST_FAILURE;
}
/* }}}*/

int mmc_prepare_store(
	mmc_pool_t *pool, mmc_request_t *request, const char *cmd, unsigned int cmd_len,
	const char *key, unsigned int key_len, unsigned int flags, unsigned int exptime, zval *value TSRMLS_DC) /* 
	assembles a SET/ADD/REPLACE request does into the buffer, returns MMC_OK on success {{{ */
{
	char *data = NULL;
	unsigned int data_len, data_free = 0;

	if (mmc_prepare_key_ex(key, key_len, request->key, &(request->key_len)) != MMC_OK) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid key");
		return MMC_REQUEST_FAILURE;
	}

	switch (Z_TYPE_P(value)) {
		case IS_STRING:
			data = Z_STRVAL_P(value);
			data_len = Z_STRLEN_P(value);
			flags &= ~MMC_SERIALIZED;
			break;

		case IS_LONG:
		case IS_DOUBLE:
		case IS_BOOL:
			convert_to_string(value);
			data = Z_STRVAL_P(value);
			data_len = Z_STRLEN_P(value);
			flags &= ~MMC_SERIALIZED;
			break;

		default: {
			php_serialize_data_t value_hash;
			zval value_copy, *value_copy_ptr;
			smart_str buf = {0};
			
			/* FIXME: we should be using 'Z' instead of this, but unfortunately it's PHP5-only */
			value_copy = *value;
			zval_copy_ctor(&value_copy);
			value_copy_ptr = &value_copy;

			/* TODO: serialize straight into buffer (voids auto-compression) */
			PHP_VAR_SERIALIZE_INIT(value_hash);
			php_var_serialize(&buf, &value_copy_ptr, &value_hash TSRMLS_CC);
			PHP_VAR_SERIALIZE_DESTROY(value_hash);

			/* you're trying to save null or something went really wrong */
			if (buf.c == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to serialize value");
				zval_dtor(&value_copy);
				return MMC_REQUEST_FAILURE;
			}

			data = buf.c;
			data_len = buf.len;
			data_free = 1;
			flags |= MMC_SERIALIZED;
			
			zval_dtor(&value_copy);
		}
	}
	
	/* autocompress large values */
	if (pool->compress_threshold && data_len >= pool->compress_threshold) {
		flags |= MMC_COMPRESSED;
	}

	if (flags & MMC_COMPRESSED) {
		char *result;
		unsigned long result_len;
		
		/* TODO: compress straight into buffer */
		if (mmc_compress(data, data_len, &result, &result_len) == MMC_OK) {
			if (result_len < data_len * (1 - pool->min_compress_savings)) {
				if (data_free) {
					efree(data);
				}
				
				data = result;
				data_len = result_len;
				data_free = 1;
			}
			else {
				efree(result);
				flags &= ~MMC_COMPRESSED;
			}
		}
		else {
			flags &= ~MMC_COMPRESSED;
		}
	}

	/* assemble command */
	smart_str_appendl(&(request->sendbuf.value), cmd, cmd_len);
	smart_str_appendl(&(request->sendbuf.value), " ", 1);
	smart_str_appendl(&(request->sendbuf.value), request->key, request->key_len);
	smart_str_appendl(&(request->sendbuf.value), " ", 1);
	smart_str_append_unsigned(&(request->sendbuf.value), flags);
	smart_str_appendl(&(request->sendbuf.value), " ", 1);
	smart_str_append_unsigned(&(request->sendbuf.value), exptime);
	smart_str_appendl(&(request->sendbuf.value), " ", 1);
	smart_str_append_unsigned(&(request->sendbuf.value), data_len);
	smart_str_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);

	smart_str_appendl(&(request->sendbuf.value), data, data_len);
	smart_str_appendl(&(request->sendbuf.value), "\r\n", sizeof("\r\n")-1);
	
	if (data_free) {
		efree(data);
	}

	return MMC_OK;
}
/* }}} */

static int mmc_unpack_value(mmc_t *mmc, mmc_request_t *request, mmc_buffer_t *buffer TSRMLS_DC) /* 
	does uncompression and unserializing to construct a zval {{{ */
{
	char *data = NULL;
	unsigned long data_len;

	zval value;
	INIT_ZVAL(value);
	
	if (request->value.flags & MMC_COMPRESSED) {
		if (mmc_uncompress(buffer->value.c, request->value.bytes, &data, &data_len) != MMC_OK) {
			return mmc_server_failure(mmc, request->io, "Failed to uncompress data", 0 TSRMLS_CC);
		}
	}
	else {
		data = buffer->value.c;
		data_len = request->value.bytes;
	}

	if (request->value.flags & MMC_SERIALIZED) {
		php_unserialize_data_t var_hash;
		const unsigned char *p = (unsigned char *)data;
		zval *object = &value;  
		
		PHP_VAR_UNSERIALIZE_INIT(var_hash);
		if (!php_var_unserialize(&object, &p, p + data_len, &var_hash TSRMLS_CC)) {
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
			if (request->value.flags & MMC_COMPRESSED) {
				efree(data);
			}
			return mmc_server_failure(mmc, request->io, "Failed to unserialize data", 0 TSRMLS_CC);
		}

		PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		if (request->value.flags & MMC_COMPRESSED) {
			efree(data);
		}

		/* delegate to value handler */
		return request->value_handler(mmc, request, object, 0, request->value_handler_param TSRMLS_CC);
	}
	else {
		/* room for \0 since buffer contains trailing \r\n and uncompress allocates + 1 */
		data[data_len] = '\0';
		ZVAL_STRINGL(&value, data, data_len, 0);
		
		if (!(request->value.flags & MMC_COMPRESSED)) {
			mmc_buffer_release(buffer);
		}

		/* delegate to value handler */
		return request->value_handler(mmc, request, &value, 0, request->value_handler_param TSRMLS_CC);
	}
}
/* }}}*/

static int mmc_server_read_value(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	read the value body into the buffer {{{ */
{
	request->readbuf.idx += 
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, request->value.bytes + 2 - request->readbuf.idx TSRMLS_CC);

	/* done reading? */
	if (request->readbuf.idx >= request->value.bytes + 2) {
		int result = mmc_unpack_value(mmc, request, &(request->readbuf) TSRMLS_CC);
		
		/* allow parse_value to read next VALUE or END line */
		request->parse = mmc_request_parse_value;
		mmc_buffer_reset(&(request->readbuf));

		return result;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

int mmc_request_parse_line(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads a single line and delegates it to value_handler {{{ */
{
	char *line;
	int line_len = mmc_stream_get_line(request->io, &line TSRMLS_CC);
	
	if (line_len > 0) {
		return request->value_handler(mmc, request, line, line_len, request->value_handler_param TSRMLS_CC);
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

int mmc_request_parse_value(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the VALUE <key> <flags> <size> header and then reads the value body {{{ */
{
	char *line;
	int line_len = mmc_stream_get_line(request->io, &line TSRMLS_CC);
	
	if (line_len > 0) {
		if (mmc_str_left(line, "END", line_len, sizeof("END")-1)) {
			return MMC_REQUEST_DONE;
		}
		if (sscanf(line, MMC_VALUE_HEADER, request->value.key, &(request->value.flags), &(request->value.bytes)) != 3) {
			return mmc_server_failure(mmc, request->io, "Malformed VALUE header", 0 TSRMLS_CC);
		}
		
		request->value.key_len = strlen(request->value.key);
		
		/* memory for data + \r\n */
		mmc_buffer_alloc(&(request->readbuf), request->value.bytes + 2);

		/* allow read_value handler to read the value body */
		request->parse = mmc_server_read_value;

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

mmc_t *mmc_server_new(
	const char *host, int host_len, unsigned short tcp_port, unsigned short udp_port, 
	int persistent, int timeout, int retry_interval TSRMLS_DC) /* {{{ */
{
	mmc_t *mmc = pemalloc(sizeof(mmc_t), persistent);
	memset(mmc, 0, sizeof(*mmc));

	mmc->host = pemalloc(host_len + 1, persistent);
	memcpy(mmc->host, host, host_len);
	mmc->host[host_len] = '\0';

	mmc->tcp.port = tcp_port;
	mmc->tcp.status = MMC_STATUS_DISCONNECTED;
	mmc->udp.port = udp_port;
	mmc->udp.status = MMC_STATUS_DISCONNECTED;
	
	mmc->persistent = persistent;
	mmc->timeout = timeout;
	
	mmc->tcp.retry_interval = retry_interval;
	mmc->tcp.chunk_size = MEMCACHE_G(chunk_size);
	mmc->udp.retry_interval = retry_interval;
	mmc->udp.chunk_size = MEMCACHE_G(chunk_size); /* = MMC_MAX_UDP_LEN;*/

	return mmc;
}
/* }}} */

void mmc_server_disconnect(mmc_t *mmc, mmc_stream_t *io TSRMLS_DC) /* {{{ */
{
	mmc_buffer_free(&(io->buffer));	

	if (io->stream != NULL) {
		if (mmc->persistent) {
			php_stream_pclose(io->stream);
		}
		else {
			php_stream_close(io->stream);
		}
		io->stream = NULL;
	}

	io->status = MMC_STATUS_DISCONNECTED;
}
/* }}} */

static void mmc_server_seterror(mmc_t *mmc, const char *error, int errnum) /* {{{ */
{
	if (error != NULL) {
		if (mmc->error != NULL) {
			efree(mmc->error);
		}
		
		mmc->error = estrdup(error);
		mmc->errnum = errnum;
	}
}
/* }}} */

static void mmc_server_deactivate(mmc_pool_t *pool, mmc_t *mmc TSRMLS_DC) /* 
	disconnect and marks the server as down, failovers all queued requests {{{ */
{
	mmc_queue_t readqueue;
	mmc_request_t *request;
	
	mmc_server_disconnect(mmc, &(mmc->tcp) TSRMLS_CC);
	mmc_server_disconnect(mmc, &(mmc->udp) TSRMLS_CC);
	
	mmc->tcp.status = MMC_STATUS_FAILED;
	mmc->udp.status = MMC_STATUS_FAILED;
	mmc->tcp.failed = (long)time(NULL);
	mmc->udp.failed = mmc->tcp.failed;
	
	mmc_queue_remove(pool->sending, mmc);
	mmc_queue_remove(pool->reading, mmc);
	
	/* failover queued requests, sendque can be ignored since 
	 * readque + readreq + buildreq will always contain all active requests */
	mmc_queue_reset(&(mmc->sendqueue));
	mmc->sendreq = NULL;

	readqueue = mmc->readqueue;
	mmc_queue_release(&(mmc->readqueue));
	
	if (mmc->readreq != NULL) {
		mmc_queue_push(&readqueue, mmc->readreq);
		mmc->readreq = NULL;
	}
	
	if (mmc->buildreq != NULL) {
		mmc_queue_push(&readqueue, mmc->buildreq);
		mmc->buildreq = NULL;
	}

	/* delegate to failover handlers */
	while ((request = mmc_queue_pop(&readqueue)) != NULL) {
		request->failover_handler(pool, request, request->failover_handler_param TSRMLS_CC);
	}
	
	mmc_queue_free(&readqueue);

	if (mmc->failure_callback != NULL) {
		zval *retval;
		zval *host, *tcp_port, *udp_port, *error, *errnum;
		zval **params[5] = {&host, &tcp_port, &udp_port, &error, &errnum};

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

		call_user_function_ex(EG(function_table), NULL, mmc->failure_callback, &retval, 5, params, 0, NULL TSRMLS_CC);

		zval_ptr_dtor(&host);
		zval_ptr_dtor(&tcp_port); zval_ptr_dtor(&udp_port);
		zval_ptr_dtor(&error); zval_ptr_dtor(&errnum);
		zval_ptr_dtor(&retval);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Server %s (tcp %d, udp %d) failed with: %s (%d)", 
			mmc->host, mmc->tcp.port, mmc->udp.port, mmc->error, mmc->errnum);
	}
}
/* }}} */

int mmc_server_failure(mmc_t *mmc, mmc_stream_t *io, const char *error, int errnum TSRMLS_DC) /* 
	determines if a request should be retried or is a hard network failure {{{ */
{
	switch (io->status) {
		case MMC_STATUS_DISCONNECTED:
			return MMC_REQUEST_RETRY;

		/* attempt reconnect of sockets in unknown state */
		case MMC_STATUS_UNKNOWN:
			io->status = MMC_STATUS_DISCONNECTED;
			return MMC_REQUEST_RETRY;
	}
	
	mmc_server_seterror(mmc, error, errnum);
	return MMC_REQUEST_FAILURE;
}
/* }}} */

int mmc_request_failure(mmc_t *mmc, mmc_stream_t *io, char *value, unsigned int value_len, int errnum TSRMLS_DC) /* 
	 checks for a valid server generated error message and calls mmc_server_failure() {{{ */
{
	if (mmc_str_left(value, "ERROR", value_len, sizeof("ERROR")-1) ||
		mmc_str_left(value, "CLIENT_ERROR", value_len, sizeof("CLIENT_ERROR")-1) ||
		mmc_str_left(value, "SERVER_ERROR", value_len, sizeof("SERVER_ERROR")-1)) 
	{
		// Trim off trailing \r\n
		value[value_len - 2] = '\0';
		return mmc_server_failure(mmc, io, value, errnum TSRMLS_CC);
	}
	
	return mmc_server_failure(mmc, io, "Malformed server response", errnum TSRMLS_CC);
}
/* }}} */

static int mmc_server_connect(mmc_pool_t *pool, mmc_t *mmc, mmc_stream_t *io, int udp TSRMLS_DC) /* 
	connects a stream, calls mmc_server_deactivate() on failure {{{ */
{
	char *host, *hash_key = NULL, *errstr = NULL;
	int	host_len, errnum = 0;
	struct timeval tv;

	/* close open stream */
	if (io->stream != NULL) {
		mmc_server_disconnect(mmc, io TSRMLS_CC);
	}

	if (mmc->persistent) {
		spprintf(&hash_key, 0, "memcache:stream:%s:%u:%d", mmc->host, io->port, udp);
	}

	tv.tv_sec = mmc->timeout;
	tv.tv_usec = 0;

#if PHP_API_VERSION > 20020918

	if (udp) {
		host_len = spprintf(&host, 0, "udp://%s:%u", mmc->host, io->port);
	}
	else if (io->port) {
		host_len = spprintf(&host, 0, "%s:%u", mmc->host, io->port);
	}
	else {
		host_len = spprintf(&host, 0, "%s", mmc->host);
	}

	io->stream = php_stream_xport_create(
		host, host_len,
		ENFORCE_SAFE_MODE | (mmc->persistent ? STREAM_OPEN_PERSISTENT : 0), 
		STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
		hash_key, &tv, NULL, &errstr, &errnum);

	efree(host);

#else

	if (mmc->persistent) {
		switch(php_stream_from_persistent_id(hash_key, &(io->stream) TSRMLS_CC)) {
			case PHP_STREAM_PERSISTENT_SUCCESS:
				if (php_stream_eof(io->stream)) {
					php_stream_pclose(io->stream);
					io->stream = NULL;
					break;
				}
			case PHP_STREAM_PERSISTENT_FAILURE:
				break;
		}
	}

	if (!io->stream) {
		if (io->port) {
			io->stream = php_stream_sock_open_host(mmc->host, io->port, udp ? SOCK_DGRAM : SOCK_STREAM, &tv, hash_key);
		}
		else if (strncasecmp("unix://", mmc->host, sizeof("unix://")-1) == 0 && strlen(mmc->host) > sizeof("unix://")-1) {
			io->stream = php_stream_sock_open_unix(mmc->host + sizeof("unix://")-1, strlen(mmc->host + sizeof("unix://")-1), hash_key, &tv);
		}
		else {
			io->stream = php_stream_sock_open_unix(mmc->host, strlen(mmc->host), hash_key, &tv);
		}
	}

#endif

	if (hash_key != NULL) {
		efree(hash_key);
	}

	/* check connection and extract socket for select() purposes */
	if (!io->stream || php_stream_cast(io->stream, PHP_STREAM_AS_FD_FOR_SELECT, (void **)&(io->fd), 1) != SUCCESS) {
		mmc_server_seterror(mmc, errstr, errnum);
		mmc_server_deactivate(pool, mmc TSRMLS_CC);

		if (errstr != NULL) {
			efree(errstr);
		}
		
		return MMC_REQUEST_FAILURE;
	}

	io->status = MMC_STATUS_CONNECTED;
	
	php_stream_auto_cleanup(io->stream);
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &tv);
	
	/* php_stream buffering prevent us from detecting datagram boundaries when using udp */ 
	if (udp) { 
		io->read = mmc_stream_read_buffered;
		io->readline = mmc_stream_readline_buffered;
		php_stream_set_option(io->stream, PHP_STREAM_OPTION_READ_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
	}
	else {
		io->read = mmc_stream_read_wrapper;
		io->readline = mmc_stream_readline_wrapper;
	}
	
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
	php_stream_set_chunk_size(io->stream, io->chunk_size);
	
	if (mmc->error != NULL) {
		efree(mmc->error);
		mmc->error = NULL;
	}
	
	return MMC_OK;
}
/* }}} */

static inline int mmc_server_valid(mmc_t *mmc TSRMLS_DC) /* 
	checks if a server should be considered valid to serve requests {{{ */
{
	if (mmc != NULL) {
		if (mmc->tcp.status >= MMC_STATUS_DISCONNECTED) {
			return 1;
		}

		if (mmc->tcp.status == MMC_STATUS_FAILED && 
			mmc->tcp.retry_interval >= 0 && (long)time(NULL) >= mmc->tcp.failed + mmc->tcp.retry_interval) {
			return 1;	
		}
	}
	
	return 0;
}
/* }}} */

/* mmc_server_set_failure_callback {{{ */
static void mmc_server_callback_dtor(zval *callback TSRMLS_DC)
{
	zval **this_obj;
	
	if (callback != NULL) {
		if (Z_TYPE_P(callback) == IS_ARRAY && 
			zend_hash_index_find(Z_ARRVAL_P(callback), 0, (void **)&this_obj) == SUCCESS &&
			Z_TYPE_PP(this_obj) == IS_OBJECT) {
			zval_ptr_dtor(this_obj);
		}
		zval_ptr_dtor(&callback);
	}
}

static void mmc_server_callback_ctor(zval *callback TSRMLS_DC)
{
	zval **this_obj;
	
	if (callback != NULL) {
		if (Z_TYPE_P(callback) == IS_ARRAY && 
			zend_hash_index_find(Z_ARRVAL_P(callback), 0, (void **)&this_obj) == SUCCESS &&
			Z_TYPE_PP(this_obj) == IS_OBJECT) {
			zval_add_ref(this_obj);
		}
		zval_add_ref(&callback);
	}
}

void mmc_server_set_failure_callback(mmc_t *mmc, zval *callback TSRMLS_DC)
{
	if (mmc->failure_callback != NULL) {
		mmc_server_callback_dtor(mmc->failure_callback TSRMLS_CC);	
	}
	
	mmc->failure_callback = callback;
	mmc_server_callback_ctor(mmc->failure_callback TSRMLS_CC);
}
/* }}} */

void mmc_server_sleep(mmc_t *mmc TSRMLS_DC) /* 
	prepare server struct for persistent sleep {{{ */
{
	mmc_buffer_free(&(mmc->tcp.buffer));
	mmc_buffer_free(&(mmc->udp.buffer));

	mmc->sendreq = NULL;
	mmc->readreq = NULL;
	mmc->buildreq = NULL;
	
	mmc_queue_free(&(mmc->sendqueue));
	mmc_queue_free(&(mmc->readqueue));
	
	mmc_server_callback_dtor(mmc->failure_callback TSRMLS_CC);
	mmc->failure_callback = NULL;
	
	if (mmc->error != NULL) {
		efree(mmc->error);
		mmc->error = NULL;
	}
}
/* }}} */

void mmc_server_free(mmc_t *mmc TSRMLS_DC) /* {{{ */
{
	if (mmc->in_free) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Recursive reference detected, bailing out");
		return;
	}
	mmc->in_free = 1;
	
	mmc_server_sleep(mmc TSRMLS_CC);
	mmc_server_disconnect(mmc, &(mmc->tcp) TSRMLS_CC);
	mmc_server_disconnect(mmc, &(mmc->udp) TSRMLS_CC);
	
	pefree(mmc->host, mmc->persistent);
	pefree(mmc, mmc->persistent);
}
/* }}} */

mmc_pool_t *mmc_pool_new(TSRMLS_D) /* {{{ */
{
	mmc_hash_function hash;
	mmc_pool_t *pool = emalloc(sizeof(mmc_pool_t));
	memset(pool, 0, sizeof(*pool));

	switch (MEMCACHE_G(hash_strategy)) {
		case MMC_CONSISTENT_HASH:
			pool->hash = &mmc_consistent_hash;
			break;
		default:
			pool->hash = &mmc_standard_hash;
	}

	switch (MEMCACHE_G(hash_function)) {
		case MMC_HASH_FNV1A:
			hash = &mmc_hash_fnv1a;
			break;
		default:
			hash = &mmc_hash_crc32;
	}
	
	pool->hash_state = pool->hash->create_state(hash);
	pool->min_compress_savings = MMC_DEFAULT_SAVINGS;
	
	pool->sending = &(pool->_sending1);
	pool->reading = &(pool->_reading1);

	return pool;
}
/* }}} */

void mmc_pool_free(mmc_pool_t *pool TSRMLS_DC) /* {{{ */
{
	int i;
	mmc_request_t *request;

	if (pool->in_free) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Recursive reference detected, bailing out");
		return;
	}
	pool->in_free = 1;

	for (i=0; i<pool->num_servers; i++) {
		if (pool->servers[i] != NULL) {
			if (pool->servers[i]->persistent) {
				mmc_server_sleep(pool->servers[i] TSRMLS_CC);
			} else {
				mmc_server_free(pool->servers[i] TSRMLS_CC);
			}
			pool->servers[i] = NULL;
		}
	}

	if (pool->num_servers) {
		efree(pool->servers);
	}

	pool->hash->free_state(pool->hash_state);
	
	mmc_queue_free(&(pool->_sending1));
	mmc_queue_free(&(pool->_sending2));
	mmc_queue_free(&(pool->_reading1));
	mmc_queue_free(&(pool->_reading2));
	mmc_queue_free(&(pool->pending));
	
	/* requests are owned by us so free them */
	while ((request = mmc_queue_pop(&(pool->free_requests))) != NULL) {
		mmc_request_free(request);
	}
	mmc_queue_free(&(pool->free_requests));
	
	efree(pool);
}
/* }}} */

void mmc_pool_add(mmc_pool_t *pool, mmc_t *mmc, unsigned int weight) /* 
	adds a server to the pool and hash strategy {{{ */
{
	pool->servers = erealloc(pool->servers, sizeof(mmc_t *) * (pool->num_servers + 1));
	pool->servers[pool->num_servers] = mmc;
	pool->num_servers++;

	pool->hash->add_server(pool->hash_state, mmc, weight);
}
/* }}} */

int mmc_pool_open(mmc_pool_t *pool, mmc_t *mmc, mmc_stream_t *io, int udp TSRMLS_DC) /* 
	connects if the stream is not already connected {{{ */
{
	switch (io->status) {
		case MMC_STATUS_CONNECTED:
		case MMC_STATUS_UNKNOWN:
			return MMC_OK;

		case MMC_STATUS_DISCONNECTED:
		case MMC_STATUS_FAILED:
			return mmc_server_connect(pool, mmc, io, udp TSRMLS_CC);
	}
	
	return MMC_REQUEST_FAILURE;
}
/* }}} */

int mmc_pool_failover_handler(mmc_pool_t *pool, mmc_request_t *request, void *param TSRMLS_DC) /* 
	uses request->key to reschedule request to other server {{{ */
{
	if (!MEMCACHE_G(allow_failover) || request->failures++ >= MEMCACHE_G(max_failover_attempts)) {
		mmc_pool_release(pool, request);
		return MMC_REQUEST_FAILURE;
	}
	return mmc_pool_schedule_key(pool, request->key, request->key_len, request TSRMLS_CC);
}
/* }}}*/

static int mmc_pool_failover_handler_null(mmc_pool_t *pool, mmc_request_t *request, void *param TSRMLS_DC) /* 
	always returns failure {{{ */
{
	mmc_pool_release(pool, request);
	return MMC_REQUEST_FAILURE;
}
/* }}}*/

mmc_request_t *mmc_pool_request(mmc_pool_t *pool, int protocol, mmc_request_parser parse, 
	mmc_request_value_handler value_handler, void *value_handler_param, 
	mmc_request_failover_handler failover_handler, void *failover_handler_param TSRMLS_DC) /* 
	allocates a request, must be added to pool using mmc_pool_schedule or released with mmc_pool_release {{{ */ 
{
	mmc_request_t *request = mmc_queue_pop(&(pool->free_requests));
	if (request == NULL) {
		request = mmc_request_new();
	}
	else {
		request->key_len = 0;
		mmc_buffer_reset(&(request->sendbuf));
	}
	
	request->parse = parse;
	request->value_handler = value_handler;
	request->value_handler_param = value_handler_param;
	
	request->failover_handler = failover_handler != NULL ? failover_handler : mmc_pool_failover_handler_null;
	request->failover_handler_param = failover_handler_param;

	request->protocol = protocol;
	
	if (protocol == MMC_PROTO_UDP) {
		mmc_udp_header_t header = {0};
		smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
	}
	
	return request;
}
/* }}} */

static int mmc_pool_slot_send(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, int handle_failover TSRMLS_DC) /* {{{ */
{
	if (request != NULL) {
		/* select protocol strategy and open connection */
		if (request->protocol == MMC_PROTO_UDP && mmc->udp.port && 
			request->sendbuf.value.len <= mmc->udp.chunk_size &&
			mmc_pool_open(pool, mmc, &(mmc->udp), 1 TSRMLS_CC) == MMC_OK) 
		{
			request->io = &(mmc->udp);
			request->read = mmc_request_read_udp;

			/* initialize udp header */
			request->udp.reqid = mmc->reqid++;
			request->udp.seqid = 0;
			request->udp.total = 0;

			((mmc_udp_header_t *)request->sendbuf.value.c)->reqid = htons(request->udp.reqid);
			((mmc_udp_header_t *)request->sendbuf.value.c)->total = htons(1);
		}
		else if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0 TSRMLS_CC) == MMC_OK) {
			/* skip udp header */
			if (request->protocol == MMC_PROTO_UDP) {
				request->sendbuf.idx += sizeof(mmc_udp_header_t);
			}
			
			request->io = &(mmc->tcp);
			request->read = NULL;
		}
		else {
			mmc->sendreq = NULL;
			if (handle_failover) {
				return request->failover_handler(pool, request, request->failover_handler_param TSRMLS_CC);
			}
			return MMC_REQUEST_FAILURE; 
		}
	}
	
	mmc->sendreq = request;
	return MMC_OK;
}
/* }}} */

int mmc_pool_schedule(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	schedules a request against a server, return MMC_OK on success {{{ */
{
	if (!mmc_server_valid(mmc TSRMLS_CC)) {
		/* delegate to failover handler if connect fails */
		return request->failover_handler(pool, request, request->failover_handler_param TSRMLS_CC);
	}
	
	/* reset sendbuf to start position */
	request->sendbuf.idx = 0;
	/* reset readbuf entirely */
	mmc_buffer_reset(&(request->readbuf));

	/* push request into sendreq slot if available */
	if (mmc->sendreq == NULL) {
		if (mmc_pool_slot_send(pool, mmc, request, 0 TSRMLS_CC) != MMC_OK) {
			return request->failover_handler(pool, request, request->failover_handler_param TSRMLS_CC);
		}
		mmc_queue_push(pool->sending, mmc);
	}
	else {
		mmc_queue_push(&(mmc->sendqueue), request);
	}

	/* push request into readreq slot if available */
	if (mmc->readreq == NULL) {
		mmc->readreq = request;
		mmc_queue_push(pool->reading, mmc);
	}
	else {
		mmc_queue_push(&(mmc->readqueue), request);
	}
	
	return MMC_OK;
}
/* }}} */

static mmc_t *mmc_pool_find(mmc_pool_t *pool, const char *key, unsigned int key_len TSRMLS_DC) {
	int i;
	char keytmp[MAX_LENGTH_OF_LONG + MMC_MAX_KEY_LEN + 1];
	unsigned int keytmp_len;
	mmc_t *mmc = pool->hash->find_server(pool->hash_state, key, key_len TSRMLS_CC);

	for (i=0; !mmc_server_valid(mmc TSRMLS_CC) && MEMCACHE_G(allow_failover) && i < MEMCACHE_G(max_failover_attempts); i++) {
		keytmp_len = sprintf(keytmp, "%s-%d", key, i);
		mmc = pool->hash->find_server(pool->hash_state, keytmp, keytmp_len TSRMLS_CC);
	}
	
	return mmc;
}
/* }}} */

int mmc_pool_schedule_key(mmc_pool_t *pool, const char *key, unsigned int key_len, mmc_request_t *request TSRMLS_DC) /* 
	schedules a request against a server selected by the provided key, return MMC_OK on success {{{ */
{
	return mmc_pool_schedule(pool, mmc_pool_find(pool, key, key_len TSRMLS_CC), request TSRMLS_CC);
}
/* }}} */

int mmc_pool_schedule_get(
	mmc_pool_t *pool, int protocol, const char *key, unsigned int key_len, 
	mmc_request_value_handler value_handler, void *value_handler_param,
	mmc_request_failover_handler failover_handler, void *failover_handler_param, 
	unsigned int failures TSRMLS_DC) /* 
	schedules a get command against a server {{{ */
{
	mmc_t *mmc = mmc_pool_find(pool, key, key_len TSRMLS_CC);
	if (!mmc_server_valid(mmc TSRMLS_CC)) {
		return MMC_REQUEST_FAILURE;
	}
	
	if (mmc->buildreq == NULL) {
		mmc_queue_push(&(pool->pending), mmc);
		
		mmc->buildreq = mmc_pool_request(pool, protocol, mmc_request_parse_value, 
			value_handler, value_handler_param, failover_handler, failover_handler_param TSRMLS_CC);
		mmc->buildreq->failures = failures;

		smart_str_appendl(&(mmc->buildreq->sendbuf.value), "get", sizeof("get")-1);
	}
	else if (protocol == MMC_PROTO_UDP && mmc->buildreq->sendbuf.value.len + key_len + 3 > MMC_MAX_UDP_LEN) {
		/* datagram if full, schedule for delivery */
		smart_str_appendl(&(mmc->buildreq->sendbuf.value), "\r\n", sizeof("\r\n")-1);
		mmc_pool_schedule(pool, mmc, mmc->buildreq TSRMLS_CC);

		/* begin sending requests immediatly */
		mmc_pool_select(pool, 0 TSRMLS_CC);
		
		mmc->buildreq = mmc_pool_request(pool, protocol, mmc_request_parse_value, 
			value_handler, value_handler_param, failover_handler, failover_handler_param TSRMLS_CC);
		mmc->buildreq->failures = failures;

		smart_str_appendl(&(mmc->buildreq->sendbuf.value), "get", sizeof("get")-1);
	}

	smart_str_appendl(&(mmc->buildreq->sendbuf.value), " ", 1);
	smart_str_appendl(&(mmc->buildreq->sendbuf.value), key, key_len);

	return MMC_OK;
}
/* }}} */

int mmc_pool_schedule_command(
	mmc_pool_t *pool, mmc_t *mmc, char *cmd, unsigned int cmd_len, mmc_request_parser parse, 
	mmc_request_value_handler value_handler, void *value_handler_param TSRMLS_DC) /* 
	schedules a single command against a server {{{ */
{
	mmc_request_t *request = mmc_pool_request(pool, MMC_PROTO_TCP, parse, value_handler, value_handler_param, NULL, NULL TSRMLS_CC);
	smart_str_appendl(&(request->sendbuf.value), cmd, cmd_len);
	
	if (mmc_pool_schedule(pool, mmc, request TSRMLS_CC) != MMC_OK) {
		return MMC_REQUEST_FAILURE;
	}
	
	return MMC_OK;
}
/* }}} */

static void mmc_pool_switch(mmc_pool_t *pool) {
	/* switch sending and reading queues */
	if (pool->sending == &(pool->_sending1)) {
		pool->sending = &(pool->_sending2);
		pool->reading = &(pool->_reading2);
	}
	else {
		pool->sending = &(pool->_sending1);
		pool->reading = &(pool->_reading1);
	}

	/* reset queues so they can be re-populated */
	mmc_queue_reset(pool->sending);
	mmc_queue_reset(pool->reading);
}

static void mmc_select_failure(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, int result TSRMLS_DC) /* {{{ */
{
	if (result == 0) {
		/* timeout expired, non-responsive server */
		if (mmc_server_failure(mmc, request->io, "Network timeout", 0 TSRMLS_CC) != MMC_REQUEST_RETRY) {
			mmc_server_deactivate(pool, mmc TSRMLS_CC);
		}
	}
	else {
		/* hard failure, deactivate connection */
		mmc_server_seterror(mmc, "Unknown select() error", errno);
		mmc_server_deactivate(pool, mmc TSRMLS_CC);
	}
}
/* }}} */

static void mmc_select_retry(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	removes request from send/read queues and calls failover {{{ */
{
	mmc_queue_remove(&(mmc->sendqueue), request);
	mmc_queue_remove(&(mmc->readqueue), request);

	if (mmc->sendreq == request) {
		mmc_pool_slot_send(pool, mmc, mmc_queue_pop(&(mmc->sendqueue)), 1 TSRMLS_CC);
	}
	
	if (mmc->readreq == request) {
		mmc->readreq = mmc_queue_pop(&(mmc->readqueue));
	}

	request->failover_handler(pool, request, request->failover_handler_param TSRMLS_CC);
}
/* }}} */

void mmc_pool_select(mmc_pool_t *pool, long timeout TSRMLS_DC) /* 
	runs one select() round on all scheduled requests {{{ */
{
	struct timeval tv;
	fd_set wfds, rfds;
	int i, nfds = 0, result;
	
	mmc_t *mmc;
	mmc_queue_t *sending = pool->sending, *reading = pool->reading;
	mmc_pool_switch(pool);
	
	FD_ZERO(&wfds);
	FD_ZERO(&rfds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	for (i=0; i<mmc_queue_len(sending); i++) {
		mmc = mmc_queue_item(sending, i);
		if (mmc->sendreq->io->fd > nfds) {
			nfds = mmc->sendreq->io->fd;
		}
		FD_SET(mmc->sendreq->io->fd, &wfds);
	}
	
	for (i=0; i<mmc_queue_len(reading); i++) {
		mmc = mmc_queue_item(reading, i);
		if (mmc->readreq->io->fd > nfds) {
			nfds = mmc->readreq->io->fd;
		}
		FD_SET(mmc->readreq->io->fd, &rfds);
	}

	result = select(nfds + 1, &rfds, &wfds, NULL, &tv);
	
	if (result <= 0) {
		for (i=0; i<mmc_queue_len(sending); i++) {
			mmc_select_failure(pool, mmc_queue_item(sending, i), ((mmc_t *)mmc_queue_item(sending, i))->sendreq, result TSRMLS_CC);
		}
		for (i=0; i<mmc_queue_len(reading); i++) {
			mmc_select_failure(pool, mmc_queue_item(reading, i), ((mmc_t *)mmc_queue_item(reading, i))->readreq, result TSRMLS_CC);
		}
	}
	
	/* until stream buffer is empty */
	for (i=0; i<mmc_queue_len(sending); i++) {
		mmc = mmc_queue_item(sending, i);
		
		if (FD_ISSET(mmc->sendreq->io->fd, &wfds)) {
			do {
				/* delegate to request parse handler */
				result = mmc_request_send(mmc, mmc->sendreq TSRMLS_CC);
				
				switch (result) {
					case MMC_REQUEST_FAILURE:
						/* clear out readbit for this round */
						FD_CLR(mmc->sendreq->io->fd, &rfds);

						/* take server offline and failover requests */
						mmc_server_deactivate(pool, mmc TSRMLS_CC);
						break;
						
					case MMC_REQUEST_RETRY:
						/* allow request to reschedule itself */
						mmc_select_retry(pool, mmc, mmc->sendreq TSRMLS_CC);
						break;
						
					case MMC_REQUEST_DONE:
						/* shift next request into send slot */
						mmc_pool_slot_send(pool, mmc, mmc_queue_pop(&(mmc->sendqueue)), 1 TSRMLS_CC);
						break;
					
					case MMC_REQUEST_MORE:
						break;

					default:
						php_error_docref(NULL TSRMLS_CC, E_ERROR, "Invalid return value, bailing out");
				}
			} while (mmc->sendreq != NULL && (result == MMC_REQUEST_DONE || result == MMC_REQUEST_AGAIN));

			if (result == MMC_REQUEST_MORE) {
				/* add server to read queue once more */
				mmc_queue_push(pool->sending, mmc);
			}
		}
		else {
			/* add server to send queue once more */
			mmc_queue_push(pool->sending, mmc);
		}
	}

	for (i=0; i<mmc_queue_len(reading); i++) {
		mmc = mmc_queue_item(reading, i);

		if (FD_ISSET(mmc->readreq->io->fd, &rfds)) {
			/* fill read buffer if needed */
			if (mmc->readreq->read != NULL) {
				result = mmc->readreq->read(mmc, mmc->readreq TSRMLS_CC);
				
				if (result != MMC_OK) {
					switch (result) {
						case MMC_REQUEST_FAILURE:
							/* take server offline and failover requests */
							mmc_server_deactivate(pool, mmc TSRMLS_CC);
							break;
	
						case MMC_REQUEST_RETRY:
							/* allow request to reschedule itself */
							mmc_select_retry(pool, mmc, mmc->readreq TSRMLS_CC);
							break;

						case MMC_REQUEST_MORE:
							/* add server to read queue once more */
							mmc_queue_push(pool->reading, mmc);
							break;
						
						default:
							php_error_docref(NULL TSRMLS_CC, E_ERROR, "Invalid return value, bailing out");
					}
					
					/* skip to next request */
					continue;
				}
			}

			do {
				/* delegate to request response handler */
				result = mmc->readreq->parse(mmc, mmc->readreq TSRMLS_CC);
				
				switch (result) {
					case MMC_REQUEST_FAILURE:
						/* take server offline and failover requests */
						mmc_server_deactivate(pool, mmc TSRMLS_CC);
						break;
					
					case MMC_REQUEST_RETRY:
						/* allow request to reschedule itself */
						mmc_select_retry(pool, mmc, mmc->readreq TSRMLS_CC);
						break;

					case MMC_REQUEST_DONE:
						/* release completed request */
						mmc_pool_release(pool, mmc->readreq);

						/* shift next request into read slot */
						mmc->readreq = mmc_queue_pop(&(mmc->readqueue));
						break;
					
					case MMC_REQUEST_MORE:
						break;
					
					case MMC_REQUEST_AGAIN:
						/* request wants another loop */
						break;
						
					default:
						php_error_docref(NULL TSRMLS_CC, E_ERROR, "Invalid return value, bailing out");
				}
			} while (mmc->readreq != NULL && (result == MMC_REQUEST_DONE || result == MMC_REQUEST_AGAIN));
			
			if (result == MMC_REQUEST_MORE) {
				/* add server to read queue once more */
				mmc_queue_push(pool->reading, mmc);
			}
		}
		else {
			/* add server to read queue once more */
			mmc_queue_push(pool->reading, mmc);
		}
	}
}
/* }}} */

void mmc_pool_run(mmc_pool_t *pool TSRMLS_DC)  /* 
	runs all scheduled requests to completion {{{ */
{
	mmc_t *mmc;
	while ((mmc = mmc_queue_pop(&(pool->pending))) != NULL) {
		smart_str_appendl(&(mmc->buildreq->sendbuf.value), "\r\n", sizeof("\r\n")-1);
		mmc_pool_schedule(pool, mmc, mmc->buildreq TSRMLS_CC);
		mmc->buildreq = NULL;
	}

	while (mmc_queue_len(pool->reading) || mmc_queue_len(pool->sending)) {
		/* TODO: replace 1 with pool->timeout */
		mmc_pool_select(pool, 1 TSRMLS_CC);
	}
}
/* }}} */

inline int mmc_prepare_key_ex(const char *key, unsigned int key_len, char *result, unsigned int *result_len)  /* {{{ */
{
	unsigned int i;
	if (key_len == 0) {
		return MMC_REQUEST_FAILURE;
	}

	*result_len = key_len < MMC_MAX_KEY_LEN ? key_len : MMC_MAX_KEY_LEN;
	result[*result_len] = '\0';
	
	for (i=0; i<*result_len; i++) {
		result[i] = key[i] > ' ' ? key[i] : '_';
	}
	
	return MMC_OK;
}
/* }}} */

inline int mmc_prepare_key(zval *key, char *result, unsigned int *result_len)  /* {{{ */
{
	if (Z_TYPE_P(key) == IS_STRING) {
		return mmc_prepare_key_ex(Z_STRVAL_P(key), Z_STRLEN_P(key), result, result_len);
	} else {
		int res;
		zval *keytmp;
		ALLOC_ZVAL(keytmp);

		*keytmp = *key;
		zval_copy_ctor(keytmp);
		INIT_PZVAL(keytmp);
		convert_to_string(keytmp);

		res = mmc_prepare_key_ex(Z_STRVAL_P(keytmp), Z_STRLEN_P(keytmp), result, result_len);

		zval_dtor(keytmp);
		FREE_ZVAL(keytmp);
		
		return res;
	}
}
/* }}} */
