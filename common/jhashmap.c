/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include "jhashmap.h"
#include "jheap.h"

/* 最大移位 */
#if JHASHMAP_SHORT_FLAG
#define JHASHMAP_MAX_SHIFT      15
#else
#define JHASHMAP_MAX_SHIFT      31
#endif

static inline void _add_bucket(unsigned int _new, unsigned int prev, struct jhashmap_bucket *buckets)
{
    struct jhashmap_bucket *_newd = &buckets[_new];
    struct jhashmap_bucket *prevd = &buckets[prev];
    struct jhashmap_bucket *nextd = &buckets[prevd->next];
    nextd->prev = _new;
    _newd->next = prevd->next;
    _newd->prev = prev;
    prevd->next = _new;
}

static inline void _del_bucket(unsigned int _del, struct jhashmap_bucket *buckets)
{
   struct jhashmap_bucket *_deld = &buckets[_del];
   struct jhashmap_bucket *prevd = &buckets[_deld->prev];
   struct jhashmap_bucket *nextd = &buckets[_deld->next];
   nextd->prev = _deld->prev;
   prevd->next = _deld->next;
   _deld->prev = 0;
   _deld->next = 0;
}

int jhashmap_init(struct jhashmap_hd *hd, unsigned int bucket_shift)
{
    if (!hd->key_hash || !hd->node_hash || !hd->key_cmp || !hd->node_cmp || bucket_shift > JHASHMAP_MAX_SHIFT)
        return -1;

    hd->num = 0;
    hd->bucket_shift = bucket_shift;
    hd->bucket_num = 1U << bucket_shift;
    hd->bucket_mask = hd->bucket_num - 1;

    hd->buckets = (struct jhashmap_bucket *)jheap_calloc(hd->bucket_num + 1, sizeof(struct jhashmap_bucket));
    if (!hd->buckets)
        return -1;
    hd->buckets[hd->bucket_num].next = hd->bucket_num;
    hd->buckets[hd->bucket_num].prev = hd->bucket_num;

    return 0;
}

int jhashmap_resize(struct jhashmap_hd *hd, unsigned int bucket_shift)
{
    unsigned int bucket_num = 1U << bucket_shift;
    unsigned int bucket_mask = bucket_num - 1;
    struct jhashmap_bucket *hd_head = &hd->buckets[hd->bucket_num];
    struct jhashmap_bucket *buckets;

    if (bucket_shift > JHASHMAP_MAX_SHIFT)
        return -1;
    if (hd->bucket_shift == bucket_shift)
        return 0;

    buckets = (struct jhashmap_bucket *)jheap_calloc(bucket_num + 1, sizeof(struct jhashmap_bucket));
    if (!buckets) {
        return -1;
    }
    buckets[bucket_num].next = bucket_num;
    buckets[bucket_num].prev = bucket_num;

    if (hd->bucket_shift < bucket_shift) {
        struct jhashmap_bucket *cur;
        for (cur = &hd->buckets[hd_head->next]; cur != hd_head; cur = &hd->buckets[cur->next]) {
            struct jhashmap *c = cur->head.next, *n;
            while (c) {
                unsigned int hash = hd->node_hash(c);
                unsigned int pos = hash & bucket_mask;

                n = c->next;
                if (!buckets[pos].head.next)
                    _add_bucket(pos, bucket_num, buckets);
                c->next = buckets[pos].head.next;
                buckets[pos].head.next = c;
                c = n;
            }
        }
    } else {
        struct jhashmap_bucket *cur;
        for (cur = &hd->buckets[hd_head->next]; cur != hd_head; cur = &hd->buckets[cur->next]) {
            struct jhashmap *c = cur->head.next, *n;
            unsigned int hash = hd->node_hash(c);
            unsigned int pos = hash & bucket_mask;

            if (!buckets[pos].head.next) {
                _add_bucket(pos, bucket_num, buckets);
                buckets[pos].head.next = c;
            } else {
                n = c->next;
                while (n) { c = n; n = n->next; }
                c->next = buckets[pos].head.next;
                buckets[pos].head.next = cur->head.next;
            }
        }
    }

    hd->bucket_shift = bucket_shift;
    hd->bucket_num = bucket_num;
    hd->bucket_mask = hd->bucket_num - 1;
    jheap_free(hd->buckets);
    hd->buckets = buckets;

    return 0;
}

