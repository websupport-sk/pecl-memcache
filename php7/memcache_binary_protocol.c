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

#define MMC_DEBUG 0

#ifdef PHP_WIN32
#include <win32/php_stdint.h>
#include <winsock2.h>
#else
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include "memcache_pool.h"
#include "ext/standard/php_smart_string.h"

#ifdef htonll
#undef htonll
#endif

#ifdef ntohll
#undef ntohll
#endif

#ifndef PHP_WIN32
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
#else
uint64_t mmc_htonll(uint64_t value);
# define ntohll mmc_htonll
# define htonll mmc_htonll
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
#define MMC_BIN_OP_APPEND		0x0e
#define MMC_BIN_OP_PREPEND		0x0f


#define MMC_OP_SASL_LIST		0x20
#define MMC_OP_SASL_AUTH		0x21
#define MMC_OP_SASL_AUTH_STEP	0x21

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
	uint64_t	cas;
} mmc_request_header_t;

typedef struct mmc_get_request_header {
	mmc_request_header_t	base;
} mmc_get_request_header_t;

typedef struct mmc_version_request_header {
	mmc_request_header_t	base;
} mmc_version_request_header_t;

typedef struct mmc_store_request_header {
	mmc_request_header_t	base;
	uint32_t				flags;
	uint32_t				exptime;
} mmc_store_request_header_t;

typedef struct mmc_store_append_header {
	mmc_request_header_t	base;
} mmc_store_append_header_t;

typedef struct mmc_delete_request_header {
	mmc_request_header_t	base;
	uint32_t				exptime;
} mmc_delete_request_header_t;

typedef struct mmc_mutate_request_header {
	mmc_request_header_t	base;
	uint64_t				delta;
	uint64_t				initial;
	uint32_t				expiration;
} mmc_mutate_request_header_t;

typedef struct mmc_sasl_request_header {
	mmc_request_header_t	base;
} mmc_sasl_request_header;

typedef struct mmc_response_header {
	uint8_t		magic;
	uint8_t		opcode;
	uint16_t	error;
	uint8_t		extras_len;
	uint8_t		datatype;
	uint16_t	status;
	uint32_t	length;				/* trailing body length (not including this header) */
	uint32_t	reqid;				/* echo'ed from request */
	uint64_t	cas;
} mmc_response_header_t;

typedef struct mmc_get_response_header {
	uint32_t				flags;
} mmc_get_response_header_t;

typedef struct mmc_mutate_response_header {
	uint64_t				value;
} mmc_mutate_response_header_t;

static int mmc_request_read_response(mmc_t *, mmc_request_t *);
static int mmc_request_parse_value(mmc_t *, mmc_request_t *);
static int mmc_request_read_value(mmc_t *, mmc_request_t *);

void mmc_binary_hexdump(void *mem, unsigned int len)
{
#	define HEXDUMP_COLS 4
	unsigned int i, j;
	for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		if(i % HEXDUMP_COLS == 0) {
			printf("%06i: ", i);
		}

		if(i < len) {
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		} else {
			printf("   ");
		}

		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if(j >= len) {
					putchar(' ');
				} else if(isprint(((char*)mem)[j])) {
					putchar(0xFF & ((char*)mem)[j]);
				} else {
					putchar('.');
				}
			}
			putchar('\n');
		}
	}
}
#ifdef PHP_WIN32
uint64_t mmc_htonll(uint64_t value)
{
	if (value == 0) {
		return 0x0LL;
	} else {
		static const int num = 42;

		// Check the endianness
		if (*(const char *)(&num) == num) {
			const uint32_t high_part = htonl((uint32_t)(value >> 32));
			const uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));

			return ((uint64_t)(low_part) << 32) | high_part;
		} else {
			return value;
		}
	}
}
#endif

static inline char *mmc_stream_get(mmc_stream_t *io, size_t bytes) /*
	attempts to read a number of bytes from server, returns the a pointer to the buffer on success, NULL if the complete number of bytes could not be read {{{ */
{
	io->input.idx += io->read(io, io->input.value + io->input.idx, bytes - io->input.idx);

	if (io->input.idx >= bytes) {
		io->input.idx = 0;
		return io->input.value;
	}

	return NULL;
}
/* }}} */

