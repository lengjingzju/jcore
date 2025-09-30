#!/bin/sh
############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2024-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
# https://github.com/lengjingzju/jcore     #
############################################

name=$1
T=$2
ST=$3
compact=$4
NAME=$(echo "${name}" | tr 'a-z' 'A-Z') # 大写的名称

if [ -z "${name}" ] || [ -z "${T}" ]  || [ -z "${ST}" ] || [ "${name}" = "-h" ]; then
    echo "Usage: $0 <name> <T> <ST> <compact>"
    echo "       name:      生成的头文件或源文件名称、管理结构类型名和函数名前缀"
    echo "       T:         变量类型名，该类型下自动创建，包含一个树节点成员node和一个ST类型的数据成员"
    echo "       ST:        子变量类型名"
    echo "       compact:   省内存的可选选项，设置为y时使用10bits记录索引，更省内存，"
    echo "                  但节点数限制为了1022；否则使用21bits，支持节点数更多(2097150)，但节点内存翻倍"
    exit 1
fi

invalid=2097151
node_t="struct jprbtree_long"
if [ "${compact}" = "y" ]; then
    invalid=1023
    node_t="struct jprbtree_short"
fi

echo "${name}.h and ${name}.c have been generated."

##############
# 生成头文件 #
##############

cat <<EOF> ${name}.h
/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 红黑树的特性:
 * 1. 节点非黑即红
 * 2. 根节点是黑色
 * 3. 叶节点是黑色(叶子节点指最底端的节点NIL，指空节点)
 * 4. 红节点的子节点必须是黑色(路径中不能有两个连续的红节点)
 * 5. 从一个节点到该节点的子孙节点的所有路径上包含相同数目的黑节点(确保没有一条路径会比其他路径长出俩倍)
 */

/**
 * @brief   红黑树的节点结构点(紧凑型)
 * @note    索引位数是10位，"1023=(1<<10)-1"被定义成了无效序号，所以最大节点数是1022
 */
#ifndef JPRBTREE_SHORT
#define JPRBTREE_SHORT 1
struct jprbtree_short {
    uint32_t color      :2 ;
    uint32_t parent     :10;
    uint32_t right_son  :10;
    uint32_t left_son   :10;
};
#endif

/**
 * @brief   红黑树的节点结构(大容量型)
 * @note    索引位数是21位，"2097151=(1<<21)-1"被定义成了无效序号，所以最大节点数是2097150
 */
#ifndef JPRBTREE_LONG
#define JPRBTREE_LONG 1
struct jprbtree_long {
    uint64_t color      :1 ;
    uint64_t parent     :21;
    uint64_t right_son  :21;
    uint64_t left_son   :21;
};
#endif

/**
 * @brief   红黑树的节点类型名
 */
typedef ${node_t} ${name}_node_t;

/**
 * @brief   数据节点
 */
${T} {
    ${name}_node_t node;    // 树节点
    ${ST} data;             // 数据内容
};

/**
 * @brief   内存池管理结构
 */
struct ${name}_pool {
    int num;                // 内存单元总数
    int sel;                // 当前空闲的序号，sel等于num时表示内存耗尽
    void *idx;              // 内存池状态数组，指针类型如下: num <= 256 uint8_t; num <= 65536 uint16_t; other uint32_t
    ${T} *begin;            // 内存池开始地址
    ${T} *end;              // 内存池结束地址
    ${ST}* (*alloc)(struct ${name}_pool *pool); // 内存申请函数，返回指针
    void (*free)(struct ${name}_pool *pool, ${ST} *ptr); // 内存释放函数，通过指针
    int (*alloci)(struct ${name}_pool *pool); // 内存申请函数，返回序号，返回-1表示失败
    void (*freei)(struct ${name}_pool *pool, int index); // 内存释放函数，通过序号
    void (*freei_unsafe)(struct ${name}_pool *pool, int index); // 内存释放函数，通过序号，不检查序号合法性
    ${ST}* (*data)(struct ${name}_pool *pool, int index); // 通过序号得到指针
    ${ST}* (*data_unsafe)(struct ${name}_pool *pool, int index); // 通过序号得到指针，不检查序号合法性
    int (*index)(struct ${name}_pool *pool, ${ST} *ptr); // 通过指针得到序号，返回-1表示失败
    int (*index_unsafe)(struct ${name}_pool *pool, ${ST} *ptr); // 通过指针得到序号，不检查指针合法性
};

/**
 * @brief   初始化内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @param   num [IN] 元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_init_pool(struct ${name}_pool *pool, int num);

/**
 * @brief   扩容内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @param   num [IN] 新的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    num必须比原先的pool->num大，即只能扩容
 *          扩容realloc时begin地址可能被改变，正在使用内存地址时不能扩容
 */
int ${name}_resize_pool(struct ${name}_pool *pool, int num);

/**
 * @brief   反初始化内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @return  无返回值
 * @note    反初始化释放所有分配的内存
 */
void ${name}_uninit_pool(struct ${name}_pool *pool);

/**
 * @brief   红黑树的根
 * @note    首次使用红黑树前一定要设置num和head为空，且设置回调函数
 *          销毁红黑数全部节点再次使用红黑树一定要设置num和head为空
 */
struct ${name}_root {
    int num;                    // 节点的总数
    int index;                  // head,红黑树的根节点的序号，使用红黑树的入口
    struct ${name}_pool *pool;  // 内存池的指针，根据序号从内存池中获取数据指针
    int (*key_cmp)(${ST} *node, const void *key);  // 节点和key的比较函数
    int (*node_cmp)(${ST} *a, ${ST} *b);  // 节点之间的比较函数
};

