/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jbheap.h"
#include "jheap.h"

#define DEF_BLOCK_SIZE      8192

static jbheap_node_t s_invalid_node;

static jbheap_node_t *jbheap_new_node(jbheap_mgr_t *mgr, int size)
{
    jbheap_node_t *node = NULL;

    if (!(node = (jbheap_node_t *)jheap_malloc(sizeof(jbheap_node_t)))) {
        return NULL;
    }
    node->size = size;
    node->cur = 0;
    if (!(node->ptr = (char *)jheap_malloc(node->size))) {
        jheap_free(node);
        return NULL;
    }
    jslist_add_head(&node->list, &mgr->head);

    return node;
}

static void *jbheap_fast_alloc(jbheap_mgr_t *mgr, int size)
{
    void *p = NULL;

    if (mgr->cur->cur + size > mgr->cur->size) {
        if (!(jbheap_new_node(mgr, mgr->size >= size ? mgr->size : size))) {
            return NULL;
        }
        mgr->cur = (jbheap_node_t *)(mgr->head.next);
    }

    p = (void *)(mgr->cur->ptr + mgr->cur->cur);
    mgr->cur->cur += size;
    return p;
}

static void *jbheap_align_alloc(jbheap_mgr_t *mgr, int size)
{
    void *p = NULL;
    jbheap_node_t *pos = NULL;
    int dsize;

    dsize = (size >> mgr->align) << mgr->align;
    if (dsize != size)
        dsize += 1 << mgr->align;

    if (mgr->cur->cur + dsize > mgr->cur->size) {
        if (mgr->flag) {
            jslist_for_each_entry(pos, &mgr->head, list, jbheap_node_t) {
                if (pos->cur + dsize <= pos->size) {
                    mgr->cur = pos;
                    goto end;
                }
            }
        }

        if (!(jbheap_new_node(mgr, mgr->size >= dsize ? mgr->size : dsize))) {
            return NULL;
        }
        mgr->cur = (jbheap_node_t *)(mgr->head.next);
    }

end:
    p = (void *)(mgr->cur->ptr + mgr->cur->cur);
    mgr->cur->cur += dsize;
    return p;
}

void jbheap_init(jbheap_mgr_t *mgr)
{
    jslist_init_head(&mgr->head);
    mgr->cur = &s_invalid_node;

    if (mgr->size <= 0) {
        mgr->size = DEF_BLOCK_SIZE;
    }
    if (mgr->align < 0) {
        int i = 0;
        while ((1 << i) < (int)sizeof(void *))
            ++i;
        mgr->align = i;
    }
    mgr->alloc = mgr->align ? jbheap_align_alloc : jbheap_fast_alloc;
}

void jbheap_uninit(jbheap_mgr_t *mgr)
{
    jbheap_node_t *p = NULL, *pos = NULL, *n = NULL;

    jslist_for_each_entry_safe(p, pos, n, &mgr->head, list, jbheap_node_t) {
        jslist_del(&pos->list, &p->list, &mgr->head);
        jheap_free(pos->ptr);
        jheap_free(pos);
        pos = p;
    }
    mgr->cur = &s_invalid_node;
}

int jbheap_tell(jbheap_mgr_t *mgr, int *used)
{
    jbheap_node_t *pos = NULL;
    int size = 0, cur = 0;

    jslist_for_each_entry(pos, &mgr->head, list, jbheap_node_t) {
        size += pos->size;
        cur += pos->cur;
    }
    if (used)
        *used = cur;

    return size;
}

void jbheap_adjust(jbheap_mgr_t *mgr)
{
    jbheap_node_t *pos = NULL;

    jslist_for_each_entry(pos, &mgr->head, list, jbheap_node_t) {
        if (pos->size != pos->cur) {
            pos->size = pos->cur;
            pos->ptr = (char *)jheap_realloc(pos->ptr, pos->size);
        }
    }
}