void jhashmap_uninit(struct jhashmap_hd *hd)
{
    hd->num = 0;
    if (hd->buckets) {
        jheap_free(hd->buckets);
        hd->buckets = NULL;
    }
}

struct jhashmap *jhashmap_find(struct jhashmap_hd *hd, const void *key, struct jhashmap **prev)
{
    unsigned int hash = hd->key_hash(key);
    unsigned int pos = hash & hd->bucket_mask;
    struct jhashmap *head = &hd->buckets[pos].head;
    struct jhashmap *p = head;
    struct jhashmap *c = head->next;

    while (c) {
        if (hd->key_cmp(c, key) == 0) {
            if (prev)
                *prev = p;
            return c;
        }
        p = c;
        c = c->next;
    }

    return NULL;
}

struct jhashmap *jhashmap_find_node(struct jhashmap_hd *hd, struct jhashmap *node, struct jhashmap **prev)
{
    unsigned int hash = hd->node_hash(node);
    unsigned int pos = hash & hd->bucket_mask;
    struct jhashmap *head = &hd->buckets[pos].head;
    struct jhashmap *p = head;
    struct jhashmap *c = head->next;

    while (c) {
        if (hd->node_cmp(c, node) == 0) {
            if (prev)
                *prev = p;
            return c;
        }
        p = c;
        c = c->next;
    }

    return NULL;
}

void jhashmap_fast_add(struct jhashmap_hd *hd, struct jhashmap *node)
{
    unsigned int hash = hd->node_hash(node);
    unsigned int pos = hash & hd->bucket_mask;
    struct jhashmap *head = &hd->buckets[pos].head;

    if (!head->next)
        _add_bucket(pos, hd->bucket_num, hd->buckets);
    node->next = head->next;
    head->next = node;
    ++hd->num;
}

struct jhashmap *jhashmap_add(struct jhashmap_hd *hd, struct jhashmap *node)
{
    unsigned int hash = hd->node_hash(node);
    unsigned int pos = hash & hd->bucket_mask;
    struct jhashmap *head = &hd->buckets[pos].head;
    struct jhashmap *p = head;
    struct jhashmap *c = head->next;

    while (c) {
        if (hd->node_cmp(c, node) == 0) {
            p->next = node;
            node->next = c->next;
            c->next = NULL;
            return c;
        }
        p = c;
        c = c->next;
    }

    if (!head->next)
        _add_bucket(pos, hd->bucket_num, hd->buckets);
    node->next = head->next;
    head->next = node;
    ++hd->num;

    return NULL;
}

int jhashmap_del(struct jhashmap_hd *hd, struct jhashmap *node, struct jhashmap *prev)
{
    unsigned int hash = hd->node_hash(node);
    unsigned int pos = hash & hd->bucket_mask;
    struct jhashmap *head = &hd->buckets[pos].head;

    if (!prev) {
        struct jhashmap *p = head;
        struct jhashmap *c = head->next;
        while (c) {
            if (hd->node_cmp(c, node) == 0) {
                prev = p;
                goto success;
            }
            p = c;
            c = c->next;
        }
        return -1;
    }

success:
    --hd->num;
    prev->next = node->next;
    node->next = NULL;
    if (!head->next)
        _del_bucket(pos, hd->buckets);
    return 0;
}

void jhashmap_loop(struct jhashmap_hd *hd, uintptr_t (*cb)(struct jhashmap *node))
{
    struct jhashmap_bucket *cur, *next;
    struct jhashmap_bucket *hd_head = &hd->buckets[hd->bucket_num];

    if (!cb || !hd->buckets)
        return;

    for (cur = &hd->buckets[hd_head->next], next = &hd->buckets[cur->next]; \
            cur != hd_head; cur = next, next = &hd->buckets[next->next]) {
        uintptr_t ret = JHASHMAP_NEXT;
        struct jhashmap *head = &cur->head, *p = head, *c = p->next, *n;
        struct jhashmap node;

        while (c) {
            node = *c;
            n = c->next;

            ret = cb(c);
            switch (ret) {
            case JHASHMAP_NEXT:
                p = c;
                break;
            case JHASHMAP_STOP:
                return;
            case JHASHMAP_DEL:
                --hd->num;
                p->next = node.next;
                if (!head->next)
                    _del_bucket(next->prev, hd->buckets);
                break;
            default: // JHASHMAP_ADD
                p = (struct jhashmap *)ret;
                p->next = n;
                ++hd->num;
                break;
            }
            c = p->next;
        }
    }
}
