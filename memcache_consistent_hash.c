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

#include <stdlib.h>

#include "php.h"
#include "ext/standard/crc32.h"
#include "php_memcache.h"

ZEND_EXTERN_MODULE_GLOBALS(memcache)

typedef struct mmc_consistent_point {
	mmc_t					*server;
	unsigned int			point;
} mmc_consistent_point_t;

typedef struct mmc_consistent_state {
	int						num_servers;
	mmc_consistent_point_t	*points;
	int						num_points;
	mmc_t					*buckets[MMC_CONSISTENT_BUCKETS];
	int						buckets_populated;
} mmc_consistent_state_t;

void *mmc_consistent_create_state() /* {{{ */
{
	mmc_consistent_state_t *state = emalloc(sizeof(mmc_consistent_state_t));
	memset(state, 0, sizeof(mmc_consistent_state_t));
	return state;
}
/* }}} */

void mmc_consistent_free_state(void *s) /* {{{ */
{
	mmc_consistent_state_t *state = s;
	if (state != NULL) {
		if (state->points != NULL) {
			efree(state->points);
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

  	return ~crc;
}
/* }}} */

static int mmc_consistent_compare(const void *a, const void *b) /* {{{ */
{
	if (((mmc_consistent_point_t *)a)->point < ((mmc_consistent_point_t *)b)->point) {
		return -1;
	}
	if (((mmc_consistent_point_t *)a)->point > ((mmc_consistent_point_t *)b)->point) {
		return 1;
	}
	return 0;
}
/* }}} */

static mmc_t *mmc_consistent_find(mmc_consistent_state_t *state, unsigned int point) /* {{{ */
{
	int lo = 0, hi = state->num_points - 1, mid;

	while (1) {
		/* point is outside interval or lo >= hi, wrap-around */
		if (point <= state->points[lo].point || point > state->points[hi].point) {
			return state->points[lo].server;
		}

		/* best guess with random distribution, distance between lowpoint and point scaled down to lo-hi interval */
		mid = lo + (hi - lo) * (point - state->points[lo].point) / (state->points[hi].point - state->points[lo].point);
		MMC_DEBUG(("mmc_consistent_find: lo %d, hi %d, mid %d, point %u, midpoint %u", lo, hi, mid, point, state->points[mid].point));

		/* perfect match */
		if (point <= state->points[mid].point && point > (mid ? state->points[mid-1].point : 0)) {
			return state->points[mid].server;
		}

		/* too low, go up */
		if (state->points[mid].point < point) {
			lo = mid + 1;
		}
		else {
			hi = mid - 1;
		}
	}
}
/* }}} */

static void mmc_consistent_pupulate_buckets(mmc_consistent_state_t *state) /* {{{ */
{
	unsigned int i, step = 0xffffffff / MMC_CONSISTENT_BUCKETS;

	qsort((void *)state->points, state->num_points, sizeof(mmc_consistent_point_t), mmc_consistent_compare);
	for (i=0; i<MMC_CONSISTENT_BUCKETS; i++) {
		state->buckets[i] = mmc_consistent_find(state, step * i);
	}

	state->buckets_populated = 1;
}
/* }}} */

mmc_t *mmc_consistent_find_server(void *s, const char *key, int key_len TSRMLS_DC) /* {{{ */
{
	mmc_consistent_state_t *state = s;
	mmc_t *mmc;

	if (state->num_servers > 1) {
		unsigned int i, hash = mmc_hash(key, key_len);

		if (!state->buckets_populated) {
			mmc_consistent_pupulate_buckets(state);
		}

		mmc = state->buckets[hash % MMC_CONSISTENT_BUCKETS];

		/* perform failover if needed */
		for (i=0; !mmc_open(mmc, 0, NULL, NULL TSRMLS_CC) && MEMCACHE_G(allow_failover) && i<MEMCACHE_G(max_failover_attempts); i++) {
			char *next_key = emalloc(key_len + MAX_LENGTH_OF_LONG + 1);
			int next_len = sprintf(next_key, "%d%s", i+1, key);
			MMC_DEBUG(("mmc_consistent_find_server: failed to connect to server '%s:%d' status %d, trying next", mmc->host, mmc->port, mmc->status));

			hash += mmc_hash(next_key, next_len);
			mmc = state->buckets[hash % MMC_CONSISTENT_BUCKETS];

			efree(next_key);
		}
	}
	else {
		mmc = state->points[0].server;
		mmc_open(mmc, 0, NULL, NULL TSRMLS_CC);
	}

	return mmc->status != MMC_STATUS_FAILED ? mmc : NULL;
}
/* }}} */

void mmc_consistent_add_server(void *s, mmc_t *mmc, unsigned int weight) /* {{{ */
{
	mmc_consistent_state_t *state = s;
	int i, key_len, points = weight * MMC_CONSISTENT_POINTS;
	char *key;

	/* add weight * MMC_CONSISTENT_POINTS number of points for this server */
	if (state->num_points) {
		state->points = erealloc(state->points, sizeof(mmc_consistent_point_t) * (state->num_points + points));
	}
	else {
		state->points = emalloc(sizeof(mmc_consistent_point_t) * (points));
	}

	for (i=0; i<points; i++) {
		key_len = spprintf(&key, 0, "%s:%d-%d", mmc->host, mmc->port, i);
		state->points[state->num_points + i].server = mmc;
		state->points[state->num_points + i].point = mmc_hash(key, key_len);
		MMC_DEBUG(("mmc_consistent_add_server: key %s, point %lu", key, state->points[state->num_points + i].point));
		efree(key);
	}

	state->num_points += points;
	state->num_servers++;

	state->buckets_populated = 0;
}
/* }}} */

mmc_hash_t mmc_consistent_hash = {
	mmc_consistent_create_state,
	mmc_consistent_free_state,
	mmc_consistent_find_server,
	mmc_consistent_add_server
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