/**
 * @brief   初始化红黑树的根节点
 * @param   root [IN] 红黑树的根
 * @param   pool [IN] 内存池的指针
 * @param   key_cmp [IN] 数据节点和key的比较函数
 * @param   node_cmp [IN] 数据节点之间的比较函数
 * @return  成功返回0; 失败返回-1
 * @note    init不分配资源，所以无需uninit
 */
int ${name}_init(struct ${name}_root *root, struct ${name}_pool *pool,
    int (*key_cmp)(${ST} *node, const void *key), int (*node_cmp)(${ST} *a, ${ST} *b));

/**
 * @brief   加入节点到红黑树
 * @param   root [IN] 红黑树的根
 * @param   node [INOUT] 要加入的数据节点
 * @return  成功返回0; 失败表示节点有冲突返回-1
 * @note    一定要设置node_cmp回调函数
 */
int ${name}_add(struct ${name}_root *root, ${ST} *node);

/**
 * @brief   从红黑树删除节点
 * @param   root [IN] 红黑树的根
 * @param   node [INOUT] 要删除的数据节点
 * @return  无返回值
 * @note    无
 */
void ${name}_del(struct ${name}_root *root, ${ST} *node);

/**
 * @brief   替换红黑树的旧节点为新节点
 * @param   root [IN] 红黑树的根
 * @param   old [IN] 被替换的数据节点
 * @param   node [INOUT] 新的数据节点
 * @return  无返回值
 * @note    old和node的key值必须相等
 */
void ${name}_replace(struct ${name}_root *root, ${ST} *old, ${ST} *node);

/**
 * @brief   查询红黑树指定key值的节点
 * @param   root [IN] 红黑树的根
 * @param   key [IN] 查询的key
 * @return  返回找到的数据节点
 * @note    一定要设置key_cmp回调函数
 */
${ST} *${name}_search(struct ${name}_root *root, const void *key);

/**
 * @brief   查询红黑树key值最小的节点
 * @param   root [IN] 红黑树的根
 * @return  返回找到的数据节点
 * @note    无
 */
${ST} *${name}_first(const struct ${name}_root *root);

/**
 * @brief   查询红黑树key值最大的节点
 * @param   root [IN] 红黑树的根
 * @return  返回找到的数据节点
 * @note    无
 */
${ST} *${name}_last(const struct ${name}_root *root);

/**
 * @brief   查询红黑树node节点的下一节点
 * @param   root [IN] 红黑树的根
 * @param   node [IN] 参考的数据节点
 * @return  返回找到的数据节点
 * @note    无
 */
${ST} *${name}_next(const struct ${name}_root *root, ${ST} *node);

/**
 * @brief   查询红黑树node节点的上一节点
 * @param   root [IN] 红黑树的根
 * @param   node [IN] 参考的数据节点
 * @return  返回找到的数据节点
 * @note    无
 */
${ST} *${name}_prev(const struct ${name}_root *root, ${ST} *node);

#ifdef __cplusplus
}
#endif
EOF

##############
# 生成源文件 #
##############

cat <<EOF> ${name}.c
/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <string.h>
#include "jheap.h"
#include "${name}.h"

#define jmalloc     jheap_malloc
#define jrealloc    jheap_realloc
#define jfree       jheap_free

/* jprbtree_long 是 “2097151=(1<<21)-1” ；jprbtree_short 是 “1023=(1<<10)-1” */
#define JRBTREE_INVALID                 ${invalid}
#define JRBTREE_RED                     0
#define JRBTREE_BLACK                   1

#define ${name}_node_entry(ptr)         ((${T} *)(ptr))
#define ${name}_data_entry(ptr)         ((${T} *)((char *)(ptr)-(uintptr_t)(&((${T} *)0)->data)))
#define ${NAME}_FROM_INDEX(root, index) (&(root)->pool->begin[index].node)
#define ${NAME}_FROM_DATA(ptr)          ((${name}_node_t *)((char *)(ptr)-(uintptr_t)(&((${T} *)0)->data)))

static inline ${ST} *_${name}_alloc00(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return &(pool->begin + ((uint8_t *)pool->idx)[pool->sel++])->data;
    return NULL;
}

static inline ${ST} *_${name}_alloc01(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return &(pool->begin + ((uint16_t *)pool->idx)[pool->sel++])->data;
    return NULL;
}

static inline ${ST} *_${name}_alloc02(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return &(pool->begin + ((uint32_t *)pool->idx)[pool->sel++])->data;
    return NULL;
}

static inline void _${name}_free00(struct ${name}_pool *pool, ${ST} *ptr)
{
    if (!ptr) return;
    ((uint8_t *)pool->idx)[--pool->sel] = ${name}_data_entry(ptr) - pool->begin;
}

static inline void _${name}_free01(struct ${name}_pool *pool, ${ST} *ptr)
{
    if (!ptr) return;
    ((uint16_t *)pool->idx)[--pool->sel] = ${name}_data_entry(ptr) - pool->begin;
}

static inline void _${name}_free02(struct ${name}_pool *pool, ${ST} *ptr)
{
    if (!ptr) return;
    ((uint32_t *)pool->idx)[--pool->sel] = ${name}_data_entry(ptr) - pool->begin;
}

static inline int _${name}_alloci0(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return ((uint8_t *)pool->idx)[pool->sel++];
    return -1;
}

static inline int _${name}_alloci1(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return ((uint16_t *)pool->idx)[pool->sel++];
    return -1;
}

static inline int _${name}_alloci2(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return ((uint32_t *)pool->idx)[pool->sel++];
    return -1;
}

static inline void _${name}_freei0(struct ${name}_pool *pool, int index)
{
    if (index < 0 || index >= pool->num) return;
    ((uint8_t *)pool->idx)[--pool->sel] = index;
}

