// TODO -Wpointer-arith
// TODO check *allocs for errors.
// TODO Double check pointer arithmatic.
// TODO formatting.
#include <limits.h>    // SSIZE_MAX
#include <stddef.h>    // size_t, ptrdiff_t
#include <stdlib.h>    // *alloc()
#include <string.h>    // memcpy, memset
#include <sys/types.h> // ssize_t
#include "alist.h"


#define MIN_CAP 2


struct alist {
    size_t cap;
    size_t len;
    size_t elem_size;
    char *buf;
};


static
void
alist_init(struct alist *a, size_t elem_size)
{
    a->cap = MIN_CAP;
    a->len = 0;
    a->elem_size = elem_size;
    a->buf = calloc(MIN_CAP, elem_size);
    // TODO if calloc errors?
}

struct alist *
alist_new(size_t elem_size)
{
    // TODO if malloc errors?
    struct alist *p = malloc(sizeof (struct alist));
    alist_init(p, elem_size);
    return p;
}

struct alist *
alist_copy(struct alist *other) {
    struct alist *a = malloc(sizeof (struct alist));
    a->cap = other->cap;
    a->len = other->len;
    a->elem_size = other->elem_size;
    // TODO if calloc errors?
    a->buf = calloc(a->cap, a->elem_size);
    memcpy(a->buf, other->buf, a->len * a->elem_size);
    return a;
}

static void
alist_grow(struct alist *a)
{
    a->cap = a->cap * 3 >> 1; // (3/2) * a->cap
    // FIXME if realloc fails, we loose access to a->buf pointer
    a->buf = realloc(a->buf, a->elem_size * a->cap);
    return;
}

size_t
alist_len(struct alist *a)
{
    return a->len;
}

void *
alist_get(struct alist *a, size_t i)
{
    return a->buf + i * a->elem_size;
}


void *
alist_last(struct alist *a)
{
    if (a->len == 0)
        return NULL;
    else
        return a->buf + a->elem_size * (a->len - 1);
}


void
alist_push(struct alist *a, void *elem)
{
    if (a->len+1 >= a->cap)
        alist_grow(a);
    memcpy(a->buf + a->len * a->elem_size, elem, a->elem_size);
    a->len++;
}


void
alist_replace(struct alist *a, size_t i, void *new_elem)
{
    memcpy(a->buf + i * a->elem_size, new_elem, a->elem_size);
    return;
}


void
alist_del_id(struct alist *a, size_t i)
{
    if (i >= a->len)
        return;
    if (i == a->len - 1)
        memset(a->buf + i * a->elem_size, 0, a->elem_size);
    else {
        memmove(a->buf + i       * a->elem_size,
                a->buf + (i+1)   * a->elem_size,
                (a->len - (i+1)) * a->elem_size);
    }
    a->len--;
    return;
}


void
alist_del_val(struct alist *a, void *elem)
{
    ssize_t id = alist_in(a, elem);
    if (id >= 0)
	alist_del_id(a, (size_t)id);
    return;
}


// Completely free the array.
//
// If function-pointer `destruct` is not null, it will be called once
// with each element of the array. This is useful for free()ing
// pointers to dynamically allocated memory, for example, if the
// alist holds (char *)s to malloc'd strings.
//
// If you wish to use *a again, it will need to be reinitialized.
void
alist_free(struct alist *a, void (*destruct)(void *elem)) {
    if(destruct != NULL)
        for(size_t i = 0; i<a->len; i++)
            (*destruct)(a->buf + i * a->elem_size);
    a->cap = 0;
    a->len = 0;
    a->elem_size = 0;
    free(a->buf);
    a->buf = NULL;
}


void
alist_clear(struct alist *a, void (*destruct)(void *elem)) {
    if(destruct != NULL)
        for(size_t i = 0; i < a->len; i++)
            (*destruct)(a->buf + i * a->elem_size);
    a->len = 0;
}


ssize_t
alist_in(struct alist *a, void *elem)
{
    for (size_t i = 0; i<a->len; i++)
        if (memcmp(alist_get(a, i), elem, a->elem_size) == 0)
	  return i <= SSIZE_MAX ? (ssize_t)i : -1;
    return -1;
}


void
alist_qsort(struct alist *a, int (*compar)(const void *a, const void *b))
{
    qsort(a->buf, a->len, a->elem_size, compar);
}


ssize_t
alist_bsearch(struct alist *a,
               const void *key,
               int (*compar)(const void *a, const void *b))
{
    char *key_p = bsearch(key,
                   a->buf,
                   a->len,
                   a->elem_size,
                   compar);
    if (key_p) {
	ptrdiff_t ptr_diff = key_p - a->buf;
	ssize_t idx = ptr_diff / (ssize_t)a->elem_size;
	if (idx < SSIZE_MAX)
	    return (ssize_t)idx;
	else
	    return -1;
    } else {
        return -1;
    }
}
