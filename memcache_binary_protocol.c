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

#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "memcache_pool.h"
#include "ext/standard/php_smart_str.h"

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(x) (x)
# define htonll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# include <byteswap.h>
# define ntohll(x) bswap_64(x)
# define htonll(x) bswap_64(x)
#else
# error "Could not determine byte order: __BYTE_ORDER uncorrectly defined"
#endif

#define MMC_REQUEST_MAGIC	0x0f
#define	MMC_RESPONSE_MAGIC	0xf0

#define MMC_OP_DELETE		0x04
#define MMC_OP_MUTATE		0x05
#define MMC_OP_FLUSH		0x07
#define MMC_OP_GETQ			0x08
#define MMC_OP_NOOP			0x09
#define MMC_OP_VERSION		0x0a

typedef struct mmc_binary_request {
	mmc_request_t	base;			/* enable cast to mmc_request_t */
	mmc_queue_t		keys;			/* mmc_queue_t<zval *>, reqid -> key mappings */
	struct {
		uint8_t			cmdid;
		uint8_t			error;		/* error received in current request */
		uint32_t		reqid;		/* current reqid being processed */
	} command;
	struct {						/* stores value info while the body is being read */
		unsigned int	flags;
		unsigned long	length;
		uint64_t		cas;		/* CAS counter */
	} value;
} mmc_binary_request_t;

typedef struct mmc_request_header {
	uint8_t		magic;
	uint8_t		cmdid;
	uint8_t		key_len;
	uint8_t		_reserved;
	uint32_t	reqid;				/* opaque request id */
	uint32_t	length;				/* trailing body length (not including this header) */
} mmc_request_header_t;

typedef struct mmc_store_request_header {
	mmc_request_header_t	base;
	uint32_t				flags;
	uint32_t				exptime;
} mmc_store_request_header_t;

typedef struct mmc_cas_request_header {
	mmc_store_request_header_t	base;
	uint64_t					cas;
} mmc_cas_request_header_t;

typedef struct mmc_delete_request_header {
	mmc_request_header_t	base;
	uint32_t				exptime;
} mmc_delete_request_header_t;

typedef struct mmc_mutate_request_header {
	mmc_request_header_t	base;
	int64_t					value;
	int64_t					defval;
	uint32_t				exptime;
} mmc_mutate_request_header_t;

typedef struct mmc_flush_request_header {
	mmc_request_header_t	base;
	uint32_t				exptime;
} mmc_flush_request_header_t;

typedef struct mmc_response_header {
	uint8_t		magic;
	uint8_t		cmdid;
	uint8_t		error;
	uint8_t		_reserved;
	uint32_t	reqid;				/* echo'ed from request */
	uint32_t	length;				/* trailing body length (not including this header) */
} mmc_response_header_t;

typedef struct mmc_get_response_header {
	uint32_t				flags;
} mmc_get_response_header_t;

typedef struct mmc_gets_response_header {
	uint32_t				flags;
	uint64_t				cas;
} mmc_gets_response_header_t;

static int mmc_request_read_response(mmc_t *, mmc_request_t * TSRMLS_DC);
static int mmc_request_parse_value(mmc_t *, mmc_request_t * TSRMLS_DC);
static int mmc_request_parse_value_cas(mmc_t *, mmc_request_t * TSRMLS_DC);
static int mmc_request_read_value(mmc_t *, mmc_request_t * TSRMLS_DC);

static inline char *mmc_stream_get(mmc_stream_t *io, size_t bytes TSRMLS_DC) /* 
	attempts to read a number of bytes from server, returns the a pointer to the buffer on success, NULL if the complete number of bytes could not be read {{{ */
{
	io->input.idx += io->read(io, io->input.value + io->input.idx, bytes - io->input.idx TSRMLS_CC);
	
	if (io->input.idx >= bytes) {
		io->input.idx = 0;
		return io->input.value;
	}
	
	return NULL;
}	
/* }}} */

static inline void mmc_decode_response_header(mmc_response_header_t *header) /* {{{ */ 
{
	header->reqid = ntohl(header->reqid);
	header->length = ntohl(header->length);
}
/* }}} */