static inline void _${name}_freei1(struct ${name}_pool *pool, int index)
{
    if (index < 0 || index >= pool->num) return;
    ((uint16_t *)pool->idx)[--pool->sel] = index;
}

static inline void _${name}_freei2(struct ${name}_pool *pool, int index)
{
    if (index < 0 || index >= pool->num) return;
    ((uint32_t *)pool->idx)[--pool->sel] = index;
}

static inline void _${name}_freei0_unsafe( struct ${name}_pool *pool, int index)
{
    ((uint8_t *)pool->idx)[--pool->sel] = index;
}

static inline void _${name}_freei1_unsafe( struct ${name}_pool *pool, int index)
{
    ((uint16_t *)pool->idx)[--pool->sel] = index;
}

static inline void _${name}_freei2_unsafe( struct ${name}_pool *pool, int index)
{
    ((uint32_t *)pool->idx)[--pool->sel] = index;
}

static inline ${ST} *_${name}_data(struct ${name}_pool *pool, int index)
{
    if (index < 0 || index >= pool->num) return NULL;
    return &pool->begin[index].data;
}

static inline ${ST} *_${name}_data_unsafe(struct ${name}_pool *pool, int index)
{
    return &pool->begin[index].data;
}

static inline int _${name}_index(struct ${name}_pool *pool, ${ST} *ptr)
{
    ${T} const *data = ${name}_data_entry(ptr);
    if (data < pool->begin || data >= pool->end)
        return -1;
    return data - pool->begin;
}

static inline int _${name}_index_unsafe(struct ${name}_pool *pool, ${ST} *ptr)
{
    return ${name}_data_entry(ptr) - pool->begin;
}

int ${name}_init_pool(struct ${name}_pool *pool, int num)
{
    int i = 0, tmp = 0;

    if (num <= 0)
        return -1;

    pool->num = num;
    if (pool->num <= 256) {
        tmp = pool->num * sizeof(uint8_t);
    } else if (pool->num <= 65536) {
        tmp = pool->num * sizeof(uint16_t);
    } else {
        tmp = pool->num * sizeof(uint32_t);
    }
    if (!(pool->idx = jmalloc(tmp))) {
        return -1;
    }

    tmp = pool->num * sizeof(${T});
    if (!(pool->begin = (${T} *)jmalloc(tmp))) {
        jfree(pool->idx);
        pool->idx = NULL;
        return -1;
    }
    pool->end = pool->begin + pool->num;

    pool->sel = 0;
    if (pool->num <= 256) {
        for (i = 0; i < pool->num; ++i)
            ((uint8_t *)pool->idx)[i] = i;
    } else if (pool->num <= 65536) {
        for (i = 0; i < pool->num; ++i)
            ((uint16_t *)pool->idx)[i] = i;
    } else {
        for (i = 0; i < pool->num; ++i)
            ((uint32_t *)pool->idx)[i] = i;
    }

    if (pool->num <= 256) {
        pool->alloc = _${name}_alloc00;
        pool->free = _${name}_free00;
        pool->alloci = _${name}_alloci0;
        pool->freei = _${name}_freei0;
        pool->freei_unsafe = _${name}_freei0_unsafe;
    } else if (pool->num <= 65536) {
        pool->alloc = _${name}_alloc01;
        pool->free = _${name}_free01;
        pool->alloci = _${name}_alloci1;
        pool->freei = _${name}_freei1;
        pool->freei_unsafe = _${name}_freei1_unsafe;
    } else {
        pool->alloc = _${name}_alloc02;
        pool->free = _${name}_free02;
        pool->alloci = _${name}_alloci2;
        pool->freei = _${name}_freei2;
        pool->freei_unsafe = _${name}_freei2_unsafe;
    }

    pool->data = _${name}_data;
    pool->data_unsafe = _${name}_data_unsafe;
    pool->index = _${name}_index;
    pool->index_unsafe = _${name}_index_unsafe;
    return 0;
}

int ${name}_resize_pool(struct ${name}_pool *pool, int num)
{
    ${T} *nbegin = NULL;
    void *nidx = NULL;
    int i = 0, tmp = 0;

    if (num <= pool->num)
        return -1;

    tmp = num * sizeof(${T});
    if (!(nbegin = (${T} *)jrealloc(pool->begin, tmp))) {
        return -1;
    }
    if (num <= 256) {
        tmp = num * sizeof(uint8_t);
    } else if (num <= 65536) {
        tmp = num * sizeof(uint16_t);
    } else {
        tmp = num * sizeof(uint32_t);
    }
    if (!(nidx = jmalloc(tmp))) {
        if (nbegin != pool->begin) {
            pool->begin = nbegin;
            pool->end = pool->begin + pool->num;
        }
        return -1;
    }

    if (num <= 256) {
        memcpy(nidx, pool->idx, pool->num);
        for (i = pool->num; i < num; ++i)
            ((uint8_t *)nidx)[i] = i;
    } else if (num <= 65536) {
        if (pool->num <= 256) {
            for (i = 0; i < pool->num; ++i)
                ((uint16_t *)nidx)[i] = ((uint8_t *)pool->idx)[i];
        } else {
            memcpy(nidx, pool->idx, pool->num * sizeof(uint16_t));
        }
        for (i = pool->num; i < num; ++i)
            ((uint16_t *)nidx)[i] = i;
    } else {
        if (pool->num <= 256) {
            for (i = 0; i < pool->num; ++i)
                ((uint32_t *)nidx)[i] = ((uint8_t *)pool->idx)[i];
        } if (pool->num <= 65536) {
            for (i = 0; i < pool->num; ++i)
                ((uint32_t *)nidx)[i] = ((uint16_t *)pool->idx)[i];
        } else {
            memcpy(nidx, pool->idx, pool->num * sizeof(uint32_t));
        }
        for (i = pool->num; i < num; ++i)
            ((uint32_t *)nidx)[i] = i;
    }

    pool->num = num;
    pool->begin = nbegin;
    pool->end = pool->begin + pool->num;
    pool->idx = nidx;
    return 0;
}

