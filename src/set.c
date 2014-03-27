// vim:filetype=c:textwidth=80:shiftwidth=4:softtabstop=4:expandtab
#include "set.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MIN(x,y) ((x) < (y) ? (x) : (y))

set_t set_init(compar_t compar)
{
    return (set_t) {
        .vec = vector_init(),
        .compar = compar
    };
}

set_t set_init_with_size(compar_t compar, int size)
{
    return (set_t) {
        .vec = vector_init_with_size(size),
        .compar = compar
    };
}

set_t set_copy(const set_t *src)
{
    return (set_t) {
        .vec = vector_copy(&src->vec),
        .compar = src->compar
    };
}

set_t set_singleton(compar_t compar, const void *elem)
{
    set_t set = set_init_with_size(compar, 1);
    set_add(&set, elem);
    return set;
}

set_t set_union(const set_t *l, const set_t *r)
{
    assert(l->compar == r->compar);
    set_t set = set_init_with_size(l->compar, set_size(l) + set_size(r));
    int i = 0;
    int j = 0;
    while (i < vector_size(&l->vec) && j < vector_size(&r->vec)) {
        const int cmp = l->compar(
                vector_get(&l->vec, i),
                vector_get(&r->vec, j));
        if (cmp < 0) {
            vector_append(&set.vec, vector_get(&l->vec, i));
            ++i;
        } else if (cmp > 0) {
            vector_append(&set.vec, vector_get(&r->vec, j));
            ++j;
        } else {
            vector_append(&set.vec, vector_get(&l->vec, i));
            ++i;
            ++j;
        }
    }
    vector_append_all_range(&set.vec, &l->vec, i, vector_size(&l->vec));
    vector_append_all_range(&set.vec, &r->vec, j, vector_size(&r->vec));
    return set;
}

set_t set_difference(const set_t *l, const set_t *r)
{
    assert(l->compar == r->compar);
    set_t set = set_init_with_size(l->compar, set_size(l));
    int i = 0;
    int j = 0;
    while (i < vector_size(&l->vec)) {
        while (j < vector_size(&r->vec)) {
            const int cmp = l->compar(
                    vector_get(&l->vec, i),
                    vector_get(&r->vec, j));
            if (cmp < 0) {
                break;
            } else if (cmp > 0) {
                ++j;
            } else {
                goto skip;
            }
        }
        vector_append(&set.vec, vector_get(&l->vec, i));
skip:
        ++i;
    }
    return set;
}

set_t set_intersection(const set_t *l, const set_t *r)
{
    assert(l->compar == r->compar);
    set_t set = set_init_with_size(l->compar, MIN(set_size(l), set_size(r)));
    int i = 0;
    int j = 0;
    while (i < vector_size(&l->vec) && j < vector_size(&r->vec)) {
        const int cmp = l->compar(
                vector_get(&l->vec, i),
                vector_get(&r->vec, j));
        if (cmp < 0) {
            ++i;
        } else if (cmp > 0) {
            ++j;
        } else {
            vector_append(&set.vec, vector_get(&l->vec, i));
            ++i;
            ++j;
        }
    }
    return set;
}

void set_free(set_t *set)
{
    vector_free(&set->vec);
}

int set_cmp(const set_t *set1, const set_t *set2)
{
    assert(set1->compar == set2->compar);
    return vector_cmp(&set1->vec, &set2->vec, set1->compar);
}

bool set_eq(const set_t *set1, const set_t *set2)
{
    assert(set1->compar == set2->compar);
    return vector_eq(&set1->vec, &set2->vec, set1->compar);
}

static inline int search(const set_t *set, const void *obj)
{
    // XXX could use Knuth's uniform binary search to safe some comparisons
    int lo = 0;
    int hi = vector_size(&set->vec) - 1;
    while (lo <= hi) {
        const int i = (lo + hi) / 2;
        const int cmp = set->compar(obj, vector_get(&set->vec, i));
        if (cmp == 0) { // found
            return i;
        } else if (cmp < 0) { // left half
            hi = i - 1;
        } else { // right half
            lo = i + 1;
        }
    }
    return -1;
}

static inline int insert_pos(const set_t *set, const void *obj)
{
    // XXX could use Knuth's uniform binary search to safe some comparisons
    int lo = 0;
    int hi = vector_size(&set->vec) - 1;
    while (lo <= hi) {
        const int i = (lo + hi) / 2;
        const int cmp = set->compar(obj, vector_get(&set->vec, i));
        if (cmp == 0) { // element already present
            return -1;
        } else if (cmp <= 0) { // left half
            if (i == 0 ||
                    !(set->compar(obj,
                            vector_get(&set->vec,
                                i-1)) <= 0)) { // position found
                return i;
            } else { // left half
                hi = i - 1;
            }
        } else { // right half
            lo = i + 1;
        }
    }
    return vector_size(&set->vec);
}

const void *set_get(const set_t *set, int index)
{
    return vector_get(&set->vec, index);
}

const void **set_array(const set_t *set)
{
    return vector_array(&set->vec);
}

int set_size(const set_t *set)
{
    return vector_size(&set->vec);
}

int set_find(const set_t *set, const void *elem)
{
    return search(set, elem);
}

bool set_contains(const set_t *set, const void *elem)
{
    return set_find(set, elem) != -1;
}

bool set_add(set_t *set, const void *elem)
{
    const int i = insert_pos(set, elem);
    if (i != -1) {
        vector_insert(&set->vec, i, elem);
        return true;
    } else {
        return false;
    }
}

bool set_remove(set_t *set, const void *elem)
{
    const int i = search(set, elem);
    if (i != -1) {
        vector_remove(&set->vec, i);
        return true;
    } else {
        return false;
    }
}

const void *set_remove_index(set_t *set, int index)
{
    const void *elem = vector_get(&set->vec, index);
    vector_remove(&set->vec, index);
    return elem;
}

void set_clear(set_t *set)
{
    vector_clear(&set->vec);
}
