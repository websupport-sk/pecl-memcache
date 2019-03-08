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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef PHP_WIN32
#include <win32/php_stdint.h>
#else
#include <stdint.h>
#endif
#include <string.h>

#include "php.h"
#include "ext/standard/php_smart_string_public.h"
#include "memcache_queue.h"

/*
 * Mac OS X has no MSG_NOSIGNAL but >= 10.2 comes with SO_NOSIGPIPE which is a setsockopt() option
 * and not a send() parameter as MSG_NOSIGNAL. OpenBSD has none of the options so we need to ignore 
 * SIGPIPE events
 */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif /*MSG_NOSIGNAL*/

/* use lowest byte for flags */
#define MMC_SERIALIZED	0x0001
#define MMC_COMPRESSED	0x0002

/* use second lowest byte to indicate data type */
#define MMC_TYPE_STRING	0x0000
#define MMC_TYPE_BOOL	0x0100
/*#define MMC_TYPE_INT	0x0200*/
#define MMC_TYPE_LONG	0x0300
/*#define MMC_TYPE_DATE	0x0400*/
/*#define MMC_TYPE_BYTE	0x0500*/
/*#define MMC_TYPE_FLOAT	0x0600*/
#define MMC_TYPE_DOUBLE	0x0700
/*#define MMC_TYPE_BLOB	0x0800*/

/* These flags are set here to reserve the for use by users of the pecl-memcache in set and get */
#define MMC_RESERVED_APPLICATIONDEFINEDFLAG_12 0x10000 /* not used in pecl-memcache code */
#define MMC_RESERVED_APPLICATIONDEFINEDFLAG_13 0x20000 /* not used in pecl-memcache code */
#define MMC_RESERVED_APPLICATIONDEFINEDFLAG_14 0x40000 /* not used in pecl-memcache code */
#define MMC_RESERVED_APPLICATIONDEFINEDFLAG_15 0x80000 /* not used in pecl-memcache code */

#define MMC_BUFFER_SIZE			4096
#define MMC_MAX_UDP_LEN			1400
#define MMC_MAX_KEY_LEN			250
#define MMC_VALUE_HEADER		"VALUE %250s %u %lu %lu"	/* keep in sync with MMC_MAX_KEY_LEN */

#define MMC_COMPRESSION_LEVEL 	Z_DEFAULT_COMPRESSION
#define MMC_DEFAULT_COMPRESS 	20000						/* minimum 20k byte values for auto compression to be used */
#define MMC_DEFAULT_SAVINGS 	0.2							/* minimum 20% savings for compression to be used */

#define MMC_PROTO_TCP 0
#define MMC_PROTO_UDP 1

#define MMC_STATUS_FAILED		-1			/* server/stream is down */
#define MMC_STATUS_DISCONNECTED	0			/* stream is disconnected, ie. new connection */
#define MMC_STATUS_UNKNOWN		1			/* stream is in unknown state, ie. non-validated persistent connection */
#define MMC_STATUS_CONNECTED	2			/* stream is connected */

#define MMC_OK 					0

#define MMC_REQUEST_FAILURE 	-1			/* operation failed, failover to other server */
#define MMC_REQUEST_DONE 		0			/* ok result, or reading/writing is done */
#define MMC_REQUEST_MORE 		1			/* more data follows in another packet, try select() again */
#define MMC_REQUEST_AGAIN 		2			/* more data follows in this packet, try read/write again */
#define MMC_REQUEST_RETRY 		3			/* retry/reschedule request */

#define MMC_RESPONSE_UNKNOWN		-2
#define MMC_RESPONSE_ERROR			-1
#define MMC_RESPONSE_NOT_FOUND		0x01			/* same as binary protocol */
#define MMC_RESPONSE_EXISTS			0x02			/* same as binary protocol */
#define MMC_RESPONSE_TOO_LARGE		0x03			/* same as binary protocol */
#define MMC_RESPONSE_NOT_STORED		0x05			/* same as binary protocol */
#define MMC_RESPONSE_CLIENT_ERROR	0x06
#define MMC_RESPONSE_UNKNOWN_CMD	0x81			/* same as binary protocol */
#define MMC_RESPONSE_OUT_OF_MEMORY	0x82			/* same as binary protocol */

#define MMC_STANDARD_HASH 		1
#define MMC_CONSISTENT_HASH 	2
#define MMC_HASH_CRC32 			1			/* CRC32 hash function */
#define MMC_HASH_FNV1A 			2			/* FNV-1a hash function */