void ${name}_uninit_pool(struct ${name}_pool *pool)
{
    if (!pool)
        return;

    if (pool->idx) {
        jfree(pool->idx);
        pool->idx = NULL;
        pool->sel = 0;
    }
    if (pool->begin) {
        jfree(pool->begin);
        pool->begin = NULL;
        pool->end = NULL;
    }
}

int ${name}_init(struct ${name}_root *root, struct ${name}_pool *pool,
    int (*key_cmp)(${ST} *node, const void *key), int (*node_cmp)(${ST} *a, ${ST} *b))
{
    if (!root || !pool || !key_cmp || !node_cmp)
        return -1;

    root->num = 0;
    root->index = JRBTREE_INVALID;
    root->pool = pool;
    root->key_cmp = key_cmp;
    root->node_cmp = node_cmp;
    return 0;
}

static void ${name}_rotate_left(struct ${name}_root *root, ${ST} *node)
{
    /*
     * 左旋示意图(对节点N进行左旋)
     *   P                 P
     *   |                 |
     *   N                 R
     *  / \\  --(左旋)-->  / \\
     * L   R             N  RR
     *    / \\           / \\
     *   LR RR         L  LR
     */

    int N_index, P_index, R_index, LR_index;
    ${name}_node_t *N_node, *P_node, *R_node, *LR_node;

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);
    P_index = N_node->parent;
    R_index = N_node->right_son;
    R_node = ${NAME}_FROM_INDEX(root, R_index);
    LR_index = R_node->left_son;

    N_node->right_son = LR_index;                   // 将LR设为N的右节点
    if (LR_index != JRBTREE_INVALID) {
        LR_node = ${NAME}_FROM_INDEX(root, LR_index);
        LR_node->parent = N_index;                  // LR不为空时将N设为LR的父节点
    }
    R_node->left_son = N_index;                     // 将N设为R的左节点
    N_node->parent = R_index;                       // 将R设为N的父节点
    R_node->parent = P_index;                       // 将P设为R的父节点
    if (P_index != JRBTREE_INVALID) {               // P不为空时将R设为P对应的左或右节点
        P_node = ${NAME}_FROM_INDEX(root, P_index);
        if (N_index == P_node->left_son)
            P_node->left_son = R_index;
        else
            P_node->right_son = R_index;
    } else {                                        // P为空时将R设为根节点
        root->index = R_index;
    }
}

static void ${name}_rotate_right(struct ${name}_root *root, ${ST} *node)
{
    /*
     * 右旋示意图(对节点N进行右旋)
     *     P                 P
     *     |                 |
     *     N                 L
     *    / \\  --(右旋)-->  / \\
     *   L   R             LL  N
     *  / \\                   / \\
     * LL RL                 RL  R
     */

    int N_index, P_index, L_index, RL_index;
    ${name}_node_t *N_node, *P_node, *L_node, *RL_node;

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);
    P_index = N_node->parent;
    L_index = N_node->left_son;
    L_node = ${NAME}_FROM_INDEX(root, L_index);
    RL_index = L_node->right_son;

    N_node->left_son = RL_index;                    // 将RL设为N的左节点
    if (RL_index != JRBTREE_INVALID) {
        RL_node = ${NAME}_FROM_INDEX(root, RL_index);
        RL_node->parent = N_index;                  // RL不为空时将N设为RL的父节点
    }
    L_node->right_son = N_index;                    // 将N设为L的右节点
    N_node->parent = L_index;                       // 将L设为N的父节点
    L_node->parent = P_index;                       // 将P设为L的父节点
    if (P_index != JRBTREE_INVALID) {               // P不为空时将L设为P对应的左或右节点
        P_node = ${NAME}_FROM_INDEX(root, P_index);
        if (N_index == P_node->right_son)
            P_node->right_son = L_index;
        else
            P_node->left_son = L_index;
    } else {                                        // P为空时将L设为根节点
        root->index = L_index;
    }
}

