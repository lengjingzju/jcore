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

if [ -z "${name}" ] || [ -z "${T}" ] || [ -z "${ST}" ] || [ "${name}" = "-h" ]; then
    echo "Usage: $0 <name> <T> <ST> <compact>"
    echo "       name:      生成的头文件或源文件名称、管理结构类型名和函数名前缀"
    echo "       T:         变量类型名，该类型下自动创建，包含一个链表节点成员node和一个ST类型的数据成员"
    echo "       ST:        子变量类型名"
    echo "       compact:   省内存的可选选项，设置为y时使用unsigned short记录索引，更省内存，"
    echo "                  但节点数限制为了65535；否则使用int，支持节点数更多，但节点内存翻倍"
    exit 1
fi

index_t="int"
node_t="struct jplist_long"
if [ "${compact}" = "y" ]; then
    index_t="unsigned short"
    node_t="struct jplist_short"
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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   双向循环链表挂载节点(紧凑型)
 * @note    分别指向前一个数据成员的序号和后一个数据成员的序号
 */
#ifndef JPLIST_SHORT
#define JPLIST_SHORT 1
struct jplist_short {
    unsigned short next, prev;
};
#endif

/**
 * @brief   双向循环链表挂载节点(大容量型)
 * @note    分别指向前一个数据成员的序号和后一个数据成员的序号
 */
#ifndef JPLIST_LONG
#define JPLIST_LONG 1
struct jplist_long {
    int next, prev;
};
#endif

typedef ${node_t} ${name}_node_t;

/**
 * @brief   数据节点
 */
${T} {
    ${name}_node_t node;    // 链表节点
    ${ST} data;             // 数据内容
};

/**
 * @brief   从节点的链表成员指针获取数据节点的指针
 * @param   ptr [IN] 节点的数据成员指针
 * @return  返回数据节点的指针
 * @note    无
 */
#define ${name}_node_entry(ptr)         ((${T} *)(ptr))

/**
 * @brief   从节点的数据成员指针获取数据节点的指针
 * @param   ptr [IN] 节点的数据成员指针
 * @return  返回数据节点的指针
 * @note    无
 */
#define ${name}_data_entry(ptr)         ((${T} *)((char *)(ptr)-(unsigned long)(&((${T} *)0)->data)))

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
 * @brief   双向循环链表管理结构
 */
struct ${name}_root {
    int num;                    // 节点总数
    int index;                  // 头节点的序号
    struct ${name}_pool *pool;  // 内存池的指针，根据序号从内存池中获取数据指针
};

/**
 * @brief   通过序号获取链表节点的指针
 * @param   root [IN] 双向循环链表管理结构
 * @param   index [IN] 数据节点在内存池中的序号
 * @return  返回链表节点的指针
 * @note    无
 */
#define ${NAME}_FROM_INDEX(root, index)  (&(root)->pool->begin[index].node)

/**
 * @brief   通过数据节点的指针获取链表节点的指针
 * @param   ptr [IN] 数据节点的指针
 * @return  返回链表节点的指针
 * @note    无
 */
#define ${NAME}_FROM_DATA(ptr)          ((${name}_node_t *)((char *)(ptr)-(unsigned long)(&((${T} *)0)->data)))

/**
 * @brief   初始化双向链表管理结构
 * @param   root [IN] 双向链表管理结构
 * @param   pool [IN] 内存池的指针
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
static inline int ${name}_init(struct ${name}_root *root, struct ${name}_pool *pool)
{
    int index = 0;
    ${name}_node_t *head = NULL;

    if (!root || !pool || (index = pool->alloci(pool)) < 0)
        return -1;

    root->index = index;
    root->pool = pool;
    root->num = 0;

    head = ${NAME}_FROM_INDEX(root, root->index);
    head->next = root->index;
    head->prev = root->index;

    return 0;
}

/**
 * @brief   反初始化双向链表管理结构
 * @param   root [IN] 双向链表管理结构
 * @return  无返回值
 * @note    无
 */
static inline void ${name}_uninit(struct ${name}_root *root)
{
    if (root && root->pool) {
        root->pool->freei(root->pool, root->index);
        root->pool = NULL;
        root->num = 0;
    }
}

/**
 * @brief   判断双向循环链表是否为空
 * @param   root [IN] 双向链表管理结构
 * @return  链表为空时返回真，否则返回假
 * @note    无
 */
static inline int ${name}_empty(struct ${name}_root *root)
{
    return root->num ? 0 : 1;
}

/**
 * @brief   获取双向循环链表中数据节点的数量
 * @param   root [IN] 双向链表管理结构
 * @return  返回数据节点的数量
 * @note    无
 */
static inline int ${name}_size(struct ${name}_root *root)
{
    return root->num;
}

/**
 * @brief   遍历双向循环链表的循环头
 * @param   pos [OUT] 被赋值当前数据节点的指针
 * @param   root [IN] 双向链表管理结构
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，不能在循环体内部进行添加删除节点操作
 */
