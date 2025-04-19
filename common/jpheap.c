/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdint.h>
#include "jpheap.h"
#include "jheap.h"

static inline void *_jpheap_alloc00(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint8_t *)mgr->idx)[mgr->sel++]);
    return NULL;
}

static inline void *_jpheap_alloc01(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint16_t *)mgr->idx)[mgr->sel++]);
    return NULL;
}

static inline void *_jpheap_alloc02(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint32_t *)mgr->idx)[mgr->sel++]);
    return NULL;
}

static inline void *_jpheap_alloc10(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint8_t *)mgr->idx)[mgr->sel++]);
    return jheap_malloc(mgr->size);
}

static inline void *_jpheap_alloc11(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint16_t *)mgr->idx)[mgr->sel++]);
    return jheap_malloc(mgr->size);
}

static inline void *_jpheap_alloc12(jpheap_mgr_t *mgr)
{
    if (mgr->sel < mgr->num)
        return (void *)(mgr->begin + mgr->size * ((uint32_t *)mgr->idx)[mgr->sel++]);
    return jheap_malloc(mgr->size);
}

static inline void _jpheap_free00(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    ((uint8_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

static inline void _jpheap_free01(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    ((uint16_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

static inline void _jpheap_free02(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    ((uint32_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

static inline void _jpheap_free10(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    if ((char *)ptr < mgr->begin || (char *)ptr >= mgr->end)
        jheap_free(ptr);
    else
        ((uint8_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

static inline void _jpheap_free11(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    if ((char *)ptr < mgr->begin || (char *)ptr >= mgr->end)
        jheap_free(ptr);
    else
        ((uint16_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

static inline void _jpheap_free12(jpheap_mgr_t *mgr, void *ptr)
{
    if (!ptr) return;
    if ((char *)ptr < mgr->begin || (char *)ptr >= mgr->end)
        jheap_free(ptr);
    else
        ((uint32_t *)mgr->idx)[--mgr->sel] = ((char *)ptr - mgr->begin) / mgr->size;
}

int jpheap_init(jpheap_mgr_t *mgr)
{
    int i = 0, tmp = 0;

    if (mgr->size <= 0 || mgr->num <= 0)
        return -1;

    if (mgr->num <= 256) {
        tmp = mgr->num * sizeof(uint8_t);
    } else if (mgr->num <= 65536) {
        tmp = mgr->num * sizeof(uint16_t);
    } else {
        tmp = mgr->num * sizeof(uint32_t);
    }
    if (!(mgr->idx = jheap_malloc(tmp))) {
        return -1;
    }

    tmp = mgr->num * mgr->size;
    if (!(mgr->begin = (char *)jheap_malloc(tmp))) {
        jheap_free(mgr->idx);
        mgr->idx = NULL;
        return -1;
    }
    mgr->end = mgr->begin + tmp;

    mgr->sel = 0;
    if (mgr->num <= 256) {
        for (i = 0; i < mgr->num; ++i)
            ((uint8_t *)mgr->idx)[i] = i;
    } else if (mgr->num <= 65536) {
        for (i = 0; i < mgr->num; ++i)
            ((uint16_t *)mgr->idx)[i] = i;
    } else {
        for (i = 0; i < mgr->num; ++i)
            ((uint32_t *)mgr->idx)[i] = i;
    }

    if (!mgr->sys) {
        if (mgr->num <= 256) {
            mgr->alloc = _jpheap_alloc00;
            mgr->free = _jpheap_free00;
        } else if (mgr->num <= 65536) {
            mgr->alloc = _jpheap_alloc01;
            mgr->free = _jpheap_free01;
        } else {
            mgr->alloc = _jpheap_alloc02;
            mgr->free = _jpheap_free02;
        }
    } else {
        if (mgr->num <= 256) {
            mgr->alloc = _jpheap_alloc10;
            mgr->free = _jpheap_free10;
        } else if (mgr->num <= 65536) {
            mgr->alloc = _jpheap_alloc11;
            mgr->free = _jpheap_free11;
        } else {
            mgr->alloc = _jpheap_alloc12;
            mgr->free = _jpheap_free12;
        }
    }
    return 0;
}

void jpheap_uninit(jpheap_mgr_t *mgr)
{
    if (!mgr)
        return;

    if (mgr->idx) {
        jheap_free(mgr->idx);
        mgr->idx = NULL;
        mgr->sel = 0;
    }
    if (mgr->begin) {
        jheap_free(mgr->begin);
        mgr->begin = NULL;
        mgr->end = NULL;
    }
}
