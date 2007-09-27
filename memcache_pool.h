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

#ifndef MEMCACHE_POOL_H
#define MEMCACHE_POOL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>

#include "php.h"
#include "ext/standard/php_smart_str_public.h"
#include "memcache_queue.h"

#ifndef ZSTR
#define ZSTR
#define ZSTR_VAL(v) v
#define zstr char *
#else
#define ZSTR_VAL(v) (v).s
#endif

#define MMC_SERIALIZED 1
#define MMC_COMPRESSED 2

#define MMC_BUFFER_SIZE 4096
#define MMC_MAX_UDP_LEN 1400
#define MMC_MAX_KEY_LEN 250
#define MMC_VALUE_HEADER "VALUE %250s %u %lu"	/* keep in sync with MMC_MAX_KEY_LEN */

#define MMC_COMPRESSION_LEVEL Z_DEFAULT_COMPRESSION
#define MMC_DEFAULT_SAVINGS 0.2				/* minimum 20% savings for compression to be used */

#define MMC_PROTO_TCP 0
#define MMC_PROTO_UDP 1

#define MMC_STATUS_FAILED -1				/* server/stream is down */
#define MMC_STATUS_DISCONNECTED 0			/* stream is disconnected, ie. new connection */
#define MMC_STATUS_UNKNOWN 1				/* stream is in unknown state, ie. non-validated persistent connection */
#define MMC_STATUS_CONNECTED 2				/* stream is connected */

#define MMC_OK 0

#define MMC_REQUEST_FAILURE -1				/* operation failed, failover to other server */
#define MMC_REQUEST_DONE 0					/* ok result, or reading/writing is done */
#define MMC_REQUEST_MORE 1					/* more data follows in another packet, try select() again */
#define MMC_REQUEST_AGAIN 2					/* more data follows in this packet, try read/write again */
#define MMC_REQUEST_RETRY 3					/* retry/reschedule request */

#define MMC_STANDARD_HASH 1
#define MMC_CONSISTENT_HASH 2
#define MMC_HASH_CRC32 1					/* CRC32 hash function */
#define MMC_HASH_FNV1A 2					/* FNV-1a hash function */

#define MMC_CONSISTENT_POINTS 160			/* points per server */
#define MMC_CONSISTENT_BUCKETS 1024			/* number of precomputed buckets, should be power of 2 */

/* io buffer */
typedef struct mmc_buffer {
	smart_str		value;
	unsigned int	idx;					/* current index */
} mmc_buffer_t;

#define mmc_buffer_release(b) memset((b), 0, sizeof(*(b)))
#define mmc_buffer_reset(b) (b)->value.len = (b)->idx = 0   

/* stream handlers */
typedef struct mmc_stream mmc_stream_t;

typedef size_t (*mmc_stream_read)(mmc_stream_t *stream, char *buf, size_t count TSRMLS_DC);
typedef char *(*mmc_stream_readline)(mmc_stream_t *stream, char *buf, size_t maxlen, size_t *retlen TSRMLS_DC);

/* stream container */
struct mmc_stream {
	php_stream				*stream;
	int						fd;							/* file descriptor for select() */
	unsigned short			port;
	int						chunk_size;					/* stream chunk size */
	int						status;
	long					failed;						/* the timestamp the stream was marked as failed */
	long					retry_interval;				/* seconds to wait before automatic reconnect */
	mmc_buffer_t			buffer;						/* read buffer (when using udp) */
	mmc_stream_read			read;						/* handles reading from stream */ 
	mmc_stream_readline		readline;					/* handles reading lines from stream */ 
	struct {											/* temporary input buffer, see mmc_server_readline() */
		char				value[MMC_BUFFER_SIZE];
		int					idx;
	} input;
};

/* request handlers */
typedef struct mmc mmc_t;
typedef struct mmc_pool mmc_pool_t;
typedef struct mmc_request mmc_request_t;

typedef int (*mmc_request_reader)(mmc_t *mmc, mmc_request_t *request TSRMLS_DC);
typedef int (*mmc_request_parser)(mmc_t *mmc, mmc_request_t *request TSRMLS_DC);
typedef int (*mmc_request_value_handler)(mmc_t *mmc, mmc_request_t *request, void *value, unsigned int value_len, void *param TSRMLS_DC);
typedef int (*mmc_request_failover_handler)(mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param TSRMLS_DC);

