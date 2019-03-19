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
#ifdef PHP_WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "php.h"
#include "php_network.h"
#include "ext/standard/crc32.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_string.h"
#include "zend_smart_str.h"
#include "memcache_pool.h"

ZEND_DECLARE_MODULE_GLOBALS(memcache)

MMC_POOL_INLINE void mmc_buffer_alloc(mmc_buffer_t *buffer, unsigned int size)  /*
	ensures space for an additional size bytes {{{ */
{
#if PHP_VERSION_ID < 70200
	register size_t newlen;
#endif
	smart_string_alloc((&(buffer->value)), size, 0);
}
/* }}} */

MMC_POOL_INLINE void mmc_buffer_free(mmc_buffer_t *buffer)  /* {{{ */
{
	if (buffer->value.c != NULL) {
		smart_string_free(&(buffer->value));
	}
	ZEND_SECURE_ZERO(buffer, sizeof(*buffer));
}
/* }}} */

static unsigned int mmc_hash_crc32_init()						{ return ~0; }
static unsigned int mmc_hash_crc32_finish(unsigned int seed)	{ return ~seed; }

static unsigned int mmc_hash_crc32_combine(unsigned int seed, const void *key, unsigned int key_len) /*
	CRC32 hash {{{ */
{
	const char *p = (const char *)key, *end = p + key_len;
	while (p < end) {
		CRC32(seed, *(p++));
	}

  	return seed;
}
/* }}} */

mmc_hash_function_t mmc_hash_crc32 = {
	mmc_hash_crc32_init,
	mmc_hash_crc32_combine,
	mmc_hash_crc32_finish
};

static unsigned int mmc_hash_fnv1a_combine(unsigned int seed, const void *key, unsigned int key_len) /*
	FNV-1a hash {{{ */
{
	const char *p = (const char *)key, *end = p + key_len;
	while (p < end) {
		seed ^= (unsigned int)*(p++);
		seed *= FNV_32_PRIME;
	}

	return seed;
}
/* }}} */

static unsigned int mmc_hash_fnv1a_init()						{ return FNV_32_INIT; }
static unsigned int mmc_hash_fnv1a_finish(unsigned int seed)	{ return seed; }

mmc_hash_function_t mmc_hash_fnv1a = {
	mmc_hash_fnv1a_init,
	mmc_hash_fnv1a_combine,
	mmc_hash_fnv1a_finish
};

double timeval_to_double(struct timeval tv) {
	return (double)tv.tv_sec + ((double)tv.tv_usec) / 1000000;
}

struct timeval double_to_timeval(double sec) {
	struct timeval tv;
	tv.tv_sec = (long)sec;
	tv.tv_usec = (sec - tv.tv_sec) * 1000000;
	return tv;
}

static size_t mmc_stream_read_buffered(mmc_stream_t *io, char *buf, size_t count) /*
	attempts to reads count bytes from the stream buffer {{{ */
{
	size_t toread = io->buffer.value.len - io->buffer.idx < count ? io->buffer.value.len - io->buffer.idx : count;
	memcpy(buf, io->buffer.value.c + io->buffer.idx, toread);
	io->buffer.idx += toread;
	return toread;
}
/* }}} */

static char *mmc_stream_readline_buffered(mmc_stream_t *io, char *buf, size_t maxlen, size_t *retlen)  /*
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

static size_t mmc_stream_read_wrapper(mmc_stream_t *io, char *buf, size_t count)  /* {{{ */
{
	return php_stream_read(io->stream, buf, count);
}
/* }}} */

static char *mmc_stream_readline_wrapper(mmc_stream_t *io, char *buf, size_t maxlen, size_t *retlen)  /* {{{ */
{
	return php_stream_get_line(io->stream, buf, maxlen, retlen);
}
/* }}} */

void mmc_request_reset(mmc_request_t *request) /* {{{ */
{
	request->key_len = 0;
	mmc_buffer_reset(&(request->sendbuf));
	mmc_queue_reset(&(request->failed_servers));
	request->failed_index = 0;
}
/* }}} */

void mmc_request_free(mmc_request_t *request)  /* {{{ */
{
	mmc_buffer_free(&(request->sendbuf));
	mmc_buffer_free(&(request->readbuf));
	mmc_queue_free(&(request->failed_servers));
	efree(request);
}
/* }}} */

static inline int mmc_request_send(mmc_t *mmc, mmc_request_t *request) /* {{{ */
{
	int count, bytes;

	/* send next chunk of buffer */
	count = request->sendbuf.value.len - request->sendbuf.idx;
	if (count > request->io->stream->chunk_size) {
		count = request->io->stream->chunk_size;
	}

	bytes = send(request->io->fd, request->sendbuf.value.c + request->sendbuf.idx, count, MSG_NOSIGNAL);
	if (bytes >= 0) {
		request->sendbuf.idx += bytes;

		/* done sending? */
		if (request->sendbuf.idx >= request->sendbuf.value.len) {
			return MMC_REQUEST_DONE;
		}

		return MMC_REQUEST_MORE;
	}
	else {
		char *message, buf[1024];
		long err = php_socket_errno();

		if (err == EAGAIN) {
			return MMC_REQUEST_MORE;
		}

		message = php_socket_strerror(err, buf, 1024);
		return mmc_server_failure(mmc, request->io, message, err);
	}
}
/* }}} */