static void ${name}_add_fixup(struct ${name}_root *root, ${ST} *node)
{
    /*
     * 插入的节点的默认颜色是红色，插入情况分为3种：
     * 1. 被插入的节点是根节点(该节点的父节点为NULL)，违反第2条，处理方法是直接把此节点涂为黑色
     * 2. 被插入的节点的父节点是黑色，不违反任何规则，不需要做任何处理
     * 3. 被插入的节点的父节点是红色，违反第4条，处理的核心思想是将红色上移到根节点，然后，将根节点设为黑色
     * 第3条又可以分为3x2种情况，见如下处理措施
     */
    int N_index, P_index, G_index, U_index, T_index;
    ${name}_node_t *N_node, *P_node, *G_node, *U_node, *T_node;

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);

    while (1) {
        P_index = N_node->parent;
        if (P_index == JRBTREE_INVALID)
            break;
        P_node = ${NAME}_FROM_INDEX(root, P_index);
        if (P_node->color == JRBTREE_BLACK)
            break;
        G_index = P_node->parent;
        G_node = ${NAME}_FROM_INDEX(root, G_index);

        if (P_index == G_node->left_son) {          // 父节点是祖父节点的左节点
            U_index = G_node->right_son;
            if (U_index != JRBTREE_INVALID) {       // 情况3.1.1：叔节点是红色
                U_node = ${NAME}_FROM_INDEX(root, U_index);
                if (U_node->color == JRBTREE_RED) {
                    /*
                     *    bG                  rG
                     *    / \\                 / \\
                     *   rP rU  --(变色)-->  bP bU
                     *  /                   /
                     * rN                  rN
                     */
                    P_node->color = JRBTREE_BLACK;
                    U_node->color = JRBTREE_BLACK;
                    G_node->color = JRBTREE_RED;
                    N_index = G_index;
                    N_node = ${NAME}_FROM_INDEX(root, N_index);
                    continue;
                }
            }

            if (P_node->right_son == N_index) {     // 情况3.1.2：叔节点是黑色，且当前节点是右节点
                /*
                 *  bG                  bG
                 *  / \\                 / \\
                 * rP bU  --(左旋)-->  rN bU
                 *  \\                  /
                 *  rN                rP
                 */
                ${name}_rotate_left(root, root->pool->data_unsafe(root->pool, P_index));
                T_index = P_index;
                P_index = N_index;
                N_index = T_index;
                P_node = ${NAME}_FROM_INDEX(root, P_index);
                N_node = ${NAME}_FROM_INDEX(root, N_index);
            }

            /*
             *    bG                      bP
             *    / \\                     / \\
             *   rP bU  --(变色右旋)-->  rN rG
             *  /                            \\
             * rN                            bU
             */
            P_node->color = JRBTREE_BLACK;          // 情况3.1.3：叔节点是黑色，且当前节点是左节点
            G_node->color = JRBTREE_RED;
            ${name}_rotate_right(root, root->pool->data_unsafe(root->pool, G_index));

        } else {                                    // 父节点是祖父节点的右节点

            U_index = G_node->left_son;
            if (U_index != JRBTREE_INVALID) {       // 情况3.2.1：叔节点是红色
                U_node = ${NAME}_FROM_INDEX(root, U_index);
                if (U_node->color == JRBTREE_RED) {
                    /*
                     *  bG                  rG
                     *  / \\                 / \\
                     * rU rP  --(变色)-->  bU bP
                     *     \\                   \\
                     *     rN                  rN
                     */
                    P_node->color = JRBTREE_BLACK;
                    U_node->color = JRBTREE_BLACK;
                    G_node->color = JRBTREE_RED;
                    N_index = G_index;
                    N_node = ${NAME}_FROM_INDEX(root, N_index);
                    continue;
                }
            }

            if (P_node->left_son == N_index) {      // 情况3.2.2：叔节点是黑色，且当前节点是左节点
                /*
                 *  bG                  bG
                 *  / \\                 / \\
                 * bU rP  --(右旋)-->  bU rN
                 *    /                    \\
                 *   rN                    rP
                 */
                ${name}_rotate_right(root, root->pool->data_unsafe(root->pool, P_index));
                T_index = P_index;
                P_index = N_index;
                N_index = T_index;
                P_node = ${NAME}_FROM_INDEX(root, P_index);
                N_node = ${NAME}_FROM_INDEX(root, N_index);
            }

            /*
             *    bG                      bP
             *    / \\                     / \\
             *   bU rP  --(变色左旋)-->  rG rN
             *       \\                   /
             *       rN                 bU
             */
            P_node->color = JRBTREE_BLACK;          // 情况3.2.3：叔节点是黑色，且当前节点是右节点
            G_node->color = JRBTREE_RED;
            ${name}_rotate_left(root, root->pool->data_unsafe(root->pool, G_index));
        }
    }

    T_node = ${NAME}_FROM_INDEX(root, root->index);
    T_node->color = JRBTREE_BLACK;                  // 将根节点涂为黑色
}

int ${name}_add(struct ${name}_root *root, ${ST} *node)
{
    int result = 0;
    int N_index, P_index = JRBTREE_INVALID, H_index = root->index;
    ${name}_node_t *N_node, *P_node;
    ${ST} *parent;

    while (H_index != JRBTREE_INVALID) {
        P_index = H_index;
        parent = root->pool->data_unsafe(root->pool, P_index);
        P_node = ${NAME}_FROM_DATA(parent);

        result = root->node_cmp(parent, node);
        if (result > 0)
            H_index = P_node->left_son;
        else if (result < 0)
            H_index = P_node->right_son;
        else
            return -1;
    }

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);
    N_node->parent = P_index;                       // 设置新增节点的父节点
    N_node->color = JRBTREE_RED;                    // 新节点的默认颜色为红色
    N_node->left_son = JRBTREE_INVALID;
    N_node->right_son = JRBTREE_INVALID;
    if (P_index != JRBTREE_INVALID) {               // 设置父节点的左或右节点为新节点
        if (result > 0)
            P_node->left_son = N_index;
        else
            P_node->right_son = N_index;
    } else {
        root->index = N_index;
    }
    ${name}_add_fixup(root, node);
    ++root->num;

    return 0;
}

