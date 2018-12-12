/*
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
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "lru.h"

#define LRU_DEBUG 0

#if LRU_DEBUG
#define lru_dprintf(...) do { printf(__VA_ARGS__); } while(0)
#else
#define lru_dprintf(...) do {} while(0)
#endif

/*
 * Create the LRU cache
 */
struct lru *lru_init(
    struct lru               *lru,
    lru_obj_init_func         obj_init,
    lru_obj_deinit_func       obj_deinit,
    lru_obj_key_compare_func  obj_key_compare
    )
{
    assert(lru != NULL);

    lru->active = NULL;
    lru->free = NULL;

    lru->obj_init = obj_init;
    lru->obj_deinit = obj_deinit;
    lru->obj_key_compare = obj_key_compare;

    lru->num_free = 0;
    lru->num_collisions = 0;
    lru->num_hit = 0;
    lru->num_miss = 0;

    return lru;
}

/*
 * Add a node to the free list
 */
struct lru_node *lru_add_free(struct lru *lru, struct lru_node *node)
{
    node->next = lru->free;
    lru->free = node;
    lru->num_free++;
    return node;
}

/*
 * Lookup object in cache:
 * - If found, object is promoted to front of RU list and returned
 * - If not found,
 *   - If cache is full, evict LRU, deinit object and add it to free list
 *   - Allocate object from free list, init, move to front of RU list
 */
struct lru_node *lru_lookup(struct lru *lru, uint64_t hash, void *key)
{
    struct lru_node *prev, *node;

    assert(lru != NULL);
    assert((lru->active != NULL) || (lru->free != NULL));

    /* Walk through the cache in order of recent use */
    prev = NULL;
    node = lru->active;

    lru_dprintf("Looking for hash %016lx...\n", hash);

    if (node != NULL) {
        do {
            lru_dprintf("  %016lx\n", node->hash);

            /* Fast hash compare */
            if (node->hash == hash) {
                /* Detailed key comparison */
                if (lru->obj_key_compare(node, key) == 0) {
                    lru_dprintf("Hit, node=%p!\n", node);
                    lru->num_hit++;

                    if (prev == NULL) {
                        /* Node is already at the front of the RU list */
                        return node;
                    }

                    /* Unlink and promote node */
                    lru_dprintf("Promoting node %p\n", node);
                    prev->next = node->next;
                    node->next = lru->active;
                    lru->active = node;
                    return node;
                }

                /* Hash collision! Get a better hashing function... */
                lru_dprintf("Hash collision detected!\n");
                lru->num_collisions++;
            }

            if (node->next == NULL) {
                /* No more nodes left to look at after this... Stop here as we
                 * may need to evict this final (last recently used) node.
                 */
                break;
            }

            prev = node;
            node = node->next;
        } while (1);
    }

    lru_dprintf("Miss\n");
    lru->num_miss++;

    /* Reached the end of the active list.
     *
     * `node` points to:
     * - NULL if there are no active objects in the cache, or
     * - the last object in the RU list
     *
     * `prev` points to:
     * - NULL if there are <= 1 active objects in the cache, or
     * - the second to last object in the RU list
     */

    if (lru->free == NULL) {
        /* No free nodes left, must evict a node. `node` is LRU. */
        assert(node != NULL); /* Sanity check: there must be an active object */
        lru_dprintf("Evicting %p\n", node);

        if (prev == NULL) {
            /* This was the only node */
            lru->active = NULL;
        } else {
            /* Unlink node */
            prev->next = node->next;
        }

        lru->obj_deinit(node);
        lru_add_free(lru, node);
    }

    /* Allocate a node from the free list */
    node = lru->free;
    assert(node != NULL); /* Sanity check: there must be a free node */
    lru->free = node->next;
    lru->num_free--;

    /* Initialize, promote, and return the node */
    lru->obj_init(node, key);
    node->hash = hash;
    node->next = lru->active;
    lru->active = node;
    return node;
}

/*
 * Remove all items in the active list
 */
void lru_flush(struct lru *lru)
{
    struct lru_node *node, *next;

    node = lru->active;
    next = NULL;

    while (node != NULL) {
        next = node->next;
        lru->obj_deinit(node);
        lru_add_free(lru, node);
        node = next;
    }

    lru->active = NULL;
}
