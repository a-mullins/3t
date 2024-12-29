// A simple dynamic array. Also known as a `vector` in the C++ world.
#pragma once

typedef struct darray {
    size_t cap;
    size_t len;
    size_t elem_size;
    void *buf;
} darray;

void
darray_init(darray *d, size_t elem_size);

void *
darray_get(darray *d, size_t n);

void
darray_push(darray *d, void *elem);

void
darray_clear(darray *d);
