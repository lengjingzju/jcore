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
    echo "       T:         变量类型名，该类型下自动创建，包含一个链表节点成员node和一个ST类型的数据成员"
    echo "       ST:        子变量类型名"
    echo "       compact:   省内存的可选选项，设置为y时使用双16bits记录桶索引，更省内存，但最大移位数限制为了15；"
    echo "                  否则使用双32bits记录桶索引，最大移位数限制为了31，但节点内存翻倍"
    exit 1
fi

shiftv=31
bucket_t="struct jphashmap_long"
if [ "${compact}" = "y" ]; then
    shiftv=15
    bucket_t="struct jphashmap_short"
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
 * @brief   计算哈希值
 */
#ifndef JHASHMAP_CODE_CALC
#define JHASHMAP_CODE_CALC(hash, array, size) do { unsigned int i; hash = 0; for (i = 0; i < (size); ++i) hash = (hash << 5) - hash + (array)[i]; } while (0)
#endif

/**
 * @brief   哈希表的节点结构
 */
#ifndef JPHASHMAP_NODE
#define JPHASHMAP_NODE 1
struct jphashmap_node {
    int next;               // 指向单链表下一个节点
};
#endif

/**
 * @brief   数据节点
 */
${T} {
    struct jphashmap_node node; // 树节点
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
 * @brief   哈希表的桶结构(紧凑型)
 */
struct jphashmap_short {
    unsigned short next;    // 指向前一个桶序号
    unsigned short prev;    // 指向后一个桶序号
    struct jphashmap_node head; // 挂载节点数据的单向链表
};

/**
 * @brief   哈希表的桶结构(大容量型)
 * @note    限制最大移位为15，则最大桶数为 1<<15
 */
struct jphashmap_long {
    unsigned int next;      // 指向前一个桶序号
    unsigned int prev;      // 指向后一个桶序号
    struct jphashmap_node head; // 挂载节点数据的单向链表
};

/**
 * @brief   桶结构类型名
 */
typedef ${bucket_t} ${name}_bucket_t;

/**
 * @brief   哈希表的管理结构
 * @note    首次使用哈希表前一定要设置回调函数；
 *          哈希表的节点为单链表节点struct jphashmap_node ；
 *          buckets多分配一个节点，用于存储有值桶的入口
 */
struct ${name}_root {
    unsigned int num;                   // 节点的总数
    unsigned int bucket_shift;          // 桶总数 1<<bucket_shift
    unsigned int bucket_num;            // 桶总数
    unsigned int bucket_mask;           // 桶总数-1，获取桶序号的hash值
    ${name}_bucket_t *buckets;          // 桶数组
    struct ${name}_pool *pool;          // 内存池的指针，根据序号从内存池中获取数据指针
    unsigned int (*key_hash)(const void *key);      // 通过key获取hash值
    unsigned int (*node_hash)(${ST} *node);         // 通过节点获取hash值
    int (*key_cmp)(${ST} *node, const void *key);   // 节点和key的比较函数，键值相等返回0
    int (*node_cmp)(${ST} *a, ${ST} *b);            // 节点之间的比较函数，键值相等返回0
};

/**
 * @brief   哈希表初始化
 * @param   root [INOUT] 哈希表句柄
 * @param   bucket_shift [IN] 桶的总数 1<<bucket_shift
 * @param   pool [IN] 内存池的指针
 * @return  成功返回0; 失败返回-1
 * @note    首次使用哈希表前一定要设置回调函数
 */
int ${name}_init(struct ${name}_root *root, unsigned int bucket_shift, struct ${name}_pool *pool);

/**
 * @brief   哈希表容量调整
 * @param   root [INOUT] 哈希表句柄
 * @param   bucket_shift [IN] 新的桶的总数 1<<bucket_shift
 * @return  成功返回0; 失败返回-1不改变
 * @note    用于扩容缩容
 */
int ${name}_resize(struct ${name}_root *root, unsigned int bucket_shift);

/**
 * @brief   哈希表反初始化
 * @param   root [INOUT] 哈希表句柄
 * @return  无返回值
 * @note    无
 */
void ${name}_uninit(struct ${name}_root *root);

/**
 * @brief   哈希表获取指定key的节点
 * @param   root [IN] 哈希表句柄
 * @param   key [IN] 要查找的key
 * @param   prev [OUT] 找到节点的前一个节点，可以为空
 * @return  成功返回有效指针，失败返回空指针
 * @note    无
 */
${ST} *${name}_find(struct ${name}_root *root, const void *key, ${ST} **prev);

/**
 * @brief   哈希表获取和指定node相同key的节点
 * @param   root [IN] 哈希表句柄
 * @param   key [IN] 要查找的node
 * @param   prev [OUT] 找到节点的前一个节点，可以为空
 * @return  成功返回有效指针，失败返回空指针
 * @note    无
 */
${ST} *${name}_find_node(struct ${name}_root *root, ${ST} *node, ${ST} **prev);

/**
 * @brief   哈希表增加节点
 * @param   root [IN] 哈希表句柄
 * @param   node [IN] 增加的节点
 * @return  无返回值
 * @note    不检查key重复
 */
void ${name}_fast_add(struct ${name}_root *root, ${ST} *node);

/**
 * @brief   哈希表增加节点
 * @param   root [IN] 哈希表句柄
 * @param   node [IN] 增加的节点
 * @return  有同key值的节点替换原节点并返回原节点；否则直接增加节点返回NULL
 * @note    检查key重复
 */
${ST} *${name}_add(struct ${name}_root *root, ${ST} *node);

/**
 * @brief   哈希表删除节点
 * @param   root [IN] 哈希表句柄
 * @param   node [IN] 删除的节点
 * @param   prev [IN] 删除节点的前一个节点，可以为空
 * @return  成功返回0；找不到节点时失败返回-1
 * @note    无
 */
int ${name}_del(struct ${name}_root *root, ${ST} *node, ${ST} *prev);

/**
 * @brief   哈希表遍历回调函数"unsigned long (*cb)(struct jhashmap *node)"的返回值
 * @note    没有定义JHASHMAP_ADD，JHASHMAP_ADD返回新节点数据成员的指针"(unsigned long)(ptr)"；
 *          JHASHMAP_ADD时需要保证桶序号相同；此时外部也不需要调用${name}_add增加
 */
#ifndef JHASHMAP_RET
#define JHASHMAP_RET    1
#define JHASHMAP_NEXT   0   // 表示继续循环
#define JHASHMAP_STOP   1   // 表示退出循环
#define JHASHMAP_DEL    2   // 表示删除cur节点，此时外部不用调用jhashmap_del删除
#endif

/**
 * @brief   哈希表遍历节点
 * @param   root [IN] 哈希表句柄
 * @param   cb [IN] 回调函数
 * @return  无返回值
 * @note    注意prev节点可能不是有效的节点，而是head
 */
void ${name}_loop(struct ${name}_root *root, unsigned long (*cb)(${ST} *node));

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
#include <stdint.h>
#include "jheap.h"
#include "${name}.h"

#define jmalloc     jheap_malloc
#define jrealloc    jheap_realloc
#define jfree       jheap_free

/* 最大移位，使用jphashmap_short为15，使用jphashmap_long时为31 */
#define JHASHMAP_MAX_SHIFT              ${shiftv}
#define JHASHMAP_INVALID                -1

#define ${name}_node_entry(ptr)         ((${T} *)(ptr))
#define ${name}_data_entry(ptr)         ((${T} *)((char *)(ptr)-(unsigned long)(&((${T} *)0)->data)))
#define ${NAME}_FROM_INDEX(root, index) (&(root)->pool->begin[index].node)
#define ${NAME}_FROM_DATA(ptr)          ((struct jphashmap_node *)((char *)(ptr)-(unsigned long)(&((${T} *)0)->data)))

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

static inline void _add_bucket(unsigned int _new, unsigned int prev, ${name}_bucket_t *buckets)
{
    ${name}_bucket_t *_newd = &buckets[_new];
    ${name}_bucket_t *prevd = &buckets[prev];
    ${name}_bucket_t *nextd = &buckets[prevd->next];
    nextd->prev = _new;
    _newd->next = prevd->next;
    _newd->prev = prev;
    prevd->next = _new;
}

static inline void _del_bucket(unsigned int _del, ${name}_bucket_t *buckets)
{
   ${name}_bucket_t *_deld = &buckets[_del];
   ${name}_bucket_t *prevd = &buckets[_deld->prev];
   ${name}_bucket_t *nextd = &buckets[_deld->next];
   nextd->prev = _deld->prev;
   prevd->next = _deld->next;
   _deld->prev = 0;
   _deld->next = 0;
}

int ${name}_init(struct ${name}_root *root, unsigned int bucket_shift, struct ${name}_pool *pool)
{
    unsigned int i = 0;
    if (!root->key_hash || !root->node_hash || !root->key_cmp || !root->node_cmp)
        return -1;
    if (pool == NULL || bucket_shift > JHASHMAP_MAX_SHIFT)
        return -1;

    root->num = 0;
    root->bucket_shift = bucket_shift;
    root->bucket_num = 1U << bucket_shift;
    root->bucket_mask = root->bucket_num - 1;

    root->buckets = (${name}_bucket_t *)jmalloc((root->bucket_num + 1) * sizeof(${name}_bucket_t));
    if (!root->buckets)
        return -1;
    memset(root->buckets, 0, (root->bucket_num + 1) * sizeof(${name}_bucket_t));

    for (i = 0; i <= root->bucket_num; ++i)
        root->buckets[i].head.next = JHASHMAP_INVALID;
    root->buckets[root->bucket_num].next = root->bucket_num;
    root->buckets[root->bucket_num].prev = root->bucket_num;
    root->pool = pool;

    return 0;
}

int ${name}_resize(struct ${name}_root *root, unsigned int bucket_shift)
{
    unsigned int i = 0;
    unsigned int bucket_num = 1U << bucket_shift;
    unsigned int bucket_mask = bucket_num - 1;
    ${name}_bucket_t *hd_head = &root->buckets[root->bucket_num];
    ${name}_bucket_t *buckets;

    if (bucket_shift > JHASHMAP_MAX_SHIFT)
        return -1;
    if (root->bucket_shift == bucket_shift)
        return 0;

    buckets = (${name}_bucket_t *)malloc((bucket_num + 1) * sizeof(${name}_bucket_t));
    if (!buckets) {
        return -1;
    }
    memset(buckets, 0, (bucket_num + 1) * sizeof(${name}_bucket_t));

    for (i = 0; i <= bucket_num; ++i)
        buckets[i].head.next = JHASHMAP_INVALID;
    buckets[bucket_num].next = bucket_num;
    buckets[bucket_num].prev = bucket_num;

    if (root->bucket_shift < bucket_shift) {
        ${name}_bucket_t *cur;
        for (cur = &root->buckets[hd_head->next]; cur != hd_head; cur = &root->buckets[cur->next]) {
            unsigned int hash, pos;
            int c = cur->head.next, n;
            struct jphashmap_node *c_node;
            ${ST} *c_data;

            while (c != JHASHMAP_INVALID) {
                c_node = ${NAME}_FROM_INDEX(root, c);
                c_data = root->pool->data_unsafe(root->pool, c);
                hash = root->node_hash(c_data);
                pos = hash & bucket_mask;

                n = c_node->next;
                if (buckets[pos].head.next == JHASHMAP_INVALID)
                    _add_bucket(pos, bucket_num, buckets);
                c_node->next = buckets[pos].head.next;
                buckets[pos].head.next = c;
                c = n;
            }
        }
    } else {
        ${name}_bucket_t *cur;
        for (cur = &root->buckets[hd_head->next]; cur != hd_head; cur = &root->buckets[cur->next]) {
            unsigned int hash, pos;
            int c = cur->head.next, n;
            struct jphashmap_node *c_node, *n_node;
            ${ST} *c_data;

            c_node = ${NAME}_FROM_INDEX(root, c);
            c_data = root->pool->data_unsafe(root->pool, c);
            hash = root->node_hash(c_data);
            pos = hash & bucket_mask;

            if (buckets[pos].head.next == JHASHMAP_INVALID) {
                _add_bucket(pos, bucket_num, buckets);
                buckets[pos].head.next = c;
            } else {
                n = c_node->next;
                while (n != JHASHMAP_INVALID) {
                    n_node = ${NAME}_FROM_INDEX(root, n);
                    c = n;
                    c_node = ${NAME}_FROM_INDEX(root, c);
                    n = n_node->next;
                }
                c_node->next = buckets[pos].head.next;
                buckets[pos].head.next = cur->head.next;
            }
        }
    }

    root->bucket_shift = bucket_shift;
    root->bucket_num = bucket_num;
    root->bucket_mask = root->bucket_num - 1;
    jfree(root->buckets);
    root->buckets = buckets;

    return 0;
}

void ${name}_uninit(struct ${name}_root *root)
{
    root->num = 0;
    if (root->buckets) {
        jfree(root->buckets);
        root->buckets = NULL;
    }
}

${ST} *${name}_find(struct ${name}_root *root, const void *key, ${ST} **prev)
{
    unsigned int hash = root->key_hash(key);
    unsigned int pos = hash & root->bucket_mask;
    struct jphashmap_node *head = &root->buckets[pos].head;

    int c = head->next;
    struct jphashmap_node *c_node;
    ${ST} *c_data;
    ${ST} *p_data = &${name}_node_entry(head)->data;

    while (c != JHASHMAP_INVALID) {
        c_node = ${NAME}_FROM_INDEX(root, c);
        c_data = root->pool->data_unsafe(root->pool, c);
        if (root->key_cmp(c_data, key) == 0) {
            if (prev)
                *prev = p_data;
            return c_data;
        }
        p_data = c_data;
        c = c_node->next;
    }

    return NULL;
}

${ST} *${name}_find_node(struct ${name}_root *root, ${ST} *node, ${ST} **prev)
{
    unsigned int hash = root->node_hash(node);
    unsigned int pos = hash & root->bucket_mask;
    struct jphashmap_node *head = &root->buckets[pos].head;

    int c = head->next;
    struct jphashmap_node *c_node;
    ${ST} *c_data;
    ${ST} *p_data = &${name}_node_entry(head)->data;

    while (c != JHASHMAP_INVALID) {
        c_node = ${NAME}_FROM_INDEX(root, c);
        c_data = root->pool->data_unsafe(root->pool, c);
        if (root->node_cmp(c_data, node) == 0) {
            if (prev)
                *prev = p_data;
            return c_data;
        }
        p_data = c_data;
        c = c_node->next;
    }

    return NULL;
}

void ${name}_fast_add(struct ${name}_root *root, ${ST} *node)
{
    unsigned int hash = root->node_hash(node);
    unsigned int pos = hash & root->bucket_mask;
    struct jphashmap_node *head = &root->buckets[pos].head;
    struct jphashmap_node *a_node = ${NAME}_FROM_DATA(node);
    int a = root->pool->index_unsafe(root->pool, node);

    if (head->next == JHASHMAP_INVALID)
        _add_bucket(pos, root->bucket_num, root->buckets);

    a_node->next = head->next;
    head->next = a;
    ++root->num;
}

${ST} *${name}_add(struct ${name}_root *root, ${ST} *node)
{
    unsigned int hash = root->node_hash(node);
    unsigned int pos = hash & root->bucket_mask;
    struct jphashmap_node *head = &root->buckets[pos].head;
    struct jphashmap_node *a_node = ${NAME}_FROM_DATA(node);
    int a = root->pool->index_unsafe(root->pool, node);

    int c = head->next;
    struct jphashmap_node *c_node, *p_node = head;
    ${ST} *c_data;

    while (c != JHASHMAP_INVALID) {
        c_node = ${NAME}_FROM_INDEX(root, c);
        c_data = root->pool->data_unsafe(root->pool, c);
        if (root->node_cmp(c_data, node) == 0) {
            p_node->next = a;
            a_node->next = c_node->next;
            c_node->next = JHASHMAP_INVALID;
            return c_data;
        }
        p_node = c_node;
        c = c_node->next;
    }

    if (head->next == JHASHMAP_INVALID)
        _add_bucket(pos, root->bucket_num, root->buckets);

    a_node->next = head->next;
    head->next = a;
    ++root->num;

    return NULL;
}

int ${name}_del(struct ${name}_root *root, ${ST} *node, ${ST} *prev)
{
    unsigned int hash = root->node_hash(node);
    unsigned int pos = hash & root->bucket_mask;
    struct jphashmap_node *head = &root->buckets[pos].head;
    struct jphashmap_node *d_node = ${NAME}_FROM_DATA(node);

    int c = head->next;
    struct jphashmap_node *c_node, *p_node = head;
    ${ST} *c_data;

    if (!prev) {
        while (c != JHASHMAP_INVALID) {
            c_node = ${NAME}_FROM_INDEX(root, c);
            c_data = root->pool->data_unsafe(root->pool, c);
            if (root->node_cmp(c_data, node) == 0) {
                goto success;
            }
            p_node = c_node;
            c = c_node->next;
        }
        return -1;
    } else {
        p_node = ${NAME}_FROM_DATA(prev);
    }

success:
    --root->num;
    p_node->next = d_node->next;
    d_node->next = JHASHMAP_INVALID;
    if (head->next == JHASHMAP_INVALID)
        _del_bucket(pos, root->buckets);
    return 0;
}

void ${name}_loop(struct ${name}_root *root, unsigned long (*cb)(${ST} *node))
{
    ${name}_bucket_t *cur, *next;
    ${name}_bucket_t *hd_head = &root->buckets[root->bucket_num];

    if (!cb || !root->buckets)
        return;

    for (cur = &root->buckets[hd_head->next], next = &root->buckets[cur->next];\\
            cur != hd_head; cur = next, next = &root->buckets[next->next]) {
        unsigned long ret = JHASHMAP_NEXT;
        struct jphashmap_node bak;
        struct jphashmap_node *head = &cur->head;
        int c = cur->head.next, n;
        struct jphashmap_node *p_node = head, *c_node;
        ${ST} *c_data;

        while (c != JHASHMAP_INVALID) {
            c_node = ${NAME}_FROM_INDEX(root, c);
            c_data = root->pool->data_unsafe(root->pool, c);
            bak = *c_node;
            n = c_node->next;

            ret = cb(c_data);
            switch (ret) {
            case JHASHMAP_NEXT:
                p_node = c_node;
                break;
            case JHASHMAP_STOP:
                return;
            case JHASHMAP_DEL:
                --root->num;
                p_node->next = bak.next;
                if (head->next == JHASHMAP_INVALID)
                    _del_bucket(next->prev, root->buckets);
                break;
            default: //JHASHMAP_ADD
                c_data = (${ST} *)ret;
                p_node = ${NAME}_FROM_DATA((${ST} *)ret);
                p_node->next = n;
                ++root->num;
                break;
            }
            c = p_node->next;
        }
    }
}
EOF