static int mmc_request_parse_response(mmc_t *mmc, mmc_request_t *request) /*
	reads a generic response header and reads the response body {{{ */
{
	mmc_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_response_header_t *)mmc_stream_get(request->io, sizeof(mmc_response_header_t));

	if (header != NULL) {
		if (header->magic != MMC_RESPONSE_MAGIC) {
			return mmc_server_failure(mmc, request->io, "Malformed server response (invalid magic byte)", 0);
		}

		if (header->opcode == MMC_OP_NOOP) {
			return MMC_REQUEST_DONE;
		}

		req->command.opcode = header->opcode;
		req->command.error = ntohs(header->error);
		req->command.reqid = ntohl(header->reqid);
		req->value.length = ntohl(header->length);
		req->value.cas = ntohll(header->cas);

		if (req->value.length == 0) {
			return request->response_handler(mmc, request, req->command.error, "", 0, request->response_handler_param);
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

static int mmc_request_parse_null(mmc_t *mmc, mmc_request_t *request) /*
	always returns MMC_REQUEST_DONE {{{ */
{
	return MMC_REQUEST_DONE;
}

static int mmc_request_read_response(mmc_t *mmc, mmc_request_t *request) /*
	read the response body into the buffer and delegates to response_handler {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->readbuf.idx +=
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length - request->readbuf.idx);

	/* done reading? */
	if (request->readbuf.idx >= req->value.length) {
		request->readbuf.value.c[req->value.length] = '\0';
		return request->response_handler(mmc, request, req->command.error, request->readbuf.value.c, req->value.length, request->response_handler_param);
	}

	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_read_mutate(mmc_t *mmc, mmc_request_t *request) /*
	reads and parses the mutate response header {{{ */
{
	mmc_mutate_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_mutate_response_header_t *)mmc_stream_get(request->io, sizeof(*header));
	if (header != NULL) {
		int result;
		zval *key, value;

		/* convert remembered key to string and unpack value */
		key = (zval *)mmc_queue_item(&(req->keys), req->command.reqid);

		ZVAL_LONG(&value, ntohll(header->value));

		if (Z_TYPE_P(key) != IS_STRING) {
			zval keytmp = *key;

			zval_copy_ctor(&keytmp);
			convert_to_string(&keytmp);

			result = request->value_handler(
				Z_STRVAL(keytmp), Z_STRLEN(keytmp), &value,
				req->value.flags, req->value.cas, request->value_handler_param);

			zval_dtor(&keytmp);
		}
		else {
			result = request->value_handler(
				Z_STRVAL_P(key), Z_STRLEN_P(key), &value,
				req->value.flags, req->value.cas, request->value_handler_param);
		}

		return result;
	}

	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_parse_value(mmc_t *mmc, mmc_request_t *request) /*
	reads and parses the value response header and then reads the value body {{{ */
{
	mmc_get_response_header_t *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	header = (mmc_get_response_header_t *)mmc_stream_get(request->io, sizeof(mmc_get_response_header_t));
	if (header != NULL) {
		//req->value.cas = ntohll(header->cas);
		req->value.flags = ntohl(header->flags);

		/* allow read_value handler to read the value body */
		request->parse = mmc_request_read_value;

		/* read more, php streams buffer input which must be read if available */
		return MMC_REQUEST_AGAIN;
	}

	return MMC_REQUEST_MORE;
}
/* }}}*/

static int mmc_request_read_value(mmc_t *mmc, mmc_request_t *request) /*
	read the value body into the buffer {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->readbuf.idx +=
		request->io->read(request->io, request->readbuf.value.c + request->readbuf.idx, req->value.length - request->readbuf.idx);

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
			convert_to_string(&keytmp);

			result = mmc_unpack_value(
				mmc, request, &(request->readbuf), Z_STRVAL(keytmp), Z_STRLEN(keytmp),
				req->value.flags, req->value.cas, req->value.length);

			zval_dtor(&keytmp);
		}
		else {
			result = mmc_unpack_value(
				mmc, request, &(request->readbuf), Z_STRVAL_P(key), Z_STRLEN_P(key),
				req->value.flags, req->value.cas, req->value.length);
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
	ZEND_SECURE_ZERO(request, sizeof(mmc_binary_request_t));
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
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(mmc_request_header_t));
	smart_string_appendl(&(request->sendbuf.value), key, key_len);

	/* store key to be used by the response handler */
	mmc_queue_push(&(req->keys), zkey);
}
/* }}} */

static void mmc_binary_end_get(mmc_request_t *request) /* {{{ */
{
	mmc_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	mmc_pack_header(&header, MMC_OP_NOOP, req->keys.len, 0, 0, 0);
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
}
/* }}} */

static void mmc_binary_get(mmc_request_t *request, int op, zval *zkey, const char *key, unsigned int key_len) /* {{{ */
{
	mmc_get_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_parse_value;

	/* reqid/opaque is the index into the collection of key pointers */
	mmc_pack_header(&(header.base), MMC_OP_GET, req->keys.len, key_len, 0, 0);
	header.base.cas = 0x0;

	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(mmc_get_request_header_t));
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
#if MMC_DEBUG
	mmc_binary_hexdump(request->sendbuf.value.c, request->sendbuf.value.len);
#endif
	/* store key to be used by the response handler */
	mmc_queue_push(&(req->keys), zkey);
}
/* }}} */