static void ${name}_del_fixup(struct ${name}_root *root, ${ST} *node, ${ST} *parent)
{
    /*
     * 如果删除的是黑节点，需要进行平衡操作，处理的核心思想是将黑色上移到x，分3种情况：
     * 1. x是红节点，处理方法是直接把x涂为黑色
     * 2. x是黑节点且为根节点，不需要做任何处理
     * 3. x是黑节点且不为根节点，又分为下面2x4情况处理，直到满足1或2再处理
     */
    int N_index, P_index, B_index, L_index, R_index;
    ${name}_node_t *N_node, *P_node, *B_node, *L_node, *R_node;

    if (node) {
        N_index = root->pool->index_unsafe(root->pool, node);
        N_node = ${NAME}_FROM_DATA(node);
    } else {
        N_index = JRBTREE_INVALID;
        N_node = NULL;
    }
    if (parent) {
        P_index = root->pool->index_unsafe(root->pool, parent);
        P_node = ${NAME}_FROM_DATA(parent);
    } else {
        P_index = JRBTREE_INVALID;
        P_node = NULL;
    }

    while ((N_index == JRBTREE_INVALID || N_node->color == JRBTREE_BLACK) && N_index != root->index) {
        if (P_node->left_son == N_index) {          // 测试节点是父节点的左节点
            B_index = P_node->right_son;
            B_node = ${NAME}_FROM_INDEX(root, B_index);
            if (B_node->color == JRBTREE_RED) {
                /*
                 * 情况3.1.1：兄弟节点是红色
                 *    bP                      bB
                 *    / \\                     / \\
                 *   bN rB  --(变色左旋)-->  rP bR
                 *      / \\                  / \\
                 *     bL bR                bN bL
                 */
                B_node->color = JRBTREE_BLACK;
                P_node->color = JRBTREE_RED;
                ${name}_rotate_left(root, root->pool->data_unsafe(root->pool, P_index));
                B_index = P_node->right_son;
                B_node = ${NAME}_FROM_INDEX(root, B_index);
            }

            L_index = B_node->left_son;
            if (L_index != JRBTREE_INVALID)
                L_node = ${NAME}_FROM_INDEX(root, L_index);
            else
                L_node = NULL;
            R_index = B_node->right_son;
            if (R_index != JRBTREE_INVALID)
                R_node = ${NAME}_FROM_INDEX(root, R_index);
            else
                R_node = NULL;

            if ((L_index == JRBTREE_INVALID || L_node->color == JRBTREE_BLACK) &&
                (R_index == JRBTREE_INVALID || R_node->color == JRBTREE_BLACK)) {
                /*
                 * 情况3.1.2：兄弟节点为黑色，且兄弟节点的子节点都为黑色
                 *    ?P                  ?P
                 *    / \\                 / \\
                 *   bN bB  --(变色)-->  bN rB
                 *      / \\                 / \\
                 *     bL bR               bL bR
                 */
                B_node->color = JRBTREE_RED;
                N_index = P_index;
                N_node = ${NAME}_FROM_INDEX(root, N_index);
                P_index = N_node->parent;
                if (P_index != JRBTREE_INVALID)
                    P_node = ${NAME}_FROM_INDEX(root, P_index);
                else
                    P_node = NULL;
            } else {
                if (R_index == JRBTREE_INVALID || R_node->color == JRBTREE_BLACK) {
                    /*
                     * 情况3.1.3：兄弟节点为黑色，且兄弟节点的左节点为红色右节点为黑色
                     *    ?P                      ?P
                     *    / \\                     / \\
                     *   bN bB  --(变色右旋)-->  bN bL
                     *      / \\                     / \\
                     *     rL bR                   bX rB
                     *    / \\                        / \\
                     *   bX bY                      bY bR
                     */
                    L_node->color = JRBTREE_BLACK;
                    B_node->color = JRBTREE_RED;
                    ${name}_rotate_right(root, root->pool->data_unsafe(root->pool, B_index));
                    B_index = P_node->right_son;
                    B_node = ${NAME}_FROM_INDEX(root, B_index);
                    R_index = B_node->right_son;
                    R_node = ${NAME}_FROM_INDEX(root, R_index);
                }

                /*
                 * 情况3.1.4：兄弟节点为黑色，且兄弟节点的左节点为任意色右节点为红色
                 *    ?P                       ?B
                 *    / \\                     /   \\
                 *   bN bB  --(变色左旋)-->  bP   bR
                 *      / \\                 / \\   / \\
                 *     ?L rR               bN ?L bX bY
                 *        / \\
                 *       bX bY
                 */
                B_node->color = P_node->color;
                P_node->color = JRBTREE_BLACK;
                R_node->color = JRBTREE_BLACK;
                ${name}_rotate_left(root, root->pool->data_unsafe(root->pool, P_index));
                N_index = root->index;
                if (N_index != JRBTREE_INVALID)
                    N_node = ${NAME}_FROM_INDEX(root, N_index);
                else
                    N_node = NULL;
                break;
            }
        } else {                                    // 测试节点是父节点的右节点
            B_index = P_node->left_son;
            B_node = ${NAME}_FROM_INDEX(root, B_index);
            if (B_node->color == JRBTREE_RED) {
                /*
                 * 情况3.2.1：兄弟节点是红色
                 *    bP                      bB
                 *    / \\                     / \\
                 *   rB bN  --(变色右旋)-->  bL rP
                 *  / \\                         / \\
                 * bL bR                       bR bN
                 */
                B_node->color = JRBTREE_BLACK;
                P_node->color = JRBTREE_RED;
                ${name}_rotate_right(root, root->pool->data_unsafe(root->pool, P_index));
                B_index = P_node->left_son;
                B_node = ${NAME}_FROM_INDEX(root, B_index);
            }

            L_index = B_node->left_son;
            if (L_index != JRBTREE_INVALID)
                L_node = ${NAME}_FROM_INDEX(root, L_index);
            else
                L_node = NULL;
            R_index = B_node->right_son;
            if (R_index != JRBTREE_INVALID)
                R_node = ${NAME}_FROM_INDEX(root, R_index);
            else
                R_node = NULL;

            if ((L_index == JRBTREE_INVALID || L_node->color == JRBTREE_BLACK) &&
                (R_index == JRBTREE_INVALID || R_node->color == JRBTREE_BLACK)) {
                /*
                 * 情况3.2.2：兄弟节点为黑色，且兄弟节点的子节点都为黑色
                 *    ?P                  ?P
                 *    / \\                 / \\
                 *   bB bN  --(变色)-->  bB bN
                 *  / \\                 / \\
                 * bL bR               bL bR
                 */
                B_node->color = JRBTREE_RED;
                N_index = P_index;
                N_node = ${NAME}_FROM_INDEX(root, N_index);
                P_index = N_node->parent;
                if (P_index != JRBTREE_INVALID)
                    P_node = ${NAME}_FROM_INDEX(root, P_index);
                else
                    P_node = NULL;
            } else {
                if (L_index == JRBTREE_INVALID || L_node->color == JRBTREE_BLACK) {
                    /*
                     * 情况3.2.3：兄弟节点为黑色，且兄弟节点的左节点为黑色右节点为红色
                     *         ?P                      ?P
                     *         / \\                     / \\
                     *        bB bN  --(变色左旋)-->  bR bN
                     *       / \\                     / \\
                     *      bL rR                   rB bY
                     *         / \\                  / \\
                     *        bX bY                bL bX
                     */

                    R_node->color = JRBTREE_BLACK;
                    B_node->color = JRBTREE_RED;
                    ${name}_rotate_left(root, root->pool->data_unsafe(root->pool, B_index));
                    B_index = P_node->left_son;
                    B_node = ${NAME}_FROM_INDEX(root, B_index);
                    L_index = B_node->left_son;
                    L_node = ${NAME}_FROM_INDEX(root, L_index);
                }

                /*
                 * 情况3.2.4：兄弟节点为黑色，且兄弟节点的左节点为红色右节点为任意色
                 *         ?P                       ?B
                 *         / \\                     /   \\
                 *        bB bN  --(变色右旋)-->  bL   bP
                 *       / \\                     / \\   / \\
                 *      rL ?R                   bX bY ?R bN
                 *     / \\
                 *    bX bY
                 */

                B_node->color = P_node->color;
                P_node->color = JRBTREE_BLACK;
                L_node->color = JRBTREE_BLACK;
                ${name}_rotate_right(root, root->pool->data_unsafe(root->pool, P_index));
                N_index = root->index;
                if (N_index != JRBTREE_INVALID)
                    N_node = ${NAME}_FROM_INDEX(root, N_index);
                else
                    N_node = NULL;
                break;
            }
        }
    }
    if (N_node)
        N_node->color = JRBTREE_BLACK;
}

