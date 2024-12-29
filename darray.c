#include <stdlib.h>
#include <string.h>
#include "darray.h"


void
darray_init(darray *d, size_t elem_size)
{
    d->cap = MIN_CAP;
    d->len = 0;
    d->elem_size = elem_size;
    d->buf = calloc(MIN_CAP, elem_size);
}


void *
darray_get(darray *d, size_t n)
{
    return d->buf + d->elem_size * n;
}


void
darray_push(darray *d, void *elem)
{
    if(d->len+1 >= d->cap) {
        d->cap <<= 1;
        // FIXME if realloc fails, we loose access to d->buf pointer
        d->buf = realloc(d->buf, d->elem_size * d->cap);
    }
    memcpy(d->buf + d->len * d->elem_size, elem, d->elem_size);
    d->len++;
}


void
darray_clear(darray *d) {
    d->len = 0;
}


void
darray_free(darray *d) {
    d->cap = 0;
    d->len = 0;
    d->elem_size = 0;
    free(d->buf);
}
