/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlog_core.h"
#include "jplist.h"

/* 运行的命令：./jlist.sh jplist "struct jplist" "int" */

static struct jplist_root s_list_root;
static struct jplist_pool s_list_pool;

static void list_print_all(void)
{
    struct jplist_root *root = &s_list_root;
    int *node;

    printf("All list nodes (%d): ", root->num);
    jplist_for_each_entry(node, root) {
        printf("%d ", *node);
    }
}

static void list_del_all(void)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    int *node, *next;

    jplist_for_each_entry_safe(node, next, root) {
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
    int *node;

    node = pool->alloc(pool);
    *node = value;
    jplist_add_last(root, node);
    printf("Add %d, ", *node);
    list_print_all();
    printf("\n");

    return 0;
}

static int list_add_node_first(int value)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    int *node;

    node = pool->alloc(pool);
    *node = value;
    jplist_add_first(root, node);
    printf("Add tail %d, ", *node);
    list_print_all();
    printf("\n");

    return 0;
}

static int list_del_node(int value)
{
    struct jplist_root *root = &s_list_root;
    struct jplist_pool *pool = &s_list_pool;
    int *node = NULL;

    jplist_for_each_entry(node, root) {
        if (*node == value)
            goto next;
    }

    if (!node) {
        LLOG_ERROR("node(%d) not found!\n", value);
        return -1;
    }
next:
    jplist_del(root, node);
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