void ${name}_del(struct ${name}_root *root, ${ST} *node)
{
    /*
     * 删除情况分为3种
     * 1. 被删节点没有子节点，处理方法是直接将该节点删除
     * 2. 被删节点只有1个子节点，处理方法是直接将该节点删除，并用该节点的子节点顶替它的位置
     * 3. 被删节点有2个子节点，处理方法是替换该节点为他右边树最左边的节点
     * 如果删除的是黑节点，需要进行平衡操作
     */
    int color;
    int N_index, P_index, L_index, R_index, C_index, O_index;   // C:child, O:old
    ${name}_node_t *N_node, *P_node, *L_node, *R_node, *C_node, *O_node;

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);

    if (N_node->left_son == JRBTREE_INVALID) {
        C_index = N_node->right_son;
    } else if (N_node->right_son == JRBTREE_INVALID) {
        C_index = N_node->left_son;
    } else {
        O_index = N_index;
        O_node = N_node;

        /*
         * 检索了一级就满足
         *        P                     P
         *        |                     |
         *        N                     B
         *     /     \\  --(变换)-->  /     \\
         *    A       B             A       c
         *   / \\       \\           / \\
         *  a   b       c         a   b
         *
         * 检索了多级才满足
         *        P                     P
         *        |                     |
         *        N                     c
         *     /     \\  --(变换)-->  /     \\
         *    A       B             A       B
         *   / \\     / \\           / \\     / \\
         *  a   b   c   d         a   b   e   d
         *           \\
         *            e
         */
        N_index = N_node->right_son;
        N_node = ${NAME}_FROM_INDEX(root, N_index);
        while (1) {
            L_index = N_node->left_son;
            if (L_index == JRBTREE_INVALID)
                break;
            N_index = L_index;
            N_node = ${NAME}_FROM_INDEX(root, N_index);
        }

        P_index = O_node->parent;
        if (P_index != JRBTREE_INVALID) {
            P_node = ${NAME}_FROM_INDEX(root, P_index);
            if (P_node->left_son == O_index)
                P_node->left_son = N_index;
            else
                P_node->right_son = N_index;
        } else {
            root->index = N_index;
        }

        C_index = N_node->right_son;
        P_index = N_node->parent;
        color = N_node->color;

        if (P_index == O_index) {
            P_index = N_index;
        } else {
            if (C_index != JRBTREE_INVALID) {
                C_node = ${NAME}_FROM_INDEX(root, C_index);
                C_node->parent = P_index;
            }
            P_node = ${NAME}_FROM_INDEX(root, P_index);
            P_node->left_son = C_index;

            N_node->right_son = O_node->right_son;
            R_index = O_node->right_son;
            R_node = ${NAME}_FROM_INDEX(root, R_index);
            R_node->parent = N_index;
        }

        N_node->parent = O_node->parent;
        N_node->color = O_node->color;
        N_node->left_son = O_node->left_son;

        L_index = O_node->left_son;
        L_node = ${NAME}_FROM_INDEX(root, L_index);
        L_node->parent = N_index;

        goto color;
    }

    /*
     * 只有一个子节点或没有子节点
     *  P               P
     *  |               |
     *  N  --(变换)-->  A
     *  |
     *  A
     */
    P_index = N_node->parent;
    color = N_node->color;

    if (C_index != JRBTREE_INVALID) {
        C_node = ${NAME}_FROM_INDEX(root, C_index);
        C_node->parent = P_index;
    }

    if (P_index != JRBTREE_INVALID) {
        P_node = ${NAME}_FROM_INDEX(root, P_index);
        if (P_node->left_son == N_index)
            P_node->left_son = C_index;
        else
            P_node->right_son = C_index;
    } else {
        root->index = C_index;
    }

