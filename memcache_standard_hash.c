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

#include "php.h"
#include "ext/standard/crc32.h"
#include "php_memcache.h"

ZEND_EXTERN_MODULE_GLOBALS(memcache)

typedef struct mmc_standard_state {
	int						num_servers;
	mmc_t					**buckets;
	int						num_buckets;
} mmc_standard_state_t;

void *mmc_standard_create_state() /* {{{ */
{
	mmc_standard_state_t *state = emalloc(sizeof(mmc_standard_state_t));
	memset(state, 0, sizeof(mmc_standard_state_t));
	return state;
}
/* }}} */

void mmc_standard_free_state(void *s) /* {{{ */
{
	mmc_standard_state_t *state = s;
	if (state != NULL) {
		if (state->buckets != NULL) {
			efree(state->buckets);
		}
		efree(state);
	}
}
/* }}} */

static unsigned int mmc_hash(const char *key, int key_len) /* {{{ */
{
	unsigned int crc = ~0;
	int i;

	for (i=0; i<key_len; i++) {
		CRC32(crc, key[i]);
	}

	crc = (~crc >> 16) & 0x7fff;
  	return crc ? crc : 1;
}
/* }}} */

mmc_t *mmc_standard_find_server(void *s, const char *key, int key_len TSRMLS_DC) /* {{{ */
{
	mmc_standard_state_t *state = s;

	if (state->num_servers > 1) {
		return state->buckets[mmc_hash(key, key_len) % state->num_buckets];
	}

	return state->buckets[0];
}
/* }}} */

void mmc_standard_add_server(void *s, mmc_t *mmc, unsigned int weight) /* {{{ */
{
	mmc_standard_state_t *state = s;
	int i;

	/* add weight number of buckets for this server */
	if (state->num_buckets) {
		state->buckets = erealloc(state->buckets, sizeof(mmc_t *) * (state->num_buckets + weight));
	}
	else {
		state->buckets = emalloc(sizeof(mmc_t *) * (weight));
	}

	for (i=0; i<weight; i++) {
		state->buckets[state->num_buckets + i] = mmc;
	}

	state->num_buckets += weight;
	state->num_servers++;
}
/* }}} */

mmc_hash_t mmc_standard_hash = {
	mmc_standard_create_state,
	mmc_standard_free_state,
	mmc_standard_find_server,
	mmc_standard_add_server
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
