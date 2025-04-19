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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   动态数组管理结构
 */
struct ${name} {
    size_t size;                        // 当前元素数量
    size_t asize;                       // 容量每次增加的大小
    size_t capacity;                    // 当前容量
    ${T} *data;                         // 数据存储指针
#ifndef JVECTOR_ASIZE_DEF
#define JVECTOR_ASIZE_DEF   8u
#endif
};

/**
 * @brief   初始化动态数组管理结构
 * @param   vec [IN] 要初始化的动态数组管理结构
 * @param   add_capacity [IN] 数组扩容默认增加大小
 * @param   init_capacity [IN] 数组容量初始大小
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_init(struct ${name} *vec, size_t add_capacity, size_t init_capacity);

/**
 * @brief   反初始化动态数组管理结构
 * @param   vec [IN] 动态数组管理结构
 * @return  无返回值
 * @note    无
 */
void ${name}_uninit(struct ${name} *vec);

/**
 * @brief   调整动态数组的容量
 * @param   vec [IN] 动态数组管理结构
 * @param   new_capacity [IN] 动态数组新的容量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_resize(struct ${name} *vec, size_t new_capacity);

/**
 * @brief   访问动态数组的元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  返回指针
 * @note    不检查index的合法性，index的合法性由用户保证，速度较快
 */
static inline ${T} *${name}_at_unsafe(struct ${name} *vec, size_t index)
{
    return vec->data + index;
}

/**
 * @brief   访问动态数组的元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回有效指针；失败返回NULL
 * @note    检查index的合法性
 */
static inline ${T} *${name}_at(struct ${name} *vec, size_t index)
{
    if (index >= vec->size)
        return NULL;
    return vec->data + index;
}

/**
 * @brief   动态数组末尾增加一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [IN] 要增加的元素
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_push(struct ${name} *vec, const ${T} *element);

/**
 * @brief   动态数组末尾删除一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [OUT] 被删除的元素，可以为空
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_pop(struct ${name} *vec, ${T} *element);

/**
 * @brief   动态数组末尾增加多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_pushs(struct ${name} *vec, const ${T} *elements, size_t num);

/**
 * @brief   动态数组末尾删除多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [OUT] 被删除的元素数组，可以为空
 * @param   num [INOUT] 被删除的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int ${name}_pops(struct ${name} *vec, ${T} *elements, size_t *num);

/**
 * @brief   动态数组任意位置增加一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [IN] 要增加的元素
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于vec->size
 */
int ${name}_insert(struct ${name} *vec, const ${T} *element, size_t index);

/**
 * @brief   动态数组任意位置删除一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于等于vec->size
 */
int ${name}_erase(struct ${name} *vec, size_t index);

/**
 * @brief   动态数组任意位置增加多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于vec->size
 */
int ${name}_inserts(struct ${name} *vec, const ${T} *elements, size_t num, size_t index);

/**
 * @brief   动态数组任意位置删除多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   num [INOUT] 被删除的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于等于vec->size
 */
int ${name}_erases(struct ${name} *vec, size_t *num, size_t index);

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

int ${name}_init(struct ${name} *vec, size_t add_capacity, size_t init_capacity)
{
    vec->size = 0;
    vec->asize = add_capacity ? add_capacity : JVECTOR_ASIZE_DEF;
    vec->capacity = init_capacity ? init_capacity : vec->asize;
    vec->data = (${T} *)jmalloc(vec->capacity * sizeof(${T}));
    if (!vec->data) {
        vec->capacity = 0;
        return -1;
    }
    return 0;
}

void ${name}_uninit(struct ${name} *vec)
{
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
    vec->size = 0;
    vec->capacity = 0;
}

static inline int ${name}_incsize(struct ${name} *vec, size_t new_capacity)
{
    ${T} *new_data = (${T} *)jrealloc(vec->data, new_capacity * sizeof(${T}));
    if (!new_data)
        return -1;
    vec->capacity = new_capacity;
    vec->data = new_data;
    return 0;
}