color:
    if (color == JRBTREE_BLACK)
        ${name}_del_fixup(root,
            C_index == JRBTREE_INVALID ? NULL : root->pool->data_unsafe(root->pool, C_index),
            P_index == JRBTREE_INVALID ? NULL : root->pool->data_unsafe(root->pool, P_index));
    --root->num;
}

void ${name}_replace(struct ${name}_root *root, ${ST} *old, ${ST} *node)
{
    int N_index, P_index, L_index, R_index, O_index;   //O:old
    ${name}_node_t *N_node, *P_node, *L_node, *R_node, *O_node;

    N_index = root->pool->index_unsafe(root->pool, node);
    N_node = ${NAME}_FROM_DATA(node);
    O_index = root->pool->index_unsafe(root->pool, old);
    O_node = ${NAME}_FROM_DATA(old);

    P_index = O_node->parent;
    if (P_index != JRBTREE_INVALID) {
        P_node = ${NAME}_FROM_INDEX(root, P_index);
        if (O_index == P_node->left_son)
            P_node->left_son = N_index;
        else
            P_node->right_son = N_index;
    } else {
        root->index = N_index;
    }

    L_index = O_node->left_son;
    if (L_index != JRBTREE_INVALID) {
        L_node = ${NAME}_FROM_INDEX(root, L_index);
        L_node->parent = N_index;
    }
    R_index = O_node->right_son;
    if (R_index != JRBTREE_INVALID) {
        R_node = ${NAME}_FROM_INDEX(root, R_index);
        R_node->parent = N_index;
    }
    *N_node = *O_node;
}

${ST} *${name}_search(struct ${name}_root *root, const void *key)
{
    int result = 0;
    int index = root->index;
    ${name}_node_t *inode;
    ${ST} *data;

    while (index != JRBTREE_INVALID) {
        inode = ${NAME}_FROM_INDEX(root, index);
        data = root->pool->data_unsafe(root->pool, index);
        result = root->key_cmp(data, key);
        if (result > 0)
            index = inode->left_son;
        else if (result < 0)
            index = inode->right_son;
        else
            return data;
    }
    return NULL;
}

${ST} *${name}_first(const struct ${name}_root *root)
{
    int index = root->index;
    ${name}_node_t *inode;

    if (index == JRBTREE_INVALID)
        return NULL;

    inode = ${NAME}_FROM_INDEX(root, index);
    while (inode->left_son != JRBTREE_INVALID) {
        index = inode->left_son;
        inode = ${NAME}_FROM_INDEX(root, index);
    }

    return root->pool->data_unsafe(root->pool, index);
}

${ST} *${name}_last(const struct ${name}_root *root)
{
    int index = root->index;
    ${name}_node_t *inode;

    if (index == JRBTREE_INVALID)
        return NULL;

    inode = ${NAME}_FROM_INDEX(root, index);
    while (inode->right_son != JRBTREE_INVALID) {
        index = inode->right_son;
        inode = ${NAME}_FROM_INDEX(root, index);
    }

    return root->pool->data_unsafe(root->pool, index);
}

${ST} *${name}_next(const struct ${name}_root *root, ${ST} *node)
{
    int index, pindex;
    ${name}_node_t *inode, *pnode;

    index = root->pool->index_unsafe(root->pool, node);
    inode = ${NAME}_FROM_DATA(node);
    if (inode->right_son != JRBTREE_INVALID) {
        index = inode->right_son;
        inode = ${NAME}_FROM_INDEX(root, index);
        while (inode->left_son != JRBTREE_INVALID) {
            index = inode->left_son;
            inode = ${NAME}_FROM_INDEX(root, index);
        }
        return root->pool->data_unsafe(root->pool, index);
    }

    while (1) {
        pindex = inode->parent;
        if (pindex == JRBTREE_INVALID)
            break;
        pnode = ${NAME}_FROM_INDEX(root, pindex);
        if (index != pnode->right_son)
            break;
        index = pindex;
        inode = ${NAME}_FROM_INDEX(root, index);
    }

    return pindex == JRBTREE_INVALID ? NULL : root->pool->data_unsafe(root->pool, pindex);
}

${ST} *${name}_prev(const struct ${name}_root *root, ${ST} *node)
{
    int index, pindex;
    ${name}_node_t *inode, *pnode;

    index = root->pool->index_unsafe(root->pool, node);
    inode = ${NAME}_FROM_DATA(node);
    if (inode->left_son != JRBTREE_INVALID) {
        index = inode->left_son;
        inode = ${NAME}_FROM_INDEX(root, index);
        while (inode->right_son != JRBTREE_INVALID) {
            index = inode->right_son;
            inode = ${NAME}_FROM_INDEX(root, index);
        }
        return root->pool->data_unsafe(root->pool, index);
    }

    while (1) {
        pindex = inode->parent;
        if (pindex == JRBTREE_INVALID)
            break;
        pnode = ${NAME}_FROM_INDEX(root, pindex);
        if (index != pnode->left_son)
            break;
        index = pindex;
        inode = ${NAME}_FROM_INDEX(root, index);
    }

    return pindex == JRBTREE_INVALID ? NULL : root->pool->data_unsafe(root->pool, pindex);
}
EOF