static int mmc_binary_store(
	mmc_pool_t *pool, mmc_request_t *request, int op, const char *key, unsigned int key_len,
	unsigned int flags, unsigned int exptime, unsigned long cas, zval *value) /* {{{ */
{
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	int status, prevlen, valuelen;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;

	prevlen = request->sendbuf.value.len;

/*  placeholder for upcoming append/prepend support */
#if 1
	if (op == MMC_OP_APPEND || op == MMC_OP_PREPEND) {
		mmc_store_append_header_t *header;

		if (op == MMC_OP_APPEND) {
			op = MMC_BIN_OP_APPEND;
		} else {
			op = MMC_BIN_OP_PREPEND;
		}

		/* allocate space for header */
		mmc_buffer_alloc(&(request->sendbuf), sizeof(mmc_store_append_header_t));
		request->sendbuf.value.len += sizeof(mmc_store_append_header_t);

		/* append key and data */
		smart_string_appendl(&(request->sendbuf.value), key, key_len);

		valuelen = request->sendbuf.value.len;
		status = mmc_pack_value(pool, &(request->sendbuf), value, &flags);

		if (status != MMC_OK) {
			return status;
		}

		header = (mmc_store_append_header_t *)(request->sendbuf.value.c + prevlen);

		mmc_pack_header(&(header->base), op, 0, key_len, sizeof(mmc_store_append_header_t) - sizeof(mmc_request_header_t), request->sendbuf.value.len - valuelen);
		header->base.cas = htonll(cas);
		
#if MMC_DEBUG
		mmc_binary_hexdump(request->sendbuf.value.c, request->sendbuf.value.len);
#endif
		/* todo */
		return MMC_OK;
	} else
#endif
	{
		mmc_store_request_header_t *header;

		/* allocate space for header */
		mmc_buffer_alloc(&(request->sendbuf), sizeof(mmc_store_request_header_t));
		request->sendbuf.value.len += sizeof(mmc_store_request_header_t);

		/* append key and data */
		smart_string_appendl(&(request->sendbuf.value), key, key_len);

		valuelen = request->sendbuf.value.len;
		status = mmc_pack_value(pool, &(request->sendbuf), value, &flags);

		if (status != MMC_OK) {
			return status;
		}

		/* initialize header */
		header = (mmc_store_request_header_t *)(request->sendbuf.value.c + prevlen);

		if (op == MMC_OP_CAS) {
			op = MMC_OP_SET;
		}

		mmc_pack_header(&(header->base), op, 0, key_len, sizeof(mmc_store_request_header_t) - sizeof(mmc_request_header_t), request->sendbuf.value.len - valuelen);

		header->base.cas = htonll(cas);
		header->flags = htonl(flags);
		header->exptime = htonl(exptime);

		return MMC_OK;
	}
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

	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
	smart_string_appendl(&(request->sendbuf.value), key, key_len);
}
/* }}} */

