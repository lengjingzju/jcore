/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlog_core.h"
#include "jrbtree.h"
#include "jheap.h"

typedef struct {
    struct jrbtree node;
    int value;
} jrbtree_data_t;

static struct jrbtree_root s_rbtree_root;

static int rbtree_key_cmp(struct jrbtree *node, const void *key)
{
    jrbtree_data_t *data = jrbtree_entry(node, jrbtree_data_t, node);
    int vkey = *(int *)key;

    return data->value - vkey;
}

static int rbtree_node_cmp(struct jrbtree *a, struct jrbtree *b)
{
    jrbtree_data_t *da = jrbtree_entry(a, jrbtree_data_t, node);
    jrbtree_data_t *db = jrbtree_entry(b, jrbtree_data_t, node);

    return da->value - db->value;
}

static void rbtree_print_one_node(struct jrbtree *node)
{
    jrbtree_data_t *data = jrbtree_entry(node, jrbtree_data_t, node);
    if (node->left_son)
        rbtree_print_one_node(node->left_son);
    printf(" %d", data->value);
    if (node->right_son)
        rbtree_print_one_node(node->right_son);
}

static void rbtree_print_all(void)
{
    struct jrbtree_root *root = &s_rbtree_root;

    if (root->head) {
        printf("All rbtree nodes (%d):", root->num);
        rbtree_print_one_node(root->head);
        printf("\n");
    }
}

static void rbtree_del_all(void)
{
    struct jrbtree_root *root = &s_rbtree_root;
    struct jrbtree *node = NULL;
    jrbtree_data_t *data = NULL;

    if (!root->head)
        return;
    node = jrbtree_first(root);
    while (node) {
        data = jrbtree_entry(node, jrbtree_data_t, node);
        node = jrbtree_next(node);
        jrbtree_del(root, &data->node);
        jheap_free(data);
    }
    printf("Del all nodes.\n");
    rbtree_print_all();
}

static int rbtree_add_node(int value)
{
    struct jrbtree_root *root = &s_rbtree_root;
    jrbtree_data_t *data = NULL;

    data = (jrbtree_data_t *)jheap_calloc(1, sizeof(jrbtree_data_t));
    if (!data) {
        LLOG_ERROR("calloc failed!\n");
        return -1;
    }

    data->value = value;
    jrbtree_add(root, &data->node);
    printf("Add %d, ", data->value);
    rbtree_print_all();

    return 0;
}

static int rbtree_del_node(int value)
{
    struct jrbtree_root *root = &s_rbtree_root;
    struct jrbtree *node = NULL;
    jrbtree_data_t *data = NULL;

    node = jrbtree_search(root, &value);
    if (!node) {
        LLOG_ERROR("node(%d) not found!\n", value);
        return -1;
    }
    data = jrbtree_entry(node, jrbtree_data_t, node);
    jrbtree_del(root, node);
    jheap_free(data);
    printf("Del %d, ", value);
    rbtree_print_all();

    return 0;
}

int main(void)
{
    struct jrbtree_root *root = &s_rbtree_root;
    int orig[] = {1, 2, 5, 4, 8, 9, 3, 7, 6, 10};
    int i;
    int len = sizeof(orig) / sizeof(int);
    int hlen = len >> 1;

#if JHEAP_DEBUG
    jheap_init_debug(4);
    jheap_start_debug();
#endif

    root->num = 0;
    root->head = NULL;
    root->key_cmp = rbtree_key_cmp;
    root->node_cmp = rbtree_node_cmp;

    for (i = 0; i < len; ++i)
        rbtree_add_node(orig[i]);
    for (i = 0; i < hlen; ++i)
        rbtree_del_node(orig[i]);
    rbtree_del_all();

#if JHEAP_DEBUG
    jheap_leak_debug(0);
    jheap_leak_debug(3);
    jheap_uninit_debug();
#endif
    return 0;
}
