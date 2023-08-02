/*
 * LRU object list
 *
 * Copyright (c) 2021 Matt Borgerson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef LRU_H
#define LRU_H

#include <assert.h>
#include <stdint.h>
#include "qemu/queue.h"

#define LRU_NUM_BINS (1<<16)

typedef struct LruNode {
	QTAILQ_ENTRY(LruNode) next_global;
	QTAILQ_ENTRY(LruNode) next_bin;
	uint64_t hash;
} LruNode;

typedef struct Lru Lru;

struct Lru {
	QTAILQ_HEAD(, LruNode) global;
	QTAILQ_HEAD(, LruNode) bins[LRU_NUM_BINS];

	/* Initialize a node. */
	void (*init_node)(Lru *lru, LruNode *node, void *key);

	/* In case of hash collision. Return `true` if nodes differ. */
	bool (*compare_nodes)(Lru *lru, LruNode *node, void *key);

	/* Optional. Called before eviction. Return `false` to prevent eviction. */
	bool (*pre_node_evict)(Lru *lru, LruNode *node);

	/* Optional. Called after eviction. Reclaim any associated resources. */
	void (*post_node_evict)(Lru *lru, LruNode *node);
};

static inline
void lru_init(Lru *lru)
{
	QTAILQ_INIT(&lru->global);
	for (unsigned int i = 0; i < LRU_NUM_BINS; i++) {
		QTAILQ_INIT(&lru->bins[i]);
	}
	lru->init_node = NULL;
	lru->compare_nodes = NULL;
	lru->pre_node_evict = NULL;
	lru->post_node_evict = NULL;
}

static inline
void lru_add_free(Lru *lru, LruNode *node)
{
	node->next_bin.tqe_circ.tql_prev = NULL;
	QTAILQ_INSERT_TAIL(&lru->global, node, next_global);
}

static inline
unsigned int lru_hash_to_bin(Lru *lru, uint64_t hash)
{
	return hash % LRU_NUM_BINS;
}

static inline
unsigned int lru_get_node_bin(Lru *lru, LruNode *node)
{
	return lru_hash_to_bin(lru, node->hash);
}

static inline
bool lru_is_node_in_use(Lru *lru, LruNode *node)
{
	return QTAILQ_IN_USE(node, next_bin);
}

static inline
void lru_evict_node(Lru *lru, LruNode *node)
{
	if (!lru_is_node_in_use(lru, node)) {
		return;
	}

	unsigned int bin = lru_get_node_bin(lru, node);
	QTAILQ_REMOVE(&lru->bins[bin], node, next_bin);
	if (lru->post_node_evict) {
		lru->post_node_evict(lru, node);
	}
}

static inline
LruNode *lru_evict_one(Lru *lru)
{
	LruNode *found;

	QTAILQ_FOREACH_REVERSE(found, &lru->global, next_global) {
		bool can_evict = true;
		if (lru_is_node_in_use(lru, found) && lru->pre_node_evict) {
			can_evict = lru->pre_node_evict(lru, found);
		}
		if (can_evict) {
			break;
		}
	}

	assert(found != NULL); /* No evictable node! */

	lru_evict_node(lru, found);
	return found;
}

static inline
bool lru_contains_hash(Lru *lru, uint64_t hash)
{
	unsigned int bin = lru_hash_to_bin(lru, hash);
	LruNode *iter;

	QTAILQ_FOREACH(iter, &lru->bins[bin], next_bin) {
        if (iter->hash == hash) {
            return true;
        }
    }

	return false;
}

static inline
LruNode *lru_lookup(Lru *lru, uint64_t hash, void *key)
{
	unsigned int bin = lru_hash_to_bin(lru, hash);
	LruNode *iter, *found = NULL;

	QTAILQ_FOREACH(iter, &lru->bins[bin], next_bin) {
        if ((iter->hash == hash) && !lru->compare_nodes(lru, iter, key)) {
            found = iter;
            break;
        }
    }

	if (found) {
		QTAILQ_REMOVE(&lru->bins[bin], found, next_bin);
	} else {
		found = lru_evict_one(lru);
		found->hash = hash;
		if (lru->init_node) {
			lru->init_node(lru, found, key);
		}
		assert(found->hash == hash);
	}

	QTAILQ_REMOVE(&lru->global, found, next_global);
	QTAILQ_INSERT_HEAD(&lru->global, found, next_global);
	QTAILQ_INSERT_HEAD(&lru->bins[bin], found, next_bin);

	return found;
}

static inline
void lru_flush(Lru *lru)
{
	LruNode *iter, *iter_next;

	for (unsigned int bin = 0; bin < LRU_NUM_BINS; bin++) {
		QTAILQ_FOREACH_SAFE(iter, &lru->bins[bin], next_bin, iter_next) {
			bool can_evict = true;
			if (lru->pre_node_evict) {
				can_evict = lru->pre_node_evict(lru, iter);
			}
			if (can_evict) {
				lru_evict_node(lru, iter);
				QTAILQ_REMOVE(&lru->global, iter, next_global);
				QTAILQ_INSERT_TAIL(&lru->global, iter, next_global);
			}
		}
	}
}

typedef void (*LruNodeVisitorFunc)(Lru *lru, LruNode *node, void *opaque);

static inline
void lru_visit_active(Lru *lru, LruNodeVisitorFunc visitor_func, void *opaque)
{
	LruNode *iter, *iter_next;

	for (unsigned int bin = 0; bin < LRU_NUM_BINS; bin++) {
		QTAILQ_FOREACH_SAFE(iter, &lru->bins[bin], next_bin, iter_next) {
			visitor_func(lru, iter, opaque);
		}
	}
}

#endif
