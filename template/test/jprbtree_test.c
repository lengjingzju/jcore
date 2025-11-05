/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlog_core.h"
#include "jheap.h"
#include "jprbtree.h"

static struct jprbtree_root s_rbtree_root;
static struct jprbtree_pool s_rbtree_pool;

static int rbtree_key_cmp(void **node, const void *key)
{
    return **(int **)node - *(const int *)key;
}

static int rbtree_node_cmp(void **a, void **b)
{
    return **(int **)a - **(int **)b;
}

static void rbtree_print_all(void)
{
    struct jprbtree_root *root = &s_rbtree_root;
    void **node;

    printf("All rbtree nodes (%d): ", root->num);
    node = jprbtree_first(root);
    while (node) {
        printf("%d ", **(int **)node);
        node = jprbtree_next(root, node);
    }
}

static void rbtree_del_all(void)
{
    struct jprbtree_root *root = &s_rbtree_root;
    struct jprbtree_pool *pool = &s_rbtree_pool;
    void **node, **next;

    node = jprbtree_first(root);
    while (node) {
        next = jprbtree_next(root, node);
        jprbtree_del(root, node);
        jheap_free(*(int **)node);
        pool->free(pool, node);
        node = next;
    }
    printf("Del all nodes. ");
    rbtree_print_all();
    printf("\n");
}

static int rbtree_add_node(int value)
{
    struct jprbtree_root *root = &s_rbtree_root;
    struct jprbtree_pool *pool = &s_rbtree_pool;
    void **node;

    node = pool->alloc(pool);
    *(int **)node = (int *)jheap_malloc(sizeof(int));
    **(int **)node = value;
    jprbtree_add(root, node);
    printf("Add %d, ", **(int **)node);
    rbtree_print_all();
    printf("\n");

    return 0;
}

static int rbtree_del_node(int value)
{
    struct jprbtree_root *root = &s_rbtree_root;
    struct jprbtree_pool *pool = &s_rbtree_pool;
    void **node;

    node = jprbtree_search(root, &value);
    if (!node) {
        LLOG_ERROR("node(%d) not found!\n", value);
        return -1;
    }
    jprbtree_del(root, node);
    jheap_free(*(int **)node);
    pool->free(pool, node);
    printf("Del %d, ", value);
    rbtree_print_all();
    printf("\n");

    return 0;
}

int main(void)
{
    struct jprbtree_root *root = &s_rbtree_root;
    struct jprbtree_pool *pool = &s_rbtree_pool;

    int orig[] = {1, 2, 5, 4, 8, 9, 3, 7, 6, 10};
    int i;
    int len = sizeof(orig) / sizeof(int);
    int hlen = len >> 1;

    jprbtree_init_pool(pool, 128);
    jprbtree_init(root, pool, rbtree_key_cmp, rbtree_node_cmp);

    for (i = 0; i < len; ++i)
        rbtree_add_node(orig[i]);
    for (i = 0; i < hlen; ++i)
        rbtree_del_node(orig[i]);

    rbtree_del_all();
    jprbtree_uninit_pool(pool);
    return 0;
}
