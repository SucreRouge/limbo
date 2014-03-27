// vim:filetype=c:textwidth=80:shiftwidth=4:softtabstop=4:expandtab
/*
 * A negative value of vector_t's `capacity' attribute indicates that the
 * object does not own the memory of its `array' attribute.
 */
#include "vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INIT_SIZE 4
#define RESIZE_FACTOR 2
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

vector_t vector_init(void)
{
    return vector_init_with_size(INIT_SIZE);
}

vector_t vector_init_with_size(int size)
{
    assert(size >= 0);
    size = MAX(size, INIT_SIZE);
    return (vector_t) {
        .array = malloc(size * sizeof(void *)),
        .capacity = size,
        .size = 0,
    };
}

vector_t vector_copy(const vector_t *src)
{
    return vector_copy_range(src, 0, src->size);
}

vector_t vector_copy_range(const vector_t *src, int from, int to)
{
    const int n_new_elems = to - from;
    vector_t dst;
    dst.capacity = MAX(n_new_elems, INIT_SIZE);
    dst.array = malloc(src->capacity * sizeof(void *));
    memcpy(dst.array,
            src->array + from,
            n_new_elems * sizeof(void *));
    dst.size = n_new_elems;
    return dst;
}

const vector_t vector_sub(const vector_t *src, int from, int to)
{
    const int n_new_elems = to - from;
    vector_t dst = {
        .array = src->array + sizeof(void *) * from,
        .capacity = -1,
        .size = n_new_elems
    };
    return dst;
}

void vector_free(vector_t *vec)
{
    if (vec->capacity >= 0 && vec->array != NULL) {
        free(vec->array);
    }
    vec->array = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

const void *vector_get(const vector_t *vec, int index)
{
    assert(0 <= index && index < vec->size);
    return vec->array[index];
}

int vector_cmp(const vector_t *vec1, const vector_t *vec2,
        int (*compar)(const void *, const void *))
{
    if (vec1 == vec2) {
        return 0;
    }
    const int n = MIN(vec1->size, vec2->size);
    if (compar != NULL) {
        for (int i = 0; i < n; ++i) {
            const int cmp = compar(vec1->array[i], vec2->array[i]);
            if (cmp != 0) {
                return cmp;
            }
        }
    } else {
        const int cmp = memcmp(vec1->array, vec2->array, n);
        if (cmp != 0) {
            return cmp;
        }
    }
    return vec1->size - vec2->size;
}

bool vector_eq(const vector_t *vec1, const vector_t *vec2,
        int (*compar)(const void *, const void *))
{
    return vec1->size == vec2->size && vector_cmp(vec1, vec2, compar) == 0;
}

const void **vector_array(const vector_t *vec)
{
    return vec->array;
}

int vector_size(const vector_t *vec)
{
    return vec->size;
}

void vector_prepend(vector_t *vec, const void *elem)
{
    vector_insert(vec, 0, elem);
}

void vector_append(vector_t *vec, const void *elem)
{
    vector_insert(vec, vec->size, elem);
}

void vector_insert(vector_t *vec, int index, const void *elem)
{
    assert(0 <= index && index <= vec->size);
    if (vec->size + 1 >= vec->capacity) {
        vec->capacity *= RESIZE_FACTOR;
        vec->array = realloc(vec->array, vec->capacity * sizeof(void *));
    }
    memmove(vec->array + index + 1,
            vec->array + index,
            (vec->size - index) * sizeof(void *));
    vec->array[index] = elem;
    ++vec->size;
}

void vector_prepend_all(vector_t *vec, const vector_t *elems)
{
    vector_insert_all(vec, 0, elems);
}

void vector_append_all(vector_t *vec, const vector_t *elems)
{
    vector_insert_all(vec, vec->size, elems);
}

void vector_insert_all(vector_t *vec, int index, const vector_t *elems)
{
    vector_insert_all_range(vec, index, elems, 0, elems->size);
}

void vector_prepend_all_range(vector_t *vec, const vector_t *elems,
        int from, int to)
{
    vector_insert_all_range(vec, 0, elems, from, to);
}

void vector_append_all_range(vector_t *vec, const vector_t *elems,
        int from, int to)
{
    vector_insert_all_range(vec, vec->size, elems, from, to);
}

void vector_insert_all_range(vector_t *vec, int index,
        const vector_t *elems, int from, int to)
{
    assert(0 <= index && index <= vec->size);
    const int n_new_elems = to - from;
    while (vec->size + n_new_elems >= vec->capacity) {
        vec->capacity *= RESIZE_FACTOR;
    }
    vec->array = realloc(vec->array, vec->capacity * sizeof(void *));
    memmove(vec->array + index + n_new_elems,
            vec->array + index,
            (vec->size - index) * sizeof(void *));
    memcpy(vec->array + index,
            elems->array + from,
            n_new_elems * sizeof(void *));
    vec->size += n_new_elems;
}

const void *vector_remove(vector_t *vec, int index)
{
    const void *elem;
    assert(0 <= index && index < vec->size);
    elem = vec->array[index];
    memmove(vec->array + index,
            vec->array + index + 1,
            (vec->size - index - 1) * sizeof(void *));
    --vec->size;
    return elem;
}

void vector_clear(vector_t *vec)
{
    vec->size = 0;
}
