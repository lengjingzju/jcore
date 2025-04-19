/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <string.h>
#include "jheap.h"
#include "jvector.h"

int jvector_init(struct jvector *vec, size_t element_size, size_t add_capacity, size_t init_capacity)
{
    if (!element_size)
        return -1;
    vec->size = 0;
    vec->esize = element_size;
    vec->asize = add_capacity ? add_capacity : JVECTOR_ASIZE_DEF;
    vec->capacity = init_capacity ? init_capacity : vec->asize;
    vec->data = jheap_malloc(vec->capacity * element_size);
    if (!vec->data) {
        vec->capacity = 0;
        return -1;
    }
    return 0;
}

void jvector_uninit(struct jvector *vec)
{
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
    vec->size = 0;
    vec->capacity = 0;
}

static inline int jvector_incsize(struct jvector *vec, size_t new_capacity)
{
    void *new_data = jheap_realloc(vec->data, new_capacity * vec->esize);
    if (!new_data)
        return -1;
    vec->capacity = new_capacity;
    vec->data = new_data;
    return 0;
}

int jvector_resize(struct jvector *vec, size_t new_capacity)
{
    if (new_capacity < vec->capacity) {
        vec->data = jheap_realloc(vec->data, new_capacity * vec->esize);
        vec->capacity = new_capacity;
        if (vec->size > new_capacity)
            vec->size = new_capacity;
    } else if (new_capacity > vec->capacity) {
        return jvector_incsize(vec, new_capacity);
    }
    return 0;
}

int jvector_push(struct jvector *vec, const void *element)
{
    if (vec->size >= vec->capacity) {
        if (jvector_incsize(vec, vec->size + 1 + vec->asize) < 0)
            return -1;
    }
    memcpy((char *)vec->data + vec->size * vec->esize, element, vec->esize);
    ++vec->size;
    return 0;
}

int jvector_pop(struct jvector *vec, void *element)
{
    if (!vec->size)
        return -1;
    --vec->size;
    if (element)
        memcpy(element, (char *)vec->data + vec->size * vec->esize, vec->esize);
    return 0;
}

int jvector_pushs(struct jvector *vec, const void *elements, size_t num)
{
    if (vec->size + num > vec->capacity) {
        if (jvector_incsize(vec, vec->size + num + vec->asize) < 0)
            return -1;
    }
    memcpy((char *)vec->data + vec->size * vec->esize, elements, num * vec->esize);
    vec->size += num;
    return 0;
}

int jvector_pops(struct jvector *vec, void *elements, size_t *num)
{
    if (!vec->size) {
        *num = 0;
        return -1;
    }

    if (*num >= vec->size) {
        *num = vec->size;
        vec->size = 0;
        if (elements)
            memcpy(elements, vec->data, *num * vec->esize);
    } else {
        vec->size -= *num;
        if (elements)
            memcpy(elements, (char *)vec->data + vec->size * vec->esize, *num * vec->esize);
    }
    return 0;
}

int jvector_insert(struct jvector *vec, const void *element, size_t index)
{
    if (index > vec->size)
        return -1;
    if (vec->size >= vec->capacity) {
        if (jvector_incsize(vec, vec->size + 1 + vec->asize) < 0)
            return -1;
    }

    if (index == vec->size) {
        memcpy((char *)vec->data + vec->size * vec->esize, element, vec->esize);
        ++vec->size;
    } else {
        size_t diff = vec->size - index;
        char *src = (char *)vec->data + index * vec->esize;
        char *dst = src + vec->esize;
        memmove(dst, src, diff * vec->esize);
        memcpy(src, element, vec->esize);
        ++vec->size;
    }
    return 0;
}

int jvector_erase(struct jvector *vec, size_t index)
{
    if (index >= vec->size)
        return -1;

    if (index == vec->size - 1) {
        --vec->size;
    } else {
        size_t diff = vec->size - index - 1;
        char *dst = (char *)vec->data + index * vec->esize;
        char *src = dst + vec->esize;
        memmove(dst, src, diff * vec->esize);
        --vec->size;
    }
    return 0;
}

int jvector_inserts(struct jvector *vec, const void *elements, size_t num, size_t index)
{
    if (index > vec->size)
        return -1;
    if (vec->size + num > vec->capacity) {
        if (jvector_incsize(vec, vec->size + num + vec->asize) < 0)
            return -1;
    }

    if (index == vec->size) {
        memcpy((char *)vec->data + vec->size * vec->esize, elements, num * vec->esize);
        vec->size += num;
    } else  {
        size_t diff = vec->size - index;
        size_t bytes = num * vec->esize;
        char *src = (char *)vec->data + index * vec->esize;
        char *dst = src + bytes;
        memmove(dst, src, diff * vec->esize);
        memcpy(src, elements, bytes);
        vec->size += num;
    }
    return 0;
}

int jvector_erases(struct jvector *vec, size_t *num, size_t index)
{
    if (index >= vec->size) {
        *num = 0;
        return -1;
    }

    if (*num + index >= vec->size) {
        *num = vec->size - index;
        vec->size = index;
    } else {
        size_t diff = vec->size - index - *num;
        char *dst = (char *)vec->data + index * vec->esize;
        char *src = dst + *num * vec->esize;
        memmove(dst, src, diff * vec->esize);
        vec->size -= *num;
    }
    return 0;
}