/* server request */
struct mmc_request {
	mmc_stream_t				*io;						/* stream this request is sending on */
	mmc_buffer_t				sendbuf;					/* the command to send to server */
	mmc_buffer_t				readbuf;					/* used when reading values */
	char						key[MMC_MAX_KEY_LEN + 1];	/* key buffer to use on failover of single-key requests */
	unsigned int				key_len;
	unsigned int				protocol;					/* protocol encoding of request */
	mmc_queue_t					failed_servers;				/* servers this request has failed at */
	unsigned int				failed_index;				/* last index probed on failure */
	mmc_request_reader			read;						/* handles reading (and validating datagrams) */
	mmc_request_parser			parse;						/* parses read values */
	mmc_request_value_handler		value_handler;					/* called when values has been parsed */
	void							*value_handler_param;			/* parameter to value handler */
	mmc_request_failover_handler	failover_handler;				/* called to perform failover of failed request */
	void							*failover_handler_param;		/* parameter to failover handler */
	struct {														/* stores value info while it's body is being read */
		char			key[MMC_MAX_KEY_LEN + 1];
		unsigned int	key_len;
		unsigned int	flags;
		unsigned long	bytes;
	} value;
	struct {
		uint16_t		reqid;					/* id of the request, increasing value set by client */
		uint16_t		seqid;					/* next expected seqid */
		uint16_t		total;					/* number of datagrams in response */
	} udp;
};

/* udp protocol header */
typedef struct mmc_udp_header {
	uint16_t	reqid;							/* id of the request, increasing value set by client */
	uint16_t	seqid;							/* index of this datagram in response */
	uint16_t	total;							/* number of datagrams in response */
	uint16_t	_reserved;
} mmc_udp_header_t;

/* server */
struct mmc {
	mmc_stream_t		tcp;					/* main stream, might be tcp or unix socket */
	mmc_stream_t		udp;					/* auxiliary udp connection */
	mmc_request_t		*sendreq;				/* currently sending request, NULL if none */
	mmc_request_t		*readreq;				/* currently reading request, NULL if none */
	mmc_request_t		*buildreq;				/* request currently being built, NULL if none */
	mmc_queue_t			sendqueue;				/* mmc_queue_t<mmc_request_t *>, requests waiting to be sent */
	mmc_queue_t			readqueue;				/* mmc_queue_t<mmc_request_t *>, requests waiting to be read */
	char				*host;
	long				timeout;				/* network timeout */
	int					persistent;
	uint16_t			reqid;					/* next sequential request id */
	char				*error;					/* last error message */
	int					errnum;					/* last error code */
};

/* hashing strategy */
typedef unsigned int (*mmc_hash_function)(const char *, int);
typedef void * (*mmc_hash_create_state)(mmc_hash_function);
typedef void (*mmc_hash_free_state)(void *);
typedef mmc_t * (*mmc_hash_find_server)(void *, const char *, int TSRMLS_DC);
typedef void (*mmc_hash_add_server)(void *, mmc_t *, unsigned int);

typedef struct mmc_hash {
	mmc_hash_create_state	create_state;
	mmc_hash_free_state		free_state;
	mmc_hash_find_server	find_server;
	mmc_hash_add_server		add_server;
} mmc_hash_t;

extern mmc_hash_t mmc_standard_hash;
extern mmc_hash_t mmc_consistent_hash;

/* 32 bit magic FNV-1a prime and init */
#define FNV_32_PRIME 0x01000193
#define FNV_32_INIT 0x811c9dc5 

/* failure callback prototype */
typedef void (*mmc_failure_callback)(mmc_pool_t *pool, mmc_t *mmc, void *param TSRMLS_DC);