static int mmc_request_parse_response(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads a generic response header and reads the response body {{{ */
{
	mmc_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	header = (mmc_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	
	if (header != NULL) {
		mmc_decode_response_header(header);
		if (header->magic != MMC_RESPONSE_MAGIC) {
			return mmc_server_failure(mmc, request->io, "Malformed server response (invalid magic byte)", 0 TSRMLS_CC);
		}
		
		if (header->length == 0) {
			return request->response_handler(mmc, request, header->error, "", 0, request->response_handler_param TSRMLS_CC);
		}

		req->command.error = header->error;
		req->value.length = header->length;

		/* memory for data + \0 */
		mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);

		/* allow read_response handler to read the response body */
		request->parse = mmc_request_read_response;
		
		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_read_response(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	read the response body into the buffer {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	request->readbuf.idx += 
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length - request->readbuf.idx TSRMLS_CC);

	/* done reading? */
	if (request->readbuf.idx >= req->value.length) {
		return request->response_handler(mmc, request, req->command.error, request->readbuf.value.c, req->value.length, request->response_handler_param TSRMLS_CC);
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_value_header(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the header and then reads the value header {{{ */
{
	mmc_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		mmc_decode_response_header(header);
		if (header->magic != MMC_RESPONSE_MAGIC) {
			return mmc_server_failure(mmc, request->io, "Malformed server response (invalid magic byte)", 0 TSRMLS_CC);
		}
		
		/* read response body in case of error */
		if (header->error != 0) {
			if (header->length == 0) {
				return request->response_handler(mmc, request, header->error, "", 0, request->response_handler_param TSRMLS_CC);
			}
			
			req->command.error = header->error;
			req->value.length = header->length;

			/* memory for data + \0 */
			mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);

			/* allow read_response handler to read the response body */
			request->parse = mmc_request_read_response;
		}
		else {
			/* last command in multi-get */
			if (header->cmdid == MMC_OP_NOOP) {
				return MMC_REQUEST_DONE;
			}
			
			req->command.reqid = header->reqid;
			
			/* allow parse_value_ handler to read the value specific header */
			if (header->cmdid == MMC_OP_GETS) {
				request->parse = mmc_request_parse_value_cas;
				req->value.length = header->length - sizeof(mmc_gets_response_header_t);
			}
			else {
				request->parse = mmc_request_parse_value;
				req->value.length = header->length - sizeof(mmc_get_response_header_t);
			}
		}
		
		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}

static int mmc_request_parse_value(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the value header and then reads the value body {{{ */
{
	mmc_get_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_get_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		req->value.flags = ntohl(header->flags);
		
		/* memory for data + \0 */
		mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);

		/* allow read_value handler to read the value body */
		request->parse = mmc_request_read_value;

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_value_cas(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the value header and then reads the value body {{{ */
{
	mmc_gets_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_gets_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		req->value.flags = ntohl(header->flags);
		req->value.cas = ntohll(header->cas);
		
		/* memory for data + \0 */
		mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);

		/* allow read_value handler to read the value body */
		request->parse = mmc_request_read_value;

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_read_value(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	read the value body into the buffer {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	request->readbuf.idx += 
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length - request->readbuf.idx TSRMLS_CC);

	/* done reading? */
	if (request->readbuf.idx >= req->value.length) {
		zval *key;
		int result;

		/* allow parse_value to read next VALUE or NOOP */
		request->parse = mmc_request_parse_value_header;
		mmc_buffer_reset(&(request->readbuf));

		key = (zval *)mmc_queue_item(&(req->keys), req->command.reqid);
		result = mmc_unpack_value(
			mmc, request, &(request->readbuf), Z_STRVAL_P(key), Z_STRLEN_P(key), 
			req->value.flags, req->value.cas, req->value.length TSRMLS_CC);
		
		if (result == MMC_REQUEST_AGAIN && req->command.reqid >= req->keys.len) {
			return MMC_REQUEST_DONE;
		}

		return result;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static inline void mmc_pack_header(mmc_request_header_t *header, uint8_t cmdid, unsigned int key_len, unsigned int reqid, unsigned int length) /* {{{ */ 
{
	header->magic = MMC_REQUEST_MAGIC;
	header->cmdid = cmdid;
	header->key_len = key_len & 0xff;
	header->_reserved = 0;
	header->reqid = htonl(reqid);
	header->length = htonl(length);
}
/* }}} */

static mmc_request_t *mmc_binary_create_request() /* {{{ */ 
{
	mmc_binary_request_t *request = emalloc(sizeof(mmc_binary_request_t));
	memset(request, 0, sizeof(*request));
	return (mmc_request_t *)request;
}
/* }}} */

static void mmc_binary_reset_request(mmc_request_t *request) /* {{{ */ 
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	mmc_queue_reset(&(req->keys));
	req->value.cas = 0;
	mmc_request_reset(request);
}
/* }}} */

static void mmc_binary_free_request(mmc_request_t *request) /* {{{ */ 
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	mmc_queue_free(&(req->keys));
	mmc_request_free(request);
}
/* }}} */

static void mmc_binary_begin_get(mmc_request_t *request, int op) /* {{{ */
{
	request->parse = mmc_request_parse_value_header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	req->command.cmdid = (op == MMC_OP_GET ? MMC_OP_GETQ : MMC_OP_GETS); 
}
/* }}} */

static void mmc_binary_append_get(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len) /* {{{ */
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	/* reqid/opaque is the index into the collection of key pointers */  
	mmc_pack_header(&header, req->command.cmdid, key_len, req->keys.len, key_len);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);

	/* store key to be used by the response handler */
	mmc_queue_push(&(req->keys), zkey); 
}
/* }}} */

static void mmc_binary_end_get(mmc_request_t *request) /* {{{ */ 
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	mmc_pack_header(&header, MMC_OP_NOOP, 0, req->keys.len, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_get(mmc_request_t *request, int op, zval *zkey, const char *key, unsigned int key_len) /* {{{ */ 
{
	mmc_binary_begin_get(request, op);
	mmc_binary_append_get(request, zkey, key, key_len);
	mmc_binary_end_get(request);
}
/* }}} */

static int mmc_binary_store(
	mmc_pool_t *pool, mmc_request_t *request, int op, const char *key, unsigned int key_len, 
	unsigned int flags, unsigned int exptime, unsigned long cas, zval *value TSRMLS_DC) /* {{{ */ 
{
	int status, prevlen;
	mmc_store_request_header_t *header;
	
	request->parse = mmc_request_parse_response;
	prevlen = request->sendbuf.value.len;

	/* allocate space for header */
	if (op == MMC_OP_CAS) {
		mmc_buffer_alloc(&(request->sendbuf), sizeof(mmc_cas_request_header_t));
		request->sendbuf.value.len += sizeof(mmc_cas_request_header_t);
	}
	else {
		mmc_buffer_alloc(&(request->sendbuf), sizeof(*header));
		request->sendbuf.value.len += sizeof(*header);
	}
	
	/* append key and data */
	smart_str_appendl(&(request->sendbuf.value), key, key_len);
	status = mmc_pack_value(pool, &(request->sendbuf), value, &flags TSRMLS_CC);
	
	if (status != MMC_OK) {
		return status;
	}
	
	/* initialize header */
	header = (mmc_store_request_header_t *)(request->sendbuf.value.c + prevlen);
	mmc_pack_header(&(header->base), op, key_len, 0, request->sendbuf.value.len - prevlen - sizeof(header->base));
	
	header->flags = htonl(flags);
	header->exptime = htonl(exptime);
	
	if (op == MMC_OP_CAS) {
		((mmc_cas_request_header_t *)header)->cas = htonll(cas); 
	}
	
	return MMC_OK;
}
/* }}} */

static void mmc_binary_delete(mmc_request_t *request, const char *key, unsigned int key_len, unsigned int exptime) /* {{{ */ 
{
	mmc_delete_request_header_t header;
	request->parse = mmc_request_parse_response;
	
	mmc_pack_header(&(header.base), MMC_OP_DELETE, key_len, 0, sizeof(header) - sizeof(header.base) + key_len);
	header.exptime = htonl(exptime);
	
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);
}
/* }}} */

static void mmc_binary_mutate(mmc_request_t *request, const char *key, unsigned int key_len, long value, long defval, unsigned int exptime) /* {{{ */ 
{
	mmc_mutate_request_header_t header;
	request->parse = mmc_request_parse_response;
	
	mmc_pack_header(&(header.base), MMC_OP_MUTATE, key_len, 0, sizeof(header) - sizeof(header.base) + key_len);
	header.value = htonll(value);
	header.defval = htonll(defval);
	header.exptime = htonl(exptime);
	
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);
}
/* }}} */

static void mmc_binary_flush(mmc_request_t *request, unsigned int exptime) /* {{{ */ 
{
	mmc_flush_request_header_t header;
	request->parse = mmc_request_parse_response;
	
	mmc_pack_header(&(header.base), MMC_OP_FLUSH, 0, 0, sizeof(header) - sizeof(header.base));
	header.exptime = htonl(exptime);
	
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_version(mmc_request_t *request) /* {{{ */ 
{
	mmc_request_header_t header;
	request->parse = mmc_request_parse_response;
	
	mmc_pack_header(&header, MMC_OP_VERSION, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_stats(mmc_request_t *request, const char *type, long slabid, long limit) /* {{{ */
{
	/* stats not supported */
	mmc_request_header_t header;
	request->parse = mmc_request_parse_response;
	
	mmc_pack_header(&header, MMC_OP_NOOP, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

mmc_protocol_t mmc_binary_protocol = {
	mmc_binary_create_request,
	mmc_binary_reset_request,
	mmc_binary_free_request,
	mmc_binary_get,
	mmc_binary_begin_get,
	mmc_binary_append_get,
	mmc_binary_end_get,
	mmc_binary_store,
	mmc_binary_delete,
	mmc_binary_mutate,
	mmc_binary_flush,
	mmc_binary_version,
	mmc_binary_stats
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