#define ${name}_for_each_entry(pos, root)                                                           \\
for (pos = (root)->pool->data_unsafe((root)->pool, ${NAME}_FROM_INDEX(root, (root)->index)->next);  \\
     (root)->pool->index_unsafe((root)->pool, pos) != (root)->index;                                \\
     pos = (root)->pool->data_unsafe((root)->pool, ${NAME}_FROM_DATA(pos)->next))

/**
 * @brief   安全遍历双向循环链表的循环头
 * @param   pos [OUT] 给他赋值当前数据点的指针
 * @param   n [OUT] 给他赋值后一数据节点的指针
 * @param   root [IN] 双向链表管理结构
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
#define ${name}_for_each_entry_safe(pos, n, root)                                                   \\
for (pos = (root)->pool->data_unsafe((root)->pool, ${NAME}_FROM_INDEX(root, (root)->index)->next),  \\
     n = (root)->pool->data_unsafe((root)->pool, ${NAME}_FROM_DATA(pos)->next);                     \\
     (root)->pool->index_unsafe((root)->pool, pos) != (root)->index;                                \\
     pos = n, n = (root)->pool->data_unsafe((root)->pool, ${NAME}_FROM_DATA(n)->next))

/**
 * @brief   加入新节点到指定节点后面
 * @param   root [IN] 双向链表管理结构
 * @param   _new [INOUT] 要加入的数据节点
 * @param   list [INOUT] 参考位置的数据节点
 * @return  无返回值
 * @note    无
 */
static inline void ${name}_add(struct ${name}_root *root, ${ST} *_new, ${ST} *list)
{
    ${name}_node_t *_new_node = ${NAME}_FROM_DATA(_new);
    ${index_t} _new_index = root->pool->index_unsafe(root->pool, _new);
    ${name}_node_t *prev_node = ${NAME}_FROM_DATA(list);
    ${index_t} prev_index = root->pool->index_unsafe(root->pool, list);

    ${index_t} next_index = prev_node->next;
    ${name}_node_t *next_node = ${NAME}_FROM_INDEX(root, next_index);

    next_node->prev = _new_index;
    _new_node->next = next_index;
    _new_node->prev = prev_index;
    prev_node->next = _new_index;
    ++root->num;
}

/**
 * @brief   加入新节点到指定节点前面
 * @param   root [IN] 双向链表管理结构
 * @param   _new [INOUT] 要加入的数据节点
 * @param   list [INOUT] 参考位置的数据节点
 * @return  无返回值
 * @note    无
 */
static inline void ${name}_add_tail(struct ${name}_root *root, ${ST} *_new, ${ST} *list)
{
    ${name}_node_t *_new_node = ${NAME}_FROM_DATA(_new);
    ${index_t} _new_index = root->pool->index_unsafe(root->pool, _new);
    ${name}_node_t *next_node = ${NAME}_FROM_DATA(list);
    ${index_t} next_index = root->pool->index_unsafe(root->pool, list);
    ${index_t} prev_index = next_node->prev;
    ${name}_node_t *prev_node = ${NAME}_FROM_INDEX(root, prev_index);

    next_node->prev = _new_index;
    _new_node->next = next_index;
    _new_node->prev = prev_index;
    prev_node->next = _new_index;
    ++root->num;
}

/**
 * @brief   加入新节点到链表的最后面
 * @param   root [IN] 双向链表管理结构
 * @param   _new [INOUT] 要加入的数据节点
 * @return  无返回值
 * @note    无
 */
static inline void ${name}_add_last(struct ${name}_root *root, ${ST} *_new)
{
    ${name}_add_tail(root, _new, root->pool->data_unsafe(root->pool, root->index));
}

/**
 * @brief   加入新节点到链表的最前面
 * @param   root [IN] 双向链表管理结构
 * @param   _new [INOUT] 要加入的数据节点
 * @return  无返回值
 * @note    无
 */
static inline void ${name}_add_first(struct ${name}_root *root, ${ST} *_new)
{
    ${name}_add(root, _new, root->pool->data_unsafe(root->pool, root->index));
}

/**
 * @brief   从链表中删除指定节点
 * @param   root [IN] 双向链表管理结构
 * @param   _del [IN] 要删除的节点
 * @return  无返回值
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
static inline void ${name}_del(struct ${name}_root *root, ${ST} *_del)
{
    ${name}_node_t *_del_node = ${NAME}_FROM_DATA(_del);

    ${index_t} prev_index = _del_node->prev;
    ${name}_node_t *prev_node = ${NAME}_FROM_INDEX(root, prev_index);

    ${index_t} next_index = _del_node->next;
    ${name}_node_t *next_node = ${NAME}_FROM_INDEX(root, next_index);

    next_node->prev = prev_index;
    prev_node->next = next_index;
    --root->num;
}

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
#include <stdint.h>
#include "jheap.h"
#include "${name}.h"

#define jmalloc     jheap_malloc
#define jrealloc    jheap_realloc
#define jfree       jheap_free

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
EOF