#define MMC_CONSISTENT_POINTS	160			/* points per server */
#define MMC_CONSISTENT_BUCKETS	1024		/* number of precomputed buckets, should be power of 2 */

/* io buffer */
typedef struct mmc_buffer {
	smart_string	value;
	unsigned int	idx;					/* current index */
} mmc_buffer_t;

#define mmc_buffer_release(b) ZEND_SECURE_ZERO((b), sizeof(*(b)))
#define mmc_buffer_reset(b) (b)->value.len = (b)->idx = 0

#ifdef PHP_WIN32
#define MMC_POOL_INLINE
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
/* see https://gcc.gnu.org/gcc-5/porting_to.html */
#define MMC_POOL_INLINE extern inline
#else
#define MMC_POOL_INLINE inline
#endif

void mmc_buffer_alloc(mmc_buffer_t *, unsigned int);
void mmc_buffer_free(mmc_buffer_t *);

/* stream handlers */
typedef struct mmc_stream mmc_stream_t;

typedef size_t (*mmc_stream_read)(mmc_stream_t *stream, char *buf, size_t count);
typedef char *(*mmc_stream_readline)(mmc_stream_t *stream, char *buf, size_t maxlen, size_t *retlen);

/* stream container */
struct mmc_stream {
	php_stream				*stream;
	int						fd;							/* file descriptor for select() */
	unsigned short			port;						/* tcp/udp port stream is connected to */
	int						chunk_size;					/* stream chunk size */
	int						status;						/* stream status in MMC_STATUS_* status codes */
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

typedef int (*mmc_request_reader)(mmc_t *mmc, mmc_request_t *request);
typedef int (*mmc_request_parser)(mmc_t *mmc, mmc_request_t *request);
typedef int (*mmc_request_value_handler)(
	const char *key, unsigned int key_len, zval *value,
	unsigned int flags, unsigned long cas, void *param);
typedef int (*mmc_request_response_handler)(
	mmc_t *mmc, mmc_request_t *request, int response, const char *message, unsigned int message_len, void *param);
typedef int (*mmc_request_failover_handler)(
	mmc_pool_t *pool, mmc_t *mmc, mmc_request_t *request, void *param);

void mmc_request_reset(mmc_request_t *);
void mmc_request_free(mmc_request_t *);

/* server request */
struct mmc_request {
	mmc_stream_t					*io;							/* stream this request is sending on */
	mmc_buffer_t					sendbuf;						/* request buffer */
	mmc_buffer_t					readbuf;						/* response buffer */
	char							key[MMC_MAX_KEY_LEN + 1];		/* key buffer to use on failover of single-key requests */
	unsigned int					key_len;
	unsigned int					protocol;						/* protocol encoding of request {tcp, udp} */
	mmc_queue_t						failed_servers;					/* mmc_queue_t<mmc_t *>, servers this request has failed at */
	unsigned int					failed_index;					/* last index probed on failure */
	mmc_request_reader				read;							/* handles reading (and validating datagrams) */
	mmc_request_parser				parse;							/* called to parse response payload */
	mmc_request_value_handler		value_handler;					/* called when result value have been parsed */
	void							*value_handler_param;
	mmc_request_response_handler	response_handler;				/* called when a non-value response has been received */
	void							*response_handler_param;
	mmc_request_failover_handler	failover_handler;				/* called to perform failover of failed request */
	void							*failover_handler_param;
	struct {
		uint16_t		reqid;										/* id of the request, increasing value set by client */
		uint16_t		seqid;										/* next expected seqid */
		uint16_t		total;										/* number of datagrams in response */
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
	struct timeval		timeout;				/* network timeout for this server */
	int					persistent;
	uint16_t			reqid;					/* next sequential request id */
	char				*error;					/* last error message */
	int					errnum;					/* last error code */
};

/* wire protocol */
#define MMC_ASCII_PROTOCOL	1
#define MMC_BINARY_PROTOCOL	2

/* same as in binary protocol */
#define MMC_OP_GET			0x00
#define MMC_OP_SET			0x01
#define MMC_OP_ADD			0x02
#define MMC_OP_REPLACE		0x03
#define MMC_OP_GETS			0x32
#define MMC_OP_CAS			0x33
#define MMC_OP_APPEND		0x34	/* not supported by binary protocol */
#define MMC_OP_PREPEND		0x35	/* not supported by binary protocol */

typedef mmc_request_t * (*mmc_protocol_create_request)();
typedef void (*mmc_protocol_clone_request)(mmc_request_t *clone, mmc_request_t *request);
typedef void (*mmc_protocol_reset_request)(mmc_request_t *request);
typedef void (*mmc_protocol_free_request)(mmc_request_t *request);

typedef void (*mmc_protocol_get)(mmc_request_t *request, int op, zval *zkey, const char *key, unsigned int key_len);
typedef void (*mmc_protocol_begin_get)(mmc_request_t *request, int op);
typedef void (*mmc_protocol_append_get)(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len);
typedef void (*mmc_protocol_end_get)(mmc_request_t *request);

typedef int (*mmc_protocol_store)(
	mmc_pool_t *pool, mmc_request_t *request, int op, const char *key, unsigned int key_len,
	unsigned int flags, unsigned int exptime, unsigned long cas, zval *value);
typedef void (*mmc_protocol_delete)(mmc_request_t *request, const char *key, unsigned int key_len, unsigned int exptime);
typedef void (*mmc_protocol_mutate)(mmc_request_t *request, zval *zkey, const char *key, unsigned int key_len, long value, long defval, int defval_used, unsigned int exptime);

typedef void (*mmc_protocol_flush)(mmc_request_t *request, unsigned int exptime);
typedef void (*mmc_protocol_stats)(mmc_request_t *request, const char *type, long slabid, long limit);
typedef void (*mmc_protocol_version)(mmc_request_t *request);

typedef void (*mmc_protocol_set_sasl_auth_data)(mmc_pool_t *pool, mmc_request_t *request, const char *user, const char *password);

typedef struct mmc_protocol {
	mmc_protocol_create_request	create_request;
	mmc_protocol_clone_request	clone_request;
	mmc_protocol_reset_request	reset_request;
	mmc_protocol_free_request	free_request;

	mmc_protocol_get		get;
	mmc_protocol_begin_get	begin_get;
	mmc_protocol_append_get	append_get;
	mmc_protocol_end_get	end_get;

	mmc_protocol_store		store;
	mmc_protocol_delete		delete;
	mmc_protocol_mutate		mutate;

	mmc_protocol_flush		flush;
	mmc_protocol_version	version;
	mmc_protocol_stats		stats;
	
	mmc_protocol_set_sasl_auth_data set_sasl_auth_data;
} mmc_protocol_t;

extern mmc_protocol_t mmc_ascii_protocol;
extern mmc_protocol_t mmc_binary_protocol;

/* hashing strategy */
typedef unsigned int (*mmc_hash_function_init)();
typedef unsigned int (*mmc_hash_function_combine)(unsigned int seed, const void *key, unsigned int key_len);
typedef unsigned int (*mmc_hash_function_finish)(unsigned int seed);

typedef struct mmc_hash_function {
	mmc_hash_function_init		init;
	mmc_hash_function_combine	combine;
	mmc_hash_function_finish	finish;
} mmc_hash_function_t;

extern mmc_hash_function_t mmc_hash_crc32;
extern mmc_hash_function_t mmc_hash_fnv1a;

#define mmc_hash(hash, key, key_len) ((hash)->finish((hash)->combine((hash)->init(), (key), (key_len))))

typedef void * (*mmc_hash_create_state)(mmc_hash_function_t *);
typedef void (*mmc_hash_free_state)(void *state);
typedef mmc_t * (*mmc_hash_find_server)(void *state, const char *key, unsigned int key_len);
typedef void (*mmc_hash_add_server)(void *state, mmc_t *mmc, unsigned int weight);

typedef struct mmc_hash_strategy {
	mmc_hash_create_state	create_state;
	mmc_hash_free_state		free_state;
	mmc_hash_find_server	find_server;
	mmc_hash_add_server		add_server;
} mmc_hash_strategy_t;

extern mmc_hash_strategy_t mmc_standard_hash;
extern mmc_hash_strategy_t mmc_consistent_hash;

/* 32 bit magic FNV-1a prime and init */
#define FNV_32_PRIME	0x01000193
#define FNV_32_INIT		0x811c9dc5

/* failure callback prototype */
typedef void (*mmc_failure_callback)(mmc_pool_t *pool, mmc_t *mmc, zval *param);

/* server pool */
struct mmc_pool {
	mmc_t					**servers;
	int						num_servers;
	mmc_protocol_t			*protocol;					/* wire protocol */
	mmc_hash_strategy_t		*hash;						/* hash strategy */
	void					*hash_state;				/* strategy specific state */
	fd_set					wfds;
	fd_set					rfds;
	struct timeval			timeout;					/* smallest timeout for any of the servers */
	int						in_select;
	mmc_queue_t				*sending;					/* mmc_queue_t<mmc_t *>, connections that want to send */
	mmc_queue_t				*reading;					/* mmc_queue_t<mmc_t *>, connections that want to read */
	mmc_queue_t				_sending1, _sending2;
	mmc_queue_t				_reading1, _reading2;
	mmc_queue_t				pending;					/* mmc_queue_t<mmc_t *>, connections that have non-finalized requests */
	mmc_queue_t				free_requests;				/* mmc_queue_t<mmc_request_t *>, request free-list */
	double					min_compress_savings;
	unsigned int			compress_threshold;
	mmc_failure_callback	failure_callback;			/* receives notification when a server fails */
	zval					failure_callback_param;
};

/* server functions */
mmc_t *mmc_server_new(const char *, int, unsigned short, unsigned short, int, double, int);
void mmc_server_free(mmc_t *);
void mmc_server_disconnect(mmc_t *mmc, mmc_stream_t *io);
int mmc_server_valid(mmc_t *);
int mmc_server_failure(mmc_t *, mmc_stream_t *, const char *, int);
int mmc_request_failure(mmc_t *, mmc_stream_t *, const char *, unsigned int, int);

/* pool functions */
mmc_pool_t *mmc_pool_new();
void mmc_pool_free(mmc_pool_t *);
void mmc_pool_add(mmc_pool_t *, mmc_t *, unsigned int);
void mmc_pool_close(mmc_pool_t *);
int mmc_pool_open(mmc_pool_t *, mmc_t *, mmc_stream_t *, int);
void mmc_pool_select(mmc_pool_t *);
void mmc_pool_run(mmc_pool_t *);
mmc_t *mmc_pool_find_next(mmc_pool_t *, const char *, unsigned int, mmc_queue_t *, unsigned int *);
mmc_t *mmc_pool_find(mmc_pool_t *, const char *, unsigned int);
int mmc_pool_schedule(mmc_pool_t *, mmc_t *, mmc_request_t *);
int mmc_pool_failover_handler(mmc_pool_t *, mmc_t *, mmc_request_t *, void *);
int mmc_pool_failover_handler_null(mmc_pool_t *, mmc_t *, mmc_request_t *, void *);

mmc_request_t *mmc_pool_request(mmc_pool_t *, int,
	mmc_request_response_handler, void *, mmc_request_failover_handler, void *);
mmc_request_t *mmc_pool_request_get(mmc_pool_t *, int,
	mmc_request_value_handler, void *, mmc_request_failover_handler, void *);

#define mmc_pool_release(p, r) mmc_queue_push(&((p)->free_requests), (r))

int mmc_prepare_store(
	mmc_pool_t *, mmc_request_t *, const char *, unsigned int,
	const char *, unsigned int, unsigned int, unsigned int, zval *);

int mmc_pool_schedule_key(mmc_pool_t *, const char *, unsigned int, mmc_request_t *, unsigned int);
int mmc_pool_schedule_get(mmc_pool_t *, int, int, zval *,
	mmc_request_value_handler, void *, mmc_request_failover_handler, void *, mmc_request_t *);

/* utility functions */
int mmc_pack_value(mmc_pool_t *, mmc_buffer_t *, zval *, unsigned int *);
int mmc_unpack_value(mmc_t *, mmc_request_t *, mmc_buffer_t *, const char *, unsigned int, unsigned int, unsigned long, unsigned int);
double timeval_to_double(struct timeval tv);
struct timeval double_to_timeval(double sec);

int mmc_prepare_key_ex(const char *, unsigned int, char *, unsigned int *, char *);
int mmc_prepare_key(zval *, char *, unsigned int *);

#define mmc_str_left(h, n, hlen, nlen) ((hlen) >= (nlen) ? memcmp((h), (n), (nlen)) == 0 : 0)

/* globals */
ZEND_BEGIN_MODULE_GLOBALS(memcache)
	long default_port;
	long chunk_size;
	long protocol;
	long hash_strategy;
	long hash_function;
	long allow_failover;
	long max_failover_attempts;
	long redundancy;
	long session_redundancy;
	long compress_threshold;
	long lock_timeout;
	char *session_key_prefix;
	char *session_prefix_host_key;
	char *session_prefix_host_key_remove_www;
	char *session_prefix_host_key_remove_subdomain;
	char *session_prefix_static_key;
	char *session_save_path;
	char *key_prefix;
	char *prefix_host_key;
	char *prefix_host_key_remove_www;
	char *prefix_host_key_remove_subdomain;
	char *prefix_static_key;
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
