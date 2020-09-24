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
#include "php_memcache.h"

ZEND_EXTERN_MODULE_GLOBALS(memcache)

typedef struct mmc_standard_state {
	int						num_servers;
	mmc_t					**buckets;
	int						num_buckets;
	mmc_hash_function_t		*hash;
} mmc_standard_state_t;

void *mmc_standard_create_state(mmc_hash_function_t *hash) /* {{{ */
{
	mmc_standard_state_t *state = emalloc(sizeof(mmc_standard_state_t));
	ZEND_SECURE_ZERO(state, sizeof(mmc_standard_state_t));
	state->hash = hash;
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

mmc_t *mmc_standard_find_server(void *s, const char *key, unsigned int key_len) /* {{{ */
{
	mmc_standard_state_t *state = s;

	if (state->num_servers > 1) {
		/* "new-style" hash */
		unsigned int hash = (mmc_hash(state->hash, key, key_len) >> 16) & 0x7fff; 
		return state->buckets[(hash ? hash : 1) % state->num_buckets];
	}

	return state->buckets[0];
}
/* }}} */

void mmc_standard_add_server(void *s, mmc_t *mmc, unsigned int weight) /* {{{ */
{
	mmc_standard_state_t *state = s;
	int i;

	/* add weight number of buckets for this server */
	state->buckets = erealloc(state->buckets, sizeof(*state->buckets) * (state->num_buckets + weight));

	for (i=0; i<weight; i++) {
		state->buckets[state->num_buckets + i] = mmc;
	}

	state->num_buckets += weight;
	state->num_servers++;
}
/* }}} */

mmc_hash_strategy_t mmc_standard_hash = {
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