/* server pool */
struct mmc_pool {
	mmc_t					**servers;
	int						num_servers;
	mmc_hash_t				*hash;						/* hash strategy methods */
	void					*hash_state;				/* strategy specific state */
	mmc_queue_t				*sending;					/* mmc_queue_t<mmc_t *>, connections that want to send */
	mmc_queue_t				*reading;					/* mmc_queue_t<mmc_t *>, connections that want to read */
	mmc_queue_t				_sending1, _sending2;
	mmc_queue_t				_reading1, _reading2;
	mmc_queue_t				pending;					/* mmc_queue_t<mmc_t *>, connections that have non-finalized requests */
	mmc_queue_t				free_requests;				/* mmc_queue_t<mmc_request_t *>, request free-list */
	double					min_compress_savings;
	unsigned int			compress_threshold;
	mmc_failure_callback	failure_callback;			/* receives notification when a server fails */
	void					*failure_callback_param;
};

/* server functions */
mmc_t *mmc_server_new(const char *, int, unsigned short, unsigned short, int, int, int TSRMLS_DC);
void mmc_server_free(mmc_t * TSRMLS_DC);
void mmc_server_disconnect(mmc_t *mmc, mmc_stream_t *io TSRMLS_DC);
int mmc_server_failure(mmc_t *, mmc_stream_t *, const char *, int TSRMLS_DC);
int mmc_request_failure(mmc_t *, mmc_stream_t *, char *, unsigned int, int TSRMLS_DC);

/* request functions */
int mmc_request_parse_line(mmc_t *, mmc_request_t * TSRMLS_DC);
int mmc_request_parse_value(mmc_t *, mmc_request_t * TSRMLS_DC);

/* pool functions */
mmc_pool_t *mmc_pool_new(TSRMLS_D);
void mmc_pool_free(mmc_pool_t * TSRMLS_DC);
void mmc_pool_add(mmc_pool_t *, mmc_t *, unsigned int);
int mmc_pool_open(mmc_pool_t *, mmc_t *, mmc_stream_t *, int TSRMLS_DC);
void mmc_pool_select(mmc_pool_t *, long TSRMLS_DC);
void mmc_pool_run(mmc_pool_t * TSRMLS_DC);

int mmc_pool_failover_handler(mmc_pool_t *, mmc_t *, mmc_request_t *, void * TSRMLS_DC);

mmc_request_t *mmc_pool_request(mmc_pool_t *, int, mmc_request_parser, 
	mmc_request_value_handler, void *, mmc_request_failover_handler, void * TSRMLS_DC);
#define mmc_pool_release(p, r) mmc_queue_push(&((p)->free_requests), (r))

int mmc_prepare_store(
	mmc_pool_t *, mmc_request_t *, const char *, unsigned int,
	const char *, unsigned int, unsigned int, unsigned int, zval * TSRMLS_DC);

int mmc_pool_schedule_key(mmc_pool_t *, const char *, unsigned int, mmc_request_t *, unsigned int TSRMLS_DC);
int mmc_pool_schedule_get(mmc_pool_t *, int, const char *, unsigned int, 
	mmc_request_value_handler, void *, mmc_request_failover_handler, void *, mmc_request_t * TSRMLS_DC);
int mmc_pool_schedule_command(mmc_pool_t *, mmc_t *, char *, unsigned int, 
	mmc_request_parser, mmc_request_value_handler, void * TSRMLS_DC);

/* utility functions */
inline int mmc_prepare_key_ex(const char *, unsigned int, char *, unsigned int *);
inline int mmc_prepare_key(zval *, char *, unsigned int *);

#define mmc_str_left(h, n, hlen, nlen) ((hlen) >= (nlen) ? memcmp((h), (n), (nlen)) == 0 : 0)

/* globals */
ZEND_BEGIN_MODULE_GLOBALS(memcache)
	long default_port;
	long chunk_size;
	long hash_strategy;
	long hash_function;
	long allow_failover;
	long max_failover_attempts;
	long redundancy;
	long session_redundancy;
ZEND_END_MODULE_GLOBALS(memcache)

#ifdef ZTS
#define MEMCACHE_G(v) TSRMG(memcache_globals_id, zend_memcache_globals *, v)
#else
#define MEMCACHE_G(v) (memcache_globals.v)
#endif

#endif	/* MEMCACHE_POOL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