static void mmc_binary_mutate(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len, long value, long defval, int defval_used, unsigned int exptime) /* {{{ */
{
	mmc_mutate_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;
	uint8_t op;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_mutate;

	if (value >= 0) {
		op = MMC_OP_INCR;
		header.delta = htonll((uint64_t)value);
		//header.delta = (uint64_t)value;
	} else {
		op = MMC_OP_DECR;
		header.delta = htonll((uint64_t)-value);
	}

	/* extra is always 20 bytes 
	https://code.google.com/p/memcached/wiki/BinaryProtocolRevamped#Increment,_Decrement
	TODO: add flags to do not force alignments so we can rely on sizeof instead of 
	fixed sizes, safer&cleaner */
	mmc_pack_header(&(header.base), op, req->keys.len, key_len,  20, 0);
	header.base.cas = 0x0;
	header.initial = htonll((int64_t)defval);

	if (defval_used) {
		/* server inserts defval if key doesn't exist */
		header.expiration = htonl(exptime);
	}
	else {
		/* server replies with NOT_FOUND if exptime ~0 and key doesn't exist */
		header.expiration = ~(uint32_t)0;
		header.expiration = htonl(0x00000e10);
	}

	/* mutate request is 43 bytes */
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, 44);
	smart_string_appendl(&(request->sendbuf.value), key, key_len);

#if MMC_DEBUG
	mmc_binary_hexdump(request->sendbuf.value.c, request->sendbuf.value.len);
#endif

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
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
}
/* }}} */

static void mmc_binary_version(mmc_request_t *request) /* {{{ */
{
	mmc_version_request_header_t header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;

	mmc_pack_header(&(header.base), MMC_OP_VERSION, 0, 0, 0, 0);
	header.base.cas = 0x0;
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
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
	smart_string_appendl(&(request->sendbuf.value), (const char *)&header, sizeof(header));
}
/* }}} */

static void mmc_set_sasl_auth_data(mmc_pool_t *pool, mmc_request_t *request, const char *user,  const char *password) /* {{{ */
{
	const unsigned int key_len = 5;
	int prevlen;
	mmc_sasl_request_header *header;
	mmc_binary_request_t *req = (mmc_binary_request_t *)request;

	request->parse = mmc_request_parse_response;
	req->next_parse_handler = mmc_request_read_response;

	memcpy(request->key, "PLAIN", 5 + 1);

	prevlen = request->sendbuf.value.len;

	/* allocate space for header */
	mmc_buffer_alloc(&(request->sendbuf), sizeof(*header));
	request->sendbuf.value.len += sizeof(*header);

	/* append key and data */
	smart_string_appendl(&(request->sendbuf.value), "PLAIN", 5);

	/* initialize header */
	header = (mmc_sasl_request_header *)(request->sendbuf.value.c + prevlen);

	(header->base).magic = MMC_REQUEST_MAGIC;
	(header->base).opcode = MMC_OP_SASL_AUTH;
	(header->base).key_len = htons(key_len);
	(header->base).extras_len = 0x0;
	(header->base).datatype = 0x0;
	(header->base)._reserved = 0x0;

	(header->base).length = htonl(strlen(user) + strlen(password) + key_len + 2);
	(header->base).reqid = htonl(0);
	header->base.cas = 0x0;

	smart_string_appendl(&(request->sendbuf.value), "\0", 1);
	smart_string_appendl(&(request->sendbuf.value), user, strlen(user));
	smart_string_appendl(&(request->sendbuf.value), "\0", 1);
	smart_string_appendl(&(request->sendbuf.value), password, strlen(password));

#if MMC_DEBUG
	mmc_binary_hexdump(request->sendbuf.value.c, request->sendbuf.value.len);
#endif
	return;
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
	mmc_binary_stats,
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
