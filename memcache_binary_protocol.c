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

#define MMC_REQUEST_MAGIC	0x80
#define	MMC_RESPONSE_MAGIC	0x81

#define MMC_OP_GET			0x00
#define MMC_OP_SET			0x01
#define MMC_OP_ADD			0x02
#define MMC_OP_REPLACE		0x03
#define MMC_OP_DELETE		0x04
#define MMC_OP_INCR			0x05
#define MMC_OP_DECR			0x06
#define MMC_OP_QUIT			0x07
#define MMC_OP_FLUSH		0x08
#define MMC_OP_GETQ			0x09
#define MMC_OP_NOOP			0x0a
#define MMC_OP_VERSION		0x0b

typedef struct mmc_binary_request {
	mmc_request_t		base;					/* enable cast to mmc_request_t */
	mmc_request_parser	next_parse_handler;		/* next payload parser state */
	mmc_queue_t			keys;					/* mmc_queue_t<zval *>, reqid -> key mappings */
	struct {
		uint8_t			opcode;
		uint8_t			error;					/* error received in current request */
		uint32_t		reqid;					/* current reqid being processed */
	} command;
	struct {									/* stores value info while the body is being read */
		unsigned int	flags;
		unsigned long	length;
		uint64_t		cas;					/* CAS counter */
	} value;
} mmc_binary_request_t;

typedef struct mmc_request_header {
	uint8_t		magic;
	uint8_t		opcode;
	uint16_t	key_len;
	uint8_t		extras_len;
	uint8_t		datatype;
	uint16_t	_reserved;
	uint32_t	length;							/* trailing body length (not including this header) */
	uint32_t	reqid;							/* opaque request id */
} mmc_request_header_t;

typedef struct mmc_store_request_header {
	mmc_request_header_t	base;
	uint64_t				cas;
	uint32_t				flags;
	uint32_t				exptime;
} mmc_store_request_header_t;

typedef struct mmc_delete_request_header {
	mmc_request_header_t	base;
	uint32_t				exptime;
} mmc_delete_request_header_t;

typedef struct mmc_mutate_request_header {
	mmc_request_header_t	base;
	uint64_t				value;
	uint64_t				defval;
	uint32_t				exptime;
} mmc_mutate_request_header_t;

typedef struct mmc_response_header {
	uint8_t		magic;
	uint8_t		opcode;
	uint16_t	error;
	uint8_t		extras_len;
	uint8_t		datatype;
	uint16_t	_reserved;
	uint32_t	length;				/* trailing body length (not including this header) */
	uint32_t	reqid;				/* echo'ed from request */
} mmc_response_header_t;

typedef struct mmc_get_response_header {
	uint64_t				cas;
	uint32_t				flags;
} mmc_get_response_header_t;

typedef struct mmc_mutate_response_header {
	uint64_t				value;
} mmc_mutate_response_header_t;

static int mmc_request_read_response(mmc_t *, mmc_request_t * TSRMLS_DC);
static int mmc_request_parse_value(mmc_t *, mmc_request_t * TSRMLS_DC);
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

