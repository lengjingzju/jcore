#!/bin/sh
############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2024-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
# https://github.com/lengjingzju/jcore     #
############################################

name=$1             # 生成的头文件或源文件名称、管理结构类型名、函数名前缀
T=$2                # 变量类型名

if [ -z "${name}" ] || [ -z "${T}" ] || [ "${name}" = "-h" ]; then
    echo "Usage: $0 <name> <T>"
    echo "       name: 生成的头文件或源文件名称、管理结构类型名、函数名前缀"
    echo "       T:    变量类型名"
    exit 1
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   内存池管理结构
 */
struct ${name}_pool {
    int num;                // 内存单元总数
    int sel;                // 当前空闲的序号，sel等于num时表示内存耗尽
    void *idx;              // 内存池状态数组，指针类型如下: num <= 256 uint8_t; num <= 65536 uint16_t; other uint32_t
    ${T} *begin;            // 内存池开始地址
    ${T} *end;              // 内存池结束地址
    ${T}* (*alloc)(struct ${name}_pool *pool); // 内存申请函数，返回指针
    void (*free)(struct ${name}_pool *pool, ${T} *ptr); // 内存释放函数，通过指针
    int (*alloci)(struct ${name}_pool *pool); // 内存申请函数，返回序号，返回-1表示失败
    void (*freei)(struct ${name}_pool *pool, int index); // 内存释放函数，通过序号
    void (*freei_unsafe)(struct ${name}_pool *pool, int index); // 内存释放函数，通过序号，不检查序号合法性
    ${T}* (*data)(struct ${name}_pool *pool, int index); // 通过序号得到指针
    ${T}* (*data_unsafe)(struct ${name}_pool *pool, int index); // 通过序号得到指针，不检查序号合法性
    int (*index)(struct ${name}_pool *pool, ${T} *ptr); // 通过指针得到序号，返回-1表示失败
    int (*index_unsafe)(struct ${name}_pool *pool, ${T} *ptr); // 通过指针得到序号，不检查指针合法性
};

/**
 * @brief   初始化内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @param   num [IN] 内存池的元素数量
 * @param   sys [IN] 如果内存池用尽，sys为0时直接返回NULL; 为其它值时继续从系统内存分配
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_init(struct ${name}_pool *pool, int num, int sys);

/**
 * @brief   扩容内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @param   num [IN] 新的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    num必须比原先的pool->num大，即只能扩容
 *          扩容realloc时begin地址可能被改变，正在使用内存地址时不能扩容
 */
int ${name}_resize(struct ${name}_pool *pool, int num);

/**
 * @brief   反初始化内存池管理结构
 * @param   pool [IN] 内存池管理结构
 * @return  无返回值
 * @note    反初始化释放所有分配的内存
 */
void ${name}_uninit(struct ${name}_pool *pool);

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

static inline ${T} *_${name}_alloc00(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint8_t *)pool->idx)[pool->sel++];
    return NULL;
}

static inline ${T} *_${name}_alloc01(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint16_t *)pool->idx)[pool->sel++];
    return NULL;
}

static inline ${T} *_${name}_alloc02(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint32_t *)pool->idx)[pool->sel++];
    return NULL;
}

static inline ${T} *_${name}_alloc10(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint8_t *)pool->idx)[pool->sel++];
    return (${T} *)jmalloc(sizeof(${T}));
}

static inline ${T} *_${name}_alloc11(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint16_t *)pool->idx)[pool->sel++];
    return (${T} *)jmalloc(sizeof(${T}));
}

static inline ${T} *_${name}_alloc12(struct ${name}_pool *pool)
{
    if (pool->sel < pool->num)
        return pool->begin + ((uint32_t *)pool->idx)[pool->sel++];
    return (${T} *)jmalloc(sizeof(${T}));
}

static inline void _${name}_free00(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    ((uint8_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
}

static inline void _${name}_free01(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    ((uint16_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
}

static inline void _${name}_free02(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    ((uint32_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
}

static inline void _${name}_free10(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    if (ptr < pool->begin || ptr >= pool->end)
        jfree((void *)ptr);
    else
        ((uint8_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
}

static inline void _${name}_free11(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    if (ptr < pool->begin || ptr >= pool->end)
        jfree((void *)ptr);
    else
        ((uint16_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
}

static inline void _${name}_free12(struct ${name}_pool *pool, ${T} *ptr)
{
    if (!ptr) return;
    if (ptr < pool->begin || ptr >= pool->end)
        jfree((void *)ptr);
    else
        ((uint32_t *)pool->idx)[--pool->sel] = ptr - pool->begin;
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

static inline ${T} *_${name}_data(struct ${name}_pool *pool, int index)
{
    if (index < 0 || index >= pool->num) return NULL;
    return &pool->begin[index];
}

static inline ${T} *_${name}_data_unsafe(struct ${name}_pool *pool, int index)
{
    return &pool->begin[index];
}

static inline int _${name}_index(struct ${name}_pool *pool, ${T} *ptr)
{
    if (ptr < pool->begin || ptr >= pool->end)
        return -1;
    return ptr - pool->begin;
}

static inline int _${name}_index_unsafe(struct ${name}_pool *pool, ${T} *ptr)
{
    return ptr - pool->begin;
}

int ${name}_init(struct ${name}_pool *pool, int num, int sys)
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

    if (!sys) {
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
    } else {
        if (pool->num <= 256) {
            pool->alloc = _${name}_alloc10;
            pool->free = _${name}_free10;
            pool->alloci = _${name}_alloci0;
            pool->freei = _${name}_freei0;
            pool->freei_unsafe = _${name}_freei0_unsafe;
        } else if (pool->num <= 65536) {
            pool->alloc = _${name}_alloc11;
            pool->free = _${name}_free11;
            pool->alloci = _${name}_alloci1;
            pool->freei = _${name}_freei1;
            pool->freei_unsafe = _${name}_freei1_unsafe;
        } else {
            pool->alloc = _${name}_alloc12;
            pool->free = _${name}_free12;
            pool->alloci = _${name}_alloci2;
            pool->freei = _${name}_freei2;
            pool->freei_unsafe = _${name}_freei2_unsafe;
        }
    }
    pool->data = _${name}_data;
    pool->data_unsafe = _${name}_data_unsafe;
    pool->index = _${name}_index;
    pool->index_unsafe = _${name}_index_unsafe;
    return 0;
}

int ${name}_resize(struct ${name}_pool *pool, int num)
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

void ${name}_uninit(struct ${name}_pool *pool)
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
