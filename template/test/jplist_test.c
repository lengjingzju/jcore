/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlog_core.h"
#include "jheap.h"
#include "jplist.h"

static struct jplist_root s_list_root;
static struct jplist_pool s_list_pool;

static void list_print_all(void)
{
    struct jplist_root *root = &s_list_root;
    void **node;

    printf("All list nodes (%d): ", root->num);
    jplist_for_each_entry(node, root) {
        printf("%d ", **(int **)node);
    }
}

static void list_del_all(void)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    void **node, **next;

    jplist_for_each_entry_safe(node, next, root) {
        jheap_free(*(int **)node);
        jplist_del(root, node);
        pool->free(pool, node);
    }

    printf("Del all nodes. ");
    list_print_all();
    printf("\n");
}

static int list_add_node_last(int value)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    void **node;

    node = pool->alloc(pool);
    *(int **)node = (int *)jheap_malloc(sizeof(int));
    **(int **)node = value;
    jplist_add_last(root, node);
    printf("Add %d, ", **(int **)node);
    list_print_all();
    printf("\n");

    return 0;
}

static int list_add_node_first(int value)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    void **node;

    node = pool->alloc(pool);
    *(int **)node = (int *)jheap_malloc(sizeof(int));
    **(int **)node = value;
    jplist_add_first(root, node);
    printf("Add tail %d, ", **(int **)node);
    list_print_all();
    printf("\n");

    return 0;
}

static int list_del_node(int value)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    void **node = NULL;

    jplist_for_each_entry(node, root) {
        if (**(int **)node == value)
            goto next;
    }

    if (!node) {
        LLOG_ERROR("node(%d) not found!\n", value);
        return -1;
    }
next:
    jplist_del(root, node);
    jheap_free(*(int **)node);
    pool->free(pool, node);
    printf("Del %d, ", value);
    list_print_all();
    printf("\n");

    return 0;
}

int main(void)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;

    int orig[] = {1, 2, 5, 4, 8, 9, 3, 7, 6, 10};
    int i;
    int len = sizeof(orig) / sizeof(int);
    int hlen = len >> 1;

    jplist_init_pool(pool, 128);
    jplist_init(root, pool);

    for (i = 0; i < hlen; ++i)
        list_add_node_last(orig[i]);
    for (i = hlen; i < len; ++i)
        list_add_node_first(orig[i]);
    for (i = 0; i < hlen; ++i)
        list_del_node(orig[i]);

    list_del_all();
    jplist_uninit(root);
    jplist_uninit_pool(pool);
    return 0;
}