static int mmc_request_parse_response(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads a generic response header and reads the response body {{{ */
{
	mmc_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		if (header->magic != MMC_RESPONSE_MAGIC) {
			return mmc_server_failure(mmc, request->io, "Malformed server response (invalid magic byte)", 0 TSRMLS_CC);
		}
		
		if (header->opcode == MMC_OP_NOOP) {
			return MMC_REQUEST_DONE;
		}

		req->command.opcode = header->opcode;
		req->command.error = ntohs(header->error);
		req->command.reqid = ntohl(header->reqid);
		req->value.length = ntohl(header->length);
		
		if (req->value.length == 0) {
			return request->response_handler(mmc, request, req->command.error, "", 0, request->response_handler_param TSRMLS_CC);
		}

		/* allow read_response handler to read the response body */
		if (req->command.error) {
			request->parse = mmc_request_read_response;
			mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);
		}
		else {
			request->parse = req->next_parse_handler;

			if (req->value.length >= header->extras_len) {
				req->value.length -= header->extras_len;
			}

			mmc_buffer_alloc(&(request->readbuf), req->value.length + 1);
		}

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_null(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	always returns MMC_REQUEST_DONE {{{ */
{
	return MMC_REQUEST_DONE;
}

static int mmc_request_read_response(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	read the response body into the buffer and delegates to response_handler {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	request->readbuf.idx += 
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length - request->readbuf.idx TSRMLS_CC);

	/* done reading? */
	if (request->readbuf.idx >= req->value.length) {
		request->readbuf.value.c[req->value.length] = '\0';
		return request->response_handler(mmc, request, req->command.error, request->readbuf.value.c, req->value.length, request->response_handler_param TSRMLS_CC);
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_read_mutate(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the mutate response header {{{ */
{
	mmc_mutate_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_mutate_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		int result;
		zval *key, value;

		/* convert remembered key to string and unpack value */
		key = (zval *)mmc_queue_item(&(req->keys), req->command.reqid);
		
		INIT_PZVAL(&value);
		ZVAL_LONG(&value, ntohll(header->value));
		
		if (Z_TYPE_P(key) != IS_STRING) {
			zval keytmp = *key;
			
			zval_copy_ctor(&keytmp);
			INIT_PZVAL(&keytmp);
			convert_to_string(&keytmp);

			result = request->value_handler(
				Z_STRVAL(keytmp), Z_STRLEN(keytmp), &value, 
				req->value.flags, req->value.cas, request->value_handler_param TSRMLS_CC);
			
			zval_dtor(&keytmp);
		}
		else {
			result = request->value_handler(
				Z_STRVAL_P(key), Z_STRLEN_P(key), &value,
				req->value.flags, req->value.cas, request->value_handler_param TSRMLS_CC);
		}
		
		return result;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_value(mmc_t *mmc, mmc_request_t *request TSRMLS_DC) /* 
	reads and parses the value response header and then reads the value body {{{ */
{
	mmc_get_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_get_response_header_t *)mmc_stream_get(request->io, sizeof(*header) TSRMLS_CC);
	if (header != NULL) {
		req->value.cas = ntohll(header->cas);
		req->value.flags = ntohl(header->flags);
		
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

		/* allow parse_value to read next VALUE or NOOP, done here to ensure reentrancy */
		if (req->command.opcode == MMC_OP_GET) {
			request->parse = mmc_request_parse_null;
		}
		else {
			request->parse = mmc_request_parse_response;
		}
		mmc_buffer_reset(&(request->readbuf));

		/* convert remembered key to string and unpack value */
		key = (zval *)mmc_queue_item(&(req->keys), req->command.reqid);
		if (Z_TYPE_P(key) != IS_STRING) {
			zval keytmp = *key;
			
			zval_copy_ctor(&keytmp);
			INIT_PZVAL(&keytmp);
			convert_to_string(&keytmp);

			result = mmc_unpack_value(
				mmc, request, &(request->readbuf), Z_STRVAL(keytmp), Z_STRLEN(keytmp), 
				req->value.flags, req->value.cas, req->value.length TSRMLS_CC);
			
			zval_dtor(&keytmp);
		}
		else {
			result = mmc_unpack_value(
				mmc, request, &(request->readbuf), Z_STRVAL_P(key), Z_STRLEN_P(key), 
				req->value.flags, req->value.cas, req->value.length TSRMLS_CC);
		}
		
		if (result == MMC_REQUEST_DONE && (req->command.opcode == MMC_OP_GET || req->command.reqid >= req->keys.len)) {
			return MMC_REQUEST_DONE;
		}

		return MMC_REQUEST_AGAIN;
	}
	
	return MMC_REQUEST_MORE;
}
/* }}}*/

static inline void mmc_pack_header(mmc_request_header_t *header, uint8_t opcode, unsigned int reqid, unsigned int key_len, unsigned int extras_len, unsigned int length) /* {{{ */ 
{
	header->magic = MMC_REQUEST_MAGIC;
	header->opcode = opcode;
	header->key_len = htons(key_len);
	header->extras_len = extras_len;
	header->datatype = 0;
	header->_reserved = 0;
	header->length = htonl(key_len + extras_len + length);
	header->reqid = htonl(reqid);
}
/* }}} */

static mmc_request_t *mmc_binary_create_request() /* {{{ */ 
{
	mmc_binary_request_t *request = emalloc(sizeof(mmc_binary_request_t));
	memset(request, 0, sizeof(*request));
	return (mmc_request_t *)request;
}
/* }}} */

static void mmc_binary_clone_request(mmc_request_t *clone, mmc_request_t *request) /* {{{ */ 
{
	mmc_binary_request_t *rcl = (mmc_binary_request_t *)clone;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	
	rcl->next_parse_handler = req->next_parse_handler;
	mmc_queue_copy(&(rcl->keys), &(req->keys));
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
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_parse_value;
}
/* }}} */

static void mmc_binary_append_get(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len) /* {{{ */
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	/* reqid/opaque is the index into the collection of key pointers */  
	mmc_pack_header(&header, MMC_OP_GETQ, req->keys.len, key_len, 0, 0);
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
	mmc_pack_header(&header, MMC_OP_NOOP, req->keys.len, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_get(mmc_request_t *request, int op, zval *zkey, const char *key, unsigned int key_len) /* {{{ */ 
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_parse_value;

	/* reqid/opaque is the index into the collection of key pointers */  
	mmc_pack_header(&header, MMC_OP_GET, req->keys.len, key_len, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);

	/* store key to be used by the response handler */
	mmc_queue_push(&(req->keys), zkey); 
}
/* }}} */

static int mmc_binary_store(
	mmc_pool_t *pool, mmc_request_t *request, int op, const char *key, unsigned int key_len, 
	unsigned int flags, unsigned int exptime, unsigned long cas, zval *value TSRMLS_DC) /* {{{ */ 
{
	int status, prevlen, valuelen;
	mmc_store_request_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	
	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;

	prevlen = request->sendbuf.value.len;
	
	/* allocate space for header */
	mmc_buffer_alloc(&(request->sendbuf), sizeof(*header));
	request->sendbuf.value.len += sizeof(*header);
	
	/* append key and data */
	smart_str_appendl(&(request->sendbuf.value), key, key_len);
	
	valuelen = request->sendbuf.value.len;
	status = mmc_pack_value(pool, &(request->sendbuf), value, &flags TSRMLS_CC);
	
	if (status != MMC_OK) {
		return status;
	}
	
	/* initialize header */
	header = (mmc_store_request_header_t *)(request->sendbuf.value.c + prevlen);
	
	switch (op) {
		case MMC_OP_CAS:
			op = MMC_OP_SET;
			break;
			
		case MMC_OP_APPEND:
		case MMC_OP_PREPEND:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Binary protocol doesn't support append/prepend");
			return MMC_REQUEST_FAILURE;
	}
	
	mmc_pack_header(&(header->base), op, 0, key_len, sizeof(*header) - sizeof(header->base), request->sendbuf.value.len - valuelen);
	
	header->cas = htonll(cas); 
	header->flags = htonl(flags);
	header->exptime = htonl(exptime);
	
	return MMC_OK;
}
/* }}} */

static void mmc_binary_delete(mmc_request_t *request, const char *key, unsigned int key_len, unsigned int exptime) /* {{{ */ 
{
	mmc_delete_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;
	
	mmc_pack_header(&(header.base), MMC_OP_DELETE, 0, key_len, sizeof(header) - sizeof(header.base), 0);
	header.exptime = htonl(exptime);
	
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);
}
/* }}} */

static void mmc_binary_mutate(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len, long value, long defval, int defval_used, unsigned int exptime) /* {{{ */ 
{
	mmc_mutate_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	
	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_mutate;

	if (value >= 0) {
		mmc_pack_header(&(header.base), MMC_OP_INCR, req->keys.len, key_len, sizeof(header) - sizeof(header.base), 0);
		header.value = htonll(value);
	}
	else {
		mmc_pack_header(&(header.base), MMC_OP_DECR, req->keys.len, key_len, sizeof(header) - sizeof(header.base), 0);
		header.value = htonll(-value);
	}
		
	header.defval = htonll(defval);
	
	if (defval_used) {
		/* server inserts defval if key doesn't exist */
		header.exptime = htonl(exptime);
	}
	else {
		/* server replies with NOT_FOUND if exptime ~0 and key doesn't exist */
		header.exptime = ~(uint32_t)0;
	}
	
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
	smart_str_appendl(&(request->sendbuf.value), key, key_len);

	/* store key to be used by the response handler */
	mmc_queue_push(&(req->keys), zkey); 
}
/* }}} */

static void mmc_binary_flush(mmc_request_t *request, unsigned int exptime) /* {{{ */ 
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	
	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;
	
	mmc_pack_header(&header, MMC_OP_FLUSH, 0, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_version(mmc_request_t *request) /* {{{ */ 
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;
	
	mmc_pack_header(&header, MMC_OP_VERSION, 0, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

static void mmc_binary_stats(mmc_request_t *request, const char *type, long slabid, long limit) /* {{{ */
{
	/* stats not supported */
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;
	
	mmc_pack_header(&header, MMC_OP_NOOP, 0, 0, 0, 0);
	smart_str_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));	
}
/* }}} */

mmc_protocol_t mmc_binary_protocol = {
	mmc_binary_create_request,
	mmc_binary_clone_request,
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
