/*
 * Simple LRU Object List
 * ======================
 * - Designed for pre-allocated array of objects which are accessed frequently
 * - Objects are identified by a hash and an opaque `key` data structure
 * - Lookups are first done by hash, then confirmed by callback compare function
 * - Two singly linked lists are maintained: a free list and an active list
 * - On cache miss, object is created from free list or by evicting the LRU
 * - When created, a callback function is called to fully initialize the object
 *
 * Setup
 * -----
 * - Create an object data structure, embed in it `struct lru_node`
 * - Create an init, deinit, and compare function
 * - Call `lru_init`
 * - Allocate a number of these objects
 * - For each object, call `lru_add_free` to populate entries in the cache
 *
 * Runtime
 * -------
 * - Initialize custom key data structure (will be used for comparison)
 * - Create 64b hash of the object and/or key
 * - Call `lru_lookup` with the hash and key
 *   - The active list is searched, the compare callback will be called if an
 *     object with matching hash is found
 *   - If object is found in the cache, it will be moved to the front of the
 *     active list and returned
 *   - If object is not found in the cache:
 *     - If no free items are available, the LRU will be evicted, deinit
 *       callback will be called
 *     - An object is popped from the free list and the init callback is called
 *       on the object
 *     - The object is added to the front of the active list and returned
 *
 * ---
 *
 * Copyright (c) 2018 Matt Borgerson
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

#include <stdint.h>
#include <string.h>

struct lru_node;

typedef struct lru_node *(*lru_obj_init_func)(struct lru_node *obj, void *key);
typedef struct lru_node *(*lru_obj_deinit_func)(struct lru_node *obj);
typedef int              (*lru_obj_key_compare_func)(struct lru_node *obj, void *key);

struct lru {
	struct lru_node *active; /* Singly-linked list tracking recently active */
	struct lru_node *free;   /* Singly-linked list tracking available objects */

	lru_obj_init_func         obj_init;
	lru_obj_deinit_func       obj_deinit;
	lru_obj_key_compare_func  obj_key_compare;

	size_t num_free;
	size_t num_collisions;
	size_t num_hit;
	size_t num_miss;
};

/* This should be embedded in the object structure */
struct lru_node {
	uint64_t hash;
	struct lru_node *next;
};

struct lru *lru_init(
	struct lru *lru,
	lru_obj_init_func obj_init,
	lru_obj_deinit_func obj_deinit,
	lru_obj_key_compare_func obj_key_compare
	);

struct lru_node *lru_add_free(struct lru *lru, struct lru_node *node);
struct lru_node *lru_lookup(struct lru *lru, uint64_t hash, void *key);
void lru_flush(struct lru *lru);

#endif