static int mmc_request_read_udp(mmc_t *mmc, mmc_request_t *request) /*
	reads an entire datagram into buffer and validates the udp header {{{ */
{
	size_t bytes;
	mmc_udp_header_t *header;
	uint16_t reqid, seqid;

	/* reset buffer if completely consumed */
	if (request->io->buffer.idx >= request->io->buffer.value.len) {
		mmc_buffer_reset(&(request->io->buffer));
	}

	/* attempt to read datagram + sentinel-byte */
	mmc_buffer_alloc(&(request->io->buffer), MMC_MAX_UDP_LEN + 1);
	bytes = php_stream_read(request->io->stream, request->io->buffer.value.c + request->io->buffer.value.len, MMC_MAX_UDP_LEN + 1);

	if (bytes < sizeof(mmc_udp_header_t)) {
		return mmc_server_failure(mmc, request->io, "Failed te read complete UDP header from stream", 0);
	}
	if (bytes > MMC_MAX_UDP_LEN) {
		return mmc_server_failure(mmc, request->io, "Server sent packet larger than MMC_MAX_UDP_LEN bytes", 0);
	}

	header = (mmc_udp_header_t *)(request->io->buffer.value.c + request->io->buffer.value.len);
	reqid = ntohs(header->reqid);
	seqid = ntohs(header->seqid);

	/* initialize udp header fields */
	if (!request->udp.total && request->udp.reqid == reqid) {
		request->udp.seqid = seqid;
		request->udp.total = ntohs(header->total);
	}

	/* detect dropped packets and reschedule for tcp delivery */
	if (request->udp.reqid != reqid || request->udp.seqid != seqid) {
		/* ensure that no more udp requests are scheduled for a while */
		request->io->status = MMC_STATUS_FAILED;
		request->io->failed = (long)time(NULL);

		/* discard packets for previous requests */
		if (reqid < request->udp.reqid) {
			return MMC_REQUEST_MORE;
		}

		php_error_docref(NULL, E_NOTICE, "UDP packet loss, expected reqid/seqid %d/%d got %d/%d",
			(int)request->udp.reqid, (int)request->udp.seqid, (int)reqid, (int)seqid);
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

static void mmc_compress(mmc_pool_t *pool, mmc_buffer_t *buffer, const char *value, int value_len, unsigned int *flags, int copy) /* {{{ */
{
	/* autocompress large values */
	if (pool->compress_threshold && value_len >= pool->compress_threshold) {
		*flags |= MMC_COMPRESSED;
	}

	if (*flags & MMC_COMPRESSED) {
		int status;
		mmc_buffer_t prev;
		unsigned long result_len = value_len * (1 - pool->min_compress_savings);

		if (copy) {
			/* value is already in output buffer */
			prev = *buffer;

			/* allocate space for prev header + result */
			ZEND_SECURE_ZERO(buffer, sizeof(*buffer));
			mmc_buffer_alloc(buffer, prev.value.len + result_len);

			/* append prev header */
			smart_string_appendl(&(buffer->value), prev.value.c, prev.value.len - value_len);
			buffer->idx = prev.idx;
		}
		else {
			/* allocate space directly in buffer */
			mmc_buffer_alloc(buffer, result_len);
		}

		if (MMC_COMPRESSION_LEVEL >= 0) {
			status = compress2((unsigned char *)buffer->value.c + buffer->value.len, &result_len, (unsigned const char *)value, value_len, MMC_COMPRESSION_LEVEL);
		} else {
			status = compress((unsigned char *)buffer->value.c + buffer->value.len, &result_len, (unsigned const char *)value, value_len);
		}

		if (status == Z_OK) {
			buffer->value.len += result_len;
		}
		else {
			smart_string_appendl(&(buffer->value), value, value_len);
			*flags &= ~MMC_COMPRESSED;
		}

		if (copy) {
			mmc_buffer_free(&prev);
		}
	}
	else if (!copy) {
		smart_string_appendl(&(buffer->value), value, value_len);
	}
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

int mmc_pack_value(mmc_pool_t *pool, mmc_buffer_t *buffer, zval *value, unsigned int *flags) /*
	does serialization and compression to pack a zval into the buffer {{{ */
{
	if (*flags & 0xffff & ~MMC_COMPRESSED) {
		php_error_docref(NULL, E_WARNING, "The lowest two bytes of the flags array is reserved for pecl/memcache internal use");
		return MMC_REQUEST_FAILURE;
	}

	*flags &= ~MMC_SERIALIZED;
	switch (Z_TYPE_P(value)) {
		case IS_STRING:
			*flags |= MMC_TYPE_STRING;
			mmc_compress(pool, buffer, Z_STRVAL_P(value), Z_STRLEN_P(value), flags, 0);
			break;

		case IS_LONG:
			*flags |= MMC_TYPE_LONG;
			*flags &= ~MMC_COMPRESSED;
			smart_string_append_long(&(buffer->value), Z_LVAL_P(value));
			break;

		case IS_DOUBLE: {
			char buf[256];
			int len = snprintf(buf, 256, "%.14g", Z_DVAL_P(value));
			*flags |= MMC_TYPE_DOUBLE;
			*flags &= ~MMC_COMPRESSED;
			smart_string_appendl(&(buffer->value), buf, len);
			break;
		}

		case IS_TRUE:
		case IS_FALSE:
			*flags |= MMC_TYPE_BOOL;
			*flags &= ~MMC_COMPRESSED;
			smart_string_appendc(&(buffer->value), Z_TYPE_P(value) == IS_TRUE ? '1' : '0');
			break;

		default: {
			php_serialize_data_t value_hash;
			zval value_copy, *value_copy_ptr;
			size_t prev_len = buffer->value.len;
			smart_str buf = {0};

			/* FIXME: we should be using 'Z' instead of this, but unfortunately it's PHP5-only */
			value_copy = *value;
			zval_copy_ctor(&value_copy);
			value_copy_ptr = &value_copy;

			PHP_VAR_SERIALIZE_INIT(value_hash);
			php_var_serialize(&buf, value_copy_ptr, &value_hash);
			PHP_VAR_SERIALIZE_DESTROY(value_hash);

			smart_string_appendl(&(buffer->value), ZSTR_VAL(buf.s), ZSTR_LEN(buf.s));
			smart_str_free(&buf);

			/* trying to save null or something went really wrong */
			if (buffer->value.c == NULL || buffer->value.len == prev_len) {
				zval_dtor(&value_copy);
				php_error_docref(NULL, E_WARNING, "Failed to serialize value");
				return MMC_REQUEST_FAILURE;
			}

			*flags |= MMC_SERIALIZED;
			zval_dtor(&value_copy);

			mmc_compress(pool, buffer, buffer->value.c + prev_len, buffer->value.len - prev_len, flags, 1);
		}
	}

	return MMC_OK;
}
/* }}} */

int mmc_unpack_value(
	mmc_t *mmc, mmc_request_t *request, mmc_buffer_t *buffer, const char *key, unsigned int key_len,
	unsigned int flags, unsigned long cas, unsigned int bytes) /*
	does uncompression and unserializing to reconstruct a zval {{{ */
{
	char *data = NULL;
	unsigned long data_len;

	zval object;

	if (flags & MMC_COMPRESSED) {
		if (mmc_uncompress(buffer->value.c, bytes, &data, &data_len) != MMC_OK) {
			php_error_docref(NULL, E_NOTICE, "Failed to uncompress data");
			return MMC_REQUEST_DONE;
		}
	}
	else {
		data = buffer->value.c;
		data_len = bytes;
	}

	if (flags & MMC_SERIALIZED) {
		php_unserialize_data_t var_hash;
		const unsigned char *p = (unsigned char *)data;
		char key_tmp[MMC_MAX_KEY_LEN + 1];
		mmc_request_value_handler value_handler;
		void *value_handler_param;
		mmc_buffer_t buffer_tmp;

		/* make copies of data to ensure re-entrancy */
		memcpy(key_tmp, key, key_len + 1);
		value_handler = request->value_handler;
		value_handler_param = request->value_handler_param;

		if (!(flags & MMC_COMPRESSED)) {
			buffer_tmp = *buffer;
			mmc_buffer_release(buffer);
		}

		PHP_VAR_UNSERIALIZE_INIT(var_hash);
		if (!php_var_unserialize(&object, &p, p + data_len, &var_hash)) {
			PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

			if (flags & MMC_COMPRESSED) {
				efree(data);
			}
			else if (buffer->value.c == NULL) {
				*buffer = buffer_tmp;
			}
			else {
				mmc_buffer_free(&buffer_tmp);
			}

			php_error_docref(NULL, E_NOTICE, "Failed to unserialize data");
			return MMC_REQUEST_DONE;
		}

		PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

		if (flags & MMC_COMPRESSED) {
			efree(data);
		}
		else if (buffer->value.c == NULL) {
			*buffer = buffer_tmp;
		}
		else {
			mmc_buffer_free(&buffer_tmp);
		}

		/* delegate to value handler */
		return value_handler(key_tmp, key_len, &object, flags, cas, value_handler_param);
	}
	else {
		switch (flags & 0x0f00) {
			case MMC_TYPE_LONG: {
				long val;
				data[data_len] = '\0';
				val = strtol(data, NULL, 10);
				ZVAL_LONG(&object, val);
				break;
			}

			case MMC_TYPE_DOUBLE: {
				double val = 0;
				data[data_len] = '\0';
				sscanf(data, "%lg", &val);
				ZVAL_DOUBLE(&object, val);
				break;
			}

			case MMC_TYPE_BOOL:
				ZVAL_BOOL(&object, data_len == 1 && data[0] == '1');
				break;

			default:
				data[data_len] = '\0';
				ZVAL_STRINGL(&object, data, data_len);
				efree(data);

				if (!(flags & MMC_COMPRESSED)) {
					/* release buffer because it's now owned by the zval */
					mmc_buffer_release(buffer);
				}
		}

		/* delegate to value handler */
		return request->value_handler(key, key_len, &object, flags, cas, request->value_handler_param);
	}
}
/* }}}*/


mmc_t *mmc_server_new(
	const char *host, int host_len, unsigned short tcp_port, unsigned short udp_port,
	int persistent, double timeout, int retry_interval) /* {{{ */
{
	mmc_t *mmc = pemalloc(sizeof(mmc_t), persistent);
	ZEND_SECURE_ZERO(mmc, sizeof(*mmc));

	mmc->host = pemalloc(host_len + 1, persistent);
	memcpy(mmc->host, host, host_len);
	mmc->host[host_len] = '\0';

	mmc->tcp.port = tcp_port;
	mmc->tcp.status = MMC_STATUS_DISCONNECTED;
	mmc->udp.port = udp_port;
	mmc->udp.status = MMC_STATUS_DISCONNECTED;

	mmc->persistent = persistent;
	mmc->timeout = double_to_timeval(timeout);

	mmc->tcp.retry_interval = retry_interval;
	mmc->tcp.chunk_size = MEMCACHE_G(chunk_size);
	mmc->udp.retry_interval = retry_interval;
	mmc->udp.chunk_size = MEMCACHE_G(chunk_size); /* = MMC_MAX_UDP_LEN;*/

	return mmc;
}
/* }}} */

static void _mmc_server_disconnect(mmc_t *mmc, mmc_stream_t *io, int close_persistent_stream) /* {{{ */
{
	mmc_buffer_free(&(io->buffer));

	if (io->stream != NULL) {
		if (mmc->persistent) {
			if (close_persistent_stream) {
				php_stream_pclose(io->stream);
			}
		}
		else {
			php_stream_close(io->stream);
		}

		io->stream = NULL;
		io->fd = 0;
	}

	io->status = MMC_STATUS_DISCONNECTED;
}
/* }}} */

void mmc_server_disconnect(mmc_t *mmc, mmc_stream_t *io) /* {{{ */
{
	_mmc_server_disconnect(mmc, io, 1);
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

static void mmc_server_deactivate(mmc_pool_t *pool, mmc_t *mmc) /*
	disconnect and marks the server as down, failovers all queued requests {{{ */
{
	mmc_queue_t readqueue;
	mmc_request_t *request;

	mmc_server_disconnect(mmc, &(mmc->tcp));
	mmc_server_disconnect(mmc, &(mmc->udp));

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
		request->failover_handler(pool, mmc, request, request->failover_handler_param);
	}

	mmc_queue_free(&readqueue);

	/* fire userspace failure event */
	if (pool->failure_callback != NULL) {
		pool->failure_callback(pool, mmc, &pool->failure_callback_param);
	}
}
/* }}} */

int mmc_server_failure(mmc_t *mmc, mmc_stream_t *io, const char *error, int errnum) /*
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

int mmc_request_failure(mmc_t *mmc, mmc_stream_t *io, const char *message, unsigned int message_len, int errnum) /*
	 checks for a valid server generated error message and calls mmc_server_failure() {{{ */
{
	if (message_len) {
		return mmc_server_failure(mmc, io, message, errnum);
	}

	return mmc_server_failure(mmc, io, "Malformed server response", errnum);
}
/* }}} */

static int mmc_server_connect(mmc_pool_t *pool, mmc_t *mmc, mmc_stream_t *io, int udp) /*
	connects a stream, calls mmc_server_deactivate() on failure {{{ */
{
	char *host, *hash_key = NULL;
	zend_string *errstr = NULL;
	int	host_len, errnum = 0;
	struct timeval tv = mmc->timeout;
	int fd;

	/* close open stream */
	if (io->stream != NULL) {
		mmc_server_disconnect(mmc, io);
	}

	if (mmc->persistent) {
		spprintf(&hash_key, 0, "memcache:stream:%s:%u:%d", mmc->host, io->port, udp);
	}

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
		REPORT_ERRORS | (mmc->persistent ? STREAM_OPEN_PERSISTENT : 0),
		STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
		hash_key, &tv, NULL, &errstr, &errnum);

	efree(host);

	if (hash_key != NULL) {
		efree(hash_key);
	}

	/* check connection and extract socket for select() purposes */
	if (!io->stream || php_stream_cast(io->stream, PHP_STREAM_AS_FD_FOR_SELECT, (void **)&fd, 1) != SUCCESS) {
		mmc_server_seterror(mmc, errstr != NULL ? ZSTR_VAL(errstr) : "Connection failed", errnum);
		mmc_server_deactivate(pool, mmc);

		if (errstr != NULL) {
			efree(errstr);
		}

		return MMC_REQUEST_FAILURE;
	}
	php_stream_auto_cleanup(io->stream);
	php_stream_set_chunk_size(io->stream, io->chunk_size);
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &(mmc->timeout));

	/* doing our own buffering increases performance */
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_READ_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
	php_stream_set_option(io->stream, PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);

	io->fd = fd;
	io->status = MMC_STATUS_CONNECTED;

	/* php_stream buffering prevent us from detecting datagram boundaries when using udp */
	if (udp) {
		io->read = mmc_stream_read_buffered;
		io->readline = mmc_stream_readline_buffered;
	}
	else {
		io->read = mmc_stream_read_wrapper;
		io->readline = mmc_stream_readline_wrapper;
	}
	
#ifdef SO_NOSIGPIPE
	/* Mac OS X doesn't have MSG_NOSIGNAL */
	{
		int optval = 1;
		setsockopt(io->fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&optval, sizeof(optval));
	}
#endif

	if (mmc->error != NULL) {
		efree(mmc->error);
		mmc->error = NULL;
	}

	return MMC_OK;
}
/* }}} */

int mmc_server_valid(mmc_t *mmc) /*
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

void mmc_server_sleep(mmc_t *mmc) /*
	prepare server struct for persistent sleep {{{ */
{
	mmc_buffer_free(&(mmc->tcp.buffer));
	mmc_buffer_free(&(mmc->udp.buffer));

	mmc->sendreq = NULL;
	mmc->readreq = NULL;
	mmc->buildreq = NULL;

	mmc_queue_free(&(mmc->sendqueue));
	mmc_queue_free(&(mmc->readqueue));

	if (mmc->error != NULL) {
		efree(mmc->error);
		mmc->error = NULL;
	}
}
/* }}} */

void mmc_server_free(mmc_t *mmc) /* {{{ */
{
	pefree(mmc->host, mmc->persistent);
	pefree(mmc, mmc->persistent);
}
/* }}} */

static void mmc_pool_init_hash(mmc_pool_t *pool) /* {{{ */
{
	mmc_hash_function_t *hash;

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
}
/* }}} */

mmc_pool_t *mmc_pool_new() /* {{{ */
{
	mmc_pool_t *pool = emalloc(sizeof(mmc_pool_t));
	ZEND_SECURE_ZERO(pool, sizeof(*pool));

	switch (MEMCACHE_G(protocol)) {
		case MMC_BINARY_PROTOCOL:
			pool->protocol = &mmc_binary_protocol;
			break;
		default:
			pool->protocol = &mmc_ascii_protocol;
	}

	mmc_pool_init_hash(pool);
	pool->compress_threshold = MEMCACHE_G(compress_threshold);
	pool->min_compress_savings = MMC_DEFAULT_SAVINGS;

	pool->sending = &(pool->_sending1);
	pool->reading = &(pool->_reading1);

	return pool;
}
/* }}} */

void mmc_pool_free(mmc_pool_t *pool) /* {{{ */
{
	int i;
	mmc_request_t *request;

	for (i=0; i<pool->num_servers; i++) {
		if (pool->servers[i] != NULL) {
			if (pool->servers[i]->persistent) {
				mmc_server_sleep(pool->servers[i]);
			} else {
				mmc_server_free(pool->servers[i]);
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
		pool->protocol->free_request(request);
	}
	mmc_queue_free(&(pool->free_requests));

	efree(pool);
}
/* }}} */

void mmc_pool_add(mmc_pool_t *pool, mmc_t *mmc, unsigned int weight) /*
	adds a server to the pool and hash strategy {{{ */
{
	pool->hash->add_server(pool->hash_state, mmc, weight);
	pool->servers = erealloc(pool->servers, sizeof(*pool->servers) * (pool->num_servers + 1));
	pool->servers[pool->num_servers] = mmc;

	/* store the smallest timeout for any server */
	if (!pool->num_servers || timeval_to_double(mmc->timeout) < timeval_to_double(pool->timeout)) {
		pool->timeout = mmc->timeout;
	}

	pool->num_servers++;
}
/* }}} */

void mmc_pool_close(mmc_pool_t *pool) /*
	disconnects and removes all servers in the pool {{{ */
{
	if (pool->num_servers) {
		int i;

		for (i=0; i<pool->num_servers; i++) {
			if (pool->servers[i]->persistent) {
				mmc_server_sleep(pool->servers[i]);
			} else {
				mmc_server_free(pool->servers[i]);
			}
		}

		efree(pool->servers);
		pool->servers = NULL;
		pool->num_servers = 0;

		/* reallocate the hash strategy state */
		pool->hash->free_state(pool->hash_state);
		mmc_pool_init_hash(pool);
	}
}
/* }}} */

int mmc_pool_open(mmc_pool_t *pool, mmc_t *mmc, mmc_stream_t *io, int udp) /*
	connects if the stream is not already connected {{{ */
{
	switch (io->status) {
		case MMC_STATUS_CONNECTED:
		case MMC_STATUS_UNKNOWN:
			return MMC_OK;

		case MMC_STATUS_DISCONNECTED:
		case MMC_STATUS_FAILED:

			return mmc_server_connect(pool, mmc, io, udp);
	}

	return MMC_REQUEST_FAILURE;
}
/* }}} */

mmc_t *mmc_pool_find_next(mmc_pool_t *pool, const char *key, unsigned int key_len, mmc_queue_t *skip_servers, unsigned int *last_index) /*
	finds the next server in the failover sequence {{{ */
{
	mmc_t *mmc;
	char keytmp[MMC_MAX_KEY_LEN + MAX_LENGTH_OF_LONG + 1];
	unsigned int keytmp_len;

	/* find the next server not present in the skip-list */
	do {
		keytmp_len = sprintf(keytmp, "%s-%d", key, (*last_index)++);
		mmc = pool->hash->find_server(pool->hash_state, keytmp, keytmp_len);
	} while (mmc_queue_contains(skip_servers, mmc) && *last_index < MEMCACHE_G(max_failover_attempts));

	return mmc;
}

mmc_t *mmc_pool_find(mmc_pool_t *pool, const char *key, unsigned int key_len) /*
	maps a key to a non-failed server {{{ */
{
	mmc_t *mmc = pool->hash->find_server(pool->hash_state, key, key_len);

	/* check validity and try to failover otherwise */
	if (!mmc_server_valid(mmc) && MEMCACHE_G(allow_failover)) {
		unsigned int last_index = 0;

		do {
			mmc = mmc_pool_find_next(pool, key, key_len, NULL, &last_index);
		} while (!mmc_server_valid(mmc) && last_index < MEMCACHE_G(max_failover_attempts));
	}

	return mmc;
}
/* }}} */

int mmc_pool_failover_handler(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param) /*
	uses request->key to reschedule request to other server {{{ */
{
	if (MEMCACHE_G(allow_failover) && request->failed_index < MEMCACHE_G(max_failover_attempts) && request->failed_servers.len < pool->num_servers) {
		do {
			mmc_queue_push(&(request->failed_servers), mmc);
			mmc = mmc_pool_find_next(pool, request->key, request->key_len, &(request->failed_servers), &(request->failed_index));
		} while (!mmc_server_valid(mmc) && request->failed_index < MEMCACHE_G(max_failover_attempts) && request->failed_servers.len < pool->num_servers);

		return mmc_pool_schedule(pool, mmc, request);
	}

	mmc_pool_release(pool, request);
	return MMC_REQUEST_FAILURE;
}
/* }}}*/

int mmc_pool_failover_handler_null(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param) /*
	always returns failure {{{ */
{
	mmc_pool_release(pool, request);
	return MMC_REQUEST_FAILURE;
}
/* }}}*/

static int mmc_pool_response_handler_null(mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param) /*
	always returns done {{{ */
{
	return MMC_REQUEST_DONE;
}
/* }}}*/

static inline mmc_request_t *mmc_pool_request_alloc(mmc_pool_t *pool, int protocol,
	mmc_request_failover_handler failover_handler, void *failover_handler_param) /* {{{ */
{
	mmc_request_t *request = mmc_queue_pop(&(pool->free_requests));
	if (request == NULL) {
		request = pool->protocol->create_request();
	}
	else {
		pool->protocol->reset_request(request);
	}

	request->protocol = protocol;

	if (protocol == MMC_PROTO_UDP) {
		mmc_udp_header_t header = {0};
		smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
	}

	request->failover_handler = failover_handler != NULL ? failover_handler : mmc_pool_failover_handler_null;
	request->failover_handler_param = failover_handler_param;

	return request;
}
/* }}} */

mmc_request_t *mmc_pool_request(mmc_pool_t *pool, int protocol,
	mmc_request_response_handler response_handler, void *response_handler_param,
	mmc_request_failover_handler failover_handler, void *failover_handler_param) /*
	allocates a request, must be added to pool using mmc_pool_schedule or released with mmc_pool_release {{{ */
{
	mmc_request_t *request = mmc_pool_request_alloc(pool, protocol, failover_handler, failover_handler_param);
	request->response_handler = response_handler;
	request->response_handler_param = response_handler_param;
	return request;
}
/* }}} */

mmc_request_t *mmc_pool_request_get(mmc_pool_t *pool, int protocol,
	mmc_request_value_handler value_handler, void *value_handler_param,
	mmc_request_failover_handler failover_handler, void *failover_handler_param) /*
	allocates a request, must be added to pool using mmc_pool_schedule or released with mmc_pool_release {{{ */
{
	mmc_request_t *request = mmc_pool_request(
		pool, protocol,
		mmc_pool_response_handler_null, NULL,
		failover_handler, failover_handler_param);

	request->value_handler = value_handler;
	request->value_handler_param = value_handler_param;
	return request;
}
/* }}} */

mmc_request_t *mmc_pool_clone_request(mmc_pool_t *pool, mmc_request_t *request) /*
	clones a request, must be added to pool using mmc_pool_schedule or released with mmc_pool_release {{{ */
{
	mmc_request_t *clone = mmc_pool_request_alloc(pool, request->protocol, NULL, NULL);

	clone->response_handler = request->response_handler;
	clone->response_handler_param = request->response_handler_param;
	clone->value_handler = request->value_handler;
	clone->value_handler_param = request->value_handler_param;

	/* copy payload parser */
	clone->parse = request->parse;

	/* copy key */
	memcpy(clone->key, request->key, request->key_len);
	clone->key_len = request->key_len;

	/* copy sendbuf */
	mmc_buffer_alloc(&(clone->sendbuf), request->sendbuf.value.len);
	memcpy(clone->sendbuf.value.c, request->sendbuf.value.c, request->sendbuf.value.len);
	clone->sendbuf.value.len = request->sendbuf.value.len;

	/* copy protocol specific values */
	pool->protocol->clone_request(clone, request);

	return clone;
}
/* }}} */

static int mmc_pool_slot_send(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, int handle_failover) /* {{{ */
{
	if (request != NULL) {
		/* select protocol strategy and open connection */
		if (request->protocol == MMC_PROTO_UDP && mmc->udp.port &&
			request->sendbuf.value.len <= mmc->udp.chunk_size &&
			mmc_pool_open(pool, mmc, &(mmc->udp), 1) == MMC_OK)
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
		else if (mmc_pool_open(pool, mmc, &(mmc->tcp), 0) == MMC_OK) {
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
				return request->failover_handler(pool, mmc, request, request->failover_handler_param);
			}
			return MMC_REQUEST_FAILURE;
		}
	}

	mmc->sendreq = request;
	return MMC_OK;
}
/* }}} */

int mmc_pool_schedule(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request) /*
	schedules a request against a server, return MMC_OK on success {{{ */
{
	if (!mmc_server_valid(mmc)) {
		/* delegate to failover handler if connect fails */
		return request->failover_handler(pool, mmc, request, request->failover_handler_param);
	}

	/* reset sendbuf to start position */
	request->sendbuf.idx = 0;
	/* reset readbuf entirely */
	mmc_buffer_reset(&(request->readbuf));

	/* push request into sendreq slot if available */
	if (mmc->sendreq == NULL) {
		if (mmc_pool_slot_send(pool, mmc, request, 0) != MMC_OK) {
			return request->failover_handler(pool, mmc, request, request->failover_handler_param);
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

int mmc_pool_schedule_key(mmc_pool_t *pool, const char *key, unsigned int key_len, mmc_request_t *request, unsigned int redundancy) /*
	schedules a request against a server selected by the provided key, return MMC_OK on success {{{ */
{
	if (redundancy > 1) {
		int i, result;
		mmc_t *mmc;

		unsigned int last_index = 0;
		mmc_queue_t skip_servers = {0};

		/* schedule the first request */
		mmc = mmc_pool_find(pool, key, key_len);
		result = mmc_pool_schedule(pool, mmc, request);

		/* clone and schedule redundancy-1 additional requests */
		for (i=0; i < redundancy-1 && i < pool->num_servers-1; i++) {
			mmc_queue_push(&skip_servers, mmc);
			mmc = mmc_pool_find_next(pool, key, key_len, &skip_servers, &last_index);

			if (mmc_server_valid(mmc)) {
				mmc_pool_schedule(pool, mmc, mmc_pool_clone_request(pool, request));
			}
		}

		mmc_queue_free(&skip_servers);
		return result;
	}

	return mmc_pool_schedule(pool, mmc_pool_find(pool, key, key_len), request);
}
/* }}} */

int mmc_pool_schedule_get(
	mmc_pool_t *pool, int protocol, int op, zval *zkey,
	mmc_request_value_handler value_handler, void *value_handler_param,
	mmc_request_failover_handler failover_handler, void *failover_handler_param,
	mmc_request_t *failed_request) /*
	schedules a get command against a server {{{ */
{
	mmc_t *mmc;
	char key[MMC_MAX_KEY_LEN + 1];
	unsigned int key_len;

	if (mmc_prepare_key(zkey, key, &key_len) != MMC_OK) {
		php_error_docref(NULL, E_WARNING, "Invalid key");
		return MMC_REQUEST_FAILURE;
	}

	mmc = mmc_pool_find(pool, key, key_len);
	if (!mmc_server_valid(mmc)) {
		return MMC_REQUEST_FAILURE;
	}

	if (mmc->buildreq == NULL) {
		mmc_queue_push(&(pool->pending), mmc);

		mmc->buildreq = mmc_pool_request_get(
			pool, protocol, value_handler, value_handler_param,
			failover_handler, failover_handler_param);

		if (failed_request != NULL) {
			mmc_queue_copy(&(mmc->buildreq->failed_servers), &(failed_request->failed_servers));
			mmc->buildreq->failed_index = failed_request->failed_index;
		}

		pool->protocol->begin_get(mmc->buildreq, op);
	}
	else if (protocol == MMC_PROTO_UDP && mmc->buildreq->sendbuf.value.len + key_len + 3 > MMC_MAX_UDP_LEN) {
		/* datagram if full, schedule for delivery */
		pool->protocol->end_get(mmc->buildreq);
		mmc_pool_schedule(pool, mmc, mmc->buildreq);

		/* begin sending requests immediatly */
		mmc_pool_select(pool);

		mmc->buildreq = mmc_pool_request_get(
			pool, protocol, value_handler, value_handler_param,
			failover_handler, failover_handler_param);

		if (failed_request != NULL) {
			mmc_queue_copy(&(mmc->buildreq->failed_servers), &(failed_request->failed_servers));
			mmc->buildreq->failed_index = failed_request->failed_index;
		}

		pool->protocol->begin_get(mmc->buildreq, op);
	}

	pool->protocol->append_get(mmc->buildreq, zkey, key, key_len);
	return MMC_OK;
}
/* }}} */

static inline void mmc_pool_switch(mmc_pool_t *pool) {
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

static int mmc_select_failure(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, int result) /* {{{ */
{
	if (result == 0) {
		/* timeout expired, non-responsive server */
		if (mmc_server_failure(mmc, request->io, "Network timeout", 0) == MMC_REQUEST_RETRY) {
			return MMC_REQUEST_RETRY;
		}
	}
	else {
		char buf[1024];
		const char *message;
		long err = php_socket_errno();

		if (err) {
			message = php_socket_strerror(err, buf, 1024);
		}
		else {
			message = "Unknown select() error";
		}

		mmc_server_seterror(mmc, message, errno);
	}

	/* hard failure, deactivate connection */
	mmc_server_deactivate(pool, mmc);
	return MMC_REQUEST_FAILURE;
}
/* }}} */

static void mmc_select_retry(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request) /*
	removes request from send/read queues and calls failover {{{ */
{
	/* clear out failed request from queues */
	mmc_queue_remove(&(mmc->sendqueue), request);
	mmc_queue_remove(&(mmc->readqueue), request);

	/* shift next request into send slot */
	if (mmc->sendreq == request) {
		mmc_pool_slot_send(pool, mmc, mmc_queue_pop(&(mmc->sendqueue)), 1);

		/* clear out connection from send queue if no new request was slotted */
		if (!mmc->sendreq) {
			mmc_queue_remove(pool->sending, mmc);
		}
	}

	/* shift next request into read slot */
	if (mmc->readreq == request) {
		mmc->readreq = mmc_queue_pop(&(mmc->readqueue));

		/* clear out connection from read queue if no new request was slotted */
		if (!mmc->readreq) {
			mmc_queue_remove(pool->reading, mmc);
		}
	}

	request->failover_handler(pool, mmc, request, request->failover_handler_param);
}
/* }}} */

void mmc_pool_select(mmc_pool_t *pool) /*
	runs one select() round on all scheduled requests {{{ */
{
	int i, fd, result;
	mmc_t *mmc;
	mmc_queue_t *sending, *reading;

	/* help complete previous run */
	if (pool->in_select) {
		if (pool->sending == &(pool->_sending1)) {
			sending = &(pool->_sending2);
			reading = &(pool->_reading2);
		}
		else {
			sending = &(pool->_sending1);
			reading = &(pool->_reading1);
		}
	}
	else {
		int nfds = 0;
		struct timeval tv = pool->timeout;

		sending = pool->sending;
		reading = pool->reading;
		mmc_pool_switch(pool);

		FD_ZERO(&(pool->wfds));
		FD_ZERO(&(pool->rfds));

		for (i=0; i < sending->len; i++) {
			mmc = mmc_queue_item(sending, i);
			if (mmc->sendreq->io->fd > nfds) {
				nfds = mmc->sendreq->io->fd;
			}
			FD_SET(mmc->sendreq->io->fd, &(pool->wfds));
		}

		for (i=0; i < reading->len; i++) {
			mmc = mmc_queue_item(reading, i);
			if (mmc->readreq->io->fd > nfds) {
				nfds = mmc->readreq->io->fd;
			}
			FD_SET(mmc->readreq->io->fd, &(pool->rfds));
		}

		result = select(nfds + 1, &(pool->rfds), &(pool->wfds), NULL, &tv);

		/* if select timed out */
		if (result <= 0) {
			for (i=0; i < sending->len; i++) {
				mmc = (mmc_t *)mmc_queue_item(sending, i);
				
				/* remove sending request */
				if (!FD_ISSET(mmc->sendreq->io->fd, &(pool->wfds))) {
					mmc_queue_remove(sending, mmc);
					mmc_queue_remove(reading, mmc);
					i--;

					if (mmc_select_failure(pool, mmc, mmc->sendreq, result) == MMC_REQUEST_RETRY) {
						/* allow request to try and send again */
						mmc_select_retry(pool, mmc, mmc->sendreq);
					}
				}
			}

			for (i=0; i < reading->len; i++) {
				mmc = (mmc_t *)mmc_queue_item(reading, i);
				
				/* remove reading request */
				if (!FD_ISSET(mmc->readreq->io->fd, &(pool->rfds))) {
					mmc_queue_remove(sending, mmc);
					mmc_queue_remove(reading, mmc);
					i--;

					if (mmc_select_failure(pool, mmc, mmc->readreq, result) == MMC_REQUEST_RETRY) {
						/* allow request to try and read again */
						mmc_select_retry(pool, mmc, mmc->readreq);
					}
				}
			}
		}

		pool->in_select = 1;
	}

	for (i=0; i < sending->len; i++) {
		mmc = mmc_queue_item(sending, i);

		/* skip servers which have failed */
		if (!mmc->sendreq) {
			continue;
		}

		if (FD_ISSET(mmc->sendreq->io->fd, &(pool->wfds))) {
			fd = mmc->sendreq->io->fd;

			/* clear bit for reentrancy reasons */
			FD_CLR(fd, &(pool->wfds));

			/* until stream buffer is empty */
			do {
				/* delegate to request send handler */
				result = mmc_request_send(mmc, mmc->sendreq);

				/* check if someone has helped complete our run */
				if (!pool->in_select) {
					return;
				}

				switch (result) {
					case MMC_REQUEST_FAILURE:
						/* take server offline and failover requests */
						mmc_server_deactivate(pool, mmc);

						/* server is failed, remove from read queue */
						mmc_queue_remove(reading, mmc);
						break;

					case MMC_REQUEST_RETRY:
						/* allow request to reschedule itself */
						mmc_select_retry(pool, mmc, mmc->sendreq);
						break;

					case MMC_REQUEST_DONE:
						/* shift next request into send slot */
						mmc_pool_slot_send(pool, mmc, mmc_queue_pop(&(mmc->sendqueue)), 1);
						break;

					case MMC_REQUEST_MORE:
						/* send more data to socket */
						break;

					default:
						php_error_docref(NULL, E_ERROR, "Invalid return value, bailing out");
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

	for (i=0; i < reading->len; i++) {
		mmc = mmc_queue_item(reading, i);

		/* skip servers which have failed */
		if (!mmc->readreq) {
			continue;
		}

		if (FD_ISSET(mmc->readreq->io->fd, &(pool->rfds))) {
			fd = mmc->readreq->io->fd;

			/* clear bit for reentrancy reasons */
			FD_CLR(fd, &(pool->rfds));

			/* fill read buffer if needed */
			if (mmc->readreq->read != NULL) {
				result = mmc->readreq->read(mmc, mmc->readreq);

				if (result != MMC_OK) {
					switch (result) {
						case MMC_REQUEST_FAILURE:
							/* take server offline and failover requests */
							mmc_server_deactivate(pool, mmc);
							break;

						case MMC_REQUEST_RETRY:
							/* allow request to reschedule itself */
							mmc_select_retry(pool, mmc, mmc->readreq);
							break;

						case MMC_REQUEST_MORE:
							/* add server to read queue once more */
							mmc_queue_push(pool->reading, mmc);
							break;

						default:
							php_error_docref(NULL, E_ERROR, "Invalid return value, bailing out");
					}

					/* skip to next request */
					continue;
				}
			}

			/* until stream buffer is empty */
			do {
				/* delegate to request response handler */
				result = mmc->readreq->parse(mmc, mmc->readreq);

				/* check if someone has helped complete our run */
				if (!pool->in_select) {
					return;
				}

				switch (result) {
					case MMC_REQUEST_FAILURE:
						/* take server offline and failover requests */
						mmc_server_deactivate(pool, mmc);
						break;

					case MMC_REQUEST_RETRY:
						/* allow request to reschedule itself */
						mmc_select_retry(pool, mmc, mmc->readreq);
						break;

					case MMC_REQUEST_DONE:
						/* might have completed without having sent all data (e.g. object too large errors) */
						if (mmc->sendreq == mmc->readreq) {
							/* disconnect stream since data may have been sent before we received the SERVER_ERROR */
							mmc_server_disconnect(mmc, mmc->readreq->io);

							/* shift next request into send slot */
							mmc_pool_slot_send(pool, mmc, mmc_queue_pop(&(mmc->sendqueue)), 1);

							/* clear out connection from send queue if no new request was slotted */
							if (!mmc->sendreq) {
								mmc_queue_remove(pool->sending, mmc);
							}
						}

						/* release completed request */
						mmc_pool_release(pool, mmc->readreq);

						/* shift next request into read slot */
						mmc->readreq = mmc_queue_pop(&(mmc->readqueue));
						break;

					case MMC_REQUEST_MORE:
						/* read more data from socket */
						if (php_stream_eof(mmc->readreq->io->stream)) {
							result = mmc_server_failure(mmc, mmc->readreq->io, "Read failed (socket was unexpectedly closed)", 0);
							if (result == MMC_REQUEST_FAILURE) {
								/* take server offline and failover requests */
								mmc_server_deactivate(pool, mmc);
							}
						}
						break;

					case MMC_REQUEST_AGAIN:
						/* request wants another loop */
						break;

					default:
						php_error_docref(NULL, E_ERROR, "Invalid return value, bailing out");
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

	pool->in_select = 0;
}
/* }}} */

void mmc_pool_run(mmc_pool_t *pool)  /*
	runs all scheduled requests to completion {{{ */
{
	mmc_t *mmc;
	while ((mmc = mmc_queue_pop(&(pool->pending))) != NULL) {
		pool->protocol->end_get(mmc->buildreq);
		mmc_pool_schedule(pool, mmc, mmc->buildreq);
		mmc->buildreq = NULL;
	}

	while (pool->reading->len || pool->sending->len) {
		mmc_pool_select(pool);
	}
}
/* }}} */

MMC_POOL_INLINE int mmc_prepare_key_ex(const char *key, unsigned int key_len, char *result, unsigned int *result_len, char *prefix)  /* {{{ */
{
	unsigned int i, j, prefix_len=0;

	if (key_len == 0) {
		return MMC_REQUEST_FAILURE;
	}

	if (prefix) {
		prefix_len = strlen(prefix);
	}

	*result_len = (prefix_len + key_len) < MMC_MAX_KEY_LEN ? (prefix_len + key_len) : MMC_MAX_KEY_LEN;
	result[*result_len] = '\0';

	if (prefix_len) {
		for (i=0; i<prefix_len; i++) {
			result[i] = ((unsigned char)prefix[i]) > ' ' ? prefix[i] : '_';
		}

		for (j=0; j+i<*result_len; j++) {
			result[j+i] = ((unsigned char)key[j]) > ' ' ? key[j] : '_';
		}

		result[*result_len] = '\0';
	} else {
		for (i=0; i<*result_len; i++) {
			result[i] = ((unsigned char)key[i]) > ' ' ? key[i] : '_';
		}
	}

	return MMC_OK;
}
/* }}} */

MMC_POOL_INLINE int mmc_prepare_key(zval *key, char *result, unsigned int *result_len)  /* {{{ */
{
	if (Z_TYPE_P(key) == IS_STRING) {
		return mmc_prepare_key_ex(Z_STRVAL_P(key), Z_STRLEN_P(key), result, result_len, MEMCACHE_G(key_prefix));
	} else {
		int res;
		zval keytmp = *key;

		zval_copy_ctor(&keytmp);
		convert_to_string(&keytmp);

		res = mmc_prepare_key_ex(Z_STRVAL(keytmp), Z_STRLEN(keytmp), result, result_len, MEMCACHE_G(key_prefix));

		zval_dtor(&keytmp);
		return res;
	}
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