int ${name}_resize(struct ${name} *vec, size_t new_capacity)
{
    if (new_capacity < vec->capacity) {
        vec->data = (${T} *)jrealloc(vec->data, new_capacity * sizeof(${T}));
        vec->capacity = new_capacity;
        if (vec->size > new_capacity)
            vec->size = new_capacity;
    } else if (new_capacity > vec->capacity) {
        return ${name}_incsize(vec, new_capacity);
    }
    return 0;
}

int ${name}_push(struct ${name} *vec, const ${T} *element)
{
    if (vec->size >= vec->capacity) {
        if (${name}_incsize(vec, vec->size + 1 + vec->asize) < 0)
            return -1;
    }
    memcpy(vec->data + vec->size, element, sizeof(${T}));
    ++vec->size;
    return 0;
}

int ${name}_pop(struct ${name} *vec, ${T} *element)
{
    if (!vec->size)
        return -1;
    --vec->size;
    if (element)
        memcpy(element, vec->data + vec->size, sizeof(${T}));
    return 0;
}

int ${name}_pushs(struct ${name} *vec, const ${T} *elements, size_t num)
{
    if (vec->size + num > vec->capacity) {
        if (${name}_incsize(vec, vec->size + num + vec->asize) < 0)
            return -1;
    }
    memcpy(vec->data + vec->size, elements, num * sizeof(${T}));
    vec->size += num;
    return 0;
}

int ${name}_pops(struct ${name} *vec, ${T} *elements, size_t *num)
{
    if (!vec->size) {
        *num = 0;
        return -1;
    }

    if (*num >= vec->size) {
        *num = vec->size;
        vec->size = 0;
        if (elements)
            memcpy(elements, vec->data, *num * sizeof(${T}));
    } else {
        vec->size -= *num;
        if (elements)
            memcpy(elements, vec->data + vec->size, *num * sizeof(${T}));
    }
    return 0;
}

int ${name}_insert(struct ${name} *vec, const ${T} *element, size_t index)
{
    if (index > vec->size)
        return -1;
    if (vec->size >= vec->capacity) {
        if (${name}_incsize(vec, vec->size + 1 + vec->asize) < 0)
            return -1;
    }

    if (index == vec->size) {
        memcpy(vec->data + vec->size, element, sizeof(${T}));
        ++vec->size;
    } else {
        size_t diff = vec->size - index;
        ${T} *src = vec->data + index;
        ${T} *dst = src + 1;
        memmove(dst, src, diff * sizeof(${T}));
        memcpy(src, element, sizeof(${T}));
        ++vec->size;
    }
    return 0;
}

int ${name}_erase(struct ${name} *vec, size_t index)
{
    if (index >= vec->size)
        return -1;

    if (index == vec->size - 1) {
        --vec->size;
    } else {
        size_t diff = vec->size - index - 1;
        ${T} *dst = vec->data + index;
        ${T} *src = dst + 1;
        memmove(dst, src, diff * sizeof(${T}));
        --vec->size;
    }
    return 0;
}

int ${name}_inserts(struct ${name} *vec, const ${T} *elements, size_t num, size_t index)
{
    if (index > vec->size)
        return -1;
    if (vec->size + num > vec->capacity) {
        if (${name}_incsize(vec, vec->size + num + vec->asize) < 0)
            return -1;
    }

    if (index == vec->size) {
        memcpy(vec->data + vec->size, elements, num * sizeof(${T}));
        vec->size += num;
    } else  {
        size_t diff = vec->size - index;
        ${T} *src = vec->data + index;
        ${T} *dst = src + num;
        memmove(dst, src, diff * sizeof(${T}));
        memcpy(src, elements, num * sizeof(${T}));
        vec->size += num;
    }
    return 0;
}

int ${name}_erases(struct ${name} *vec, size_t *num, size_t index)
{
    if (index >= vec->size) {
        *num = 0;
        return -1;
    }

    if (*num + index >= vec->size) {
        *num = vec->size - index;
        vec->size = index;
    } else {
        size_t diff = vec->size - index - *num;
        ${T} *dst = vec->data + index;
        ${T} *src = dst + *num;
        memmove(dst, src, diff * sizeof(${T}));
        vec->size -= *num;
    }
    return 0;
}
EOF
