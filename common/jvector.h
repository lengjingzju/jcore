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
struct jvector {
    size_t size;                        // 当前元素数量
    size_t esize;                       // 单个元素字节数
    size_t asize;                       // 容量每次增加的大小
    size_t capacity;                    // 当前容量
    void *data;                         // 数据存储指针
#ifndef JVECTOR_ASIZE_DEF
#define JVECTOR_ASIZE_DEF   8u
#endif
};

/**
 * @brief   初始化动态数组管理结构
 * @param   vec [IN] 要初始化的动态数组管理结构
 * @param   element_size [IN] 元素单元大小
 * @param   add_capacity [IN] 数组扩容默认增加大小
 * @param   init_capacity [IN] 数组容量初始大小
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_init(struct jvector *vec, size_t element_size, size_t add_capacity, size_t init_capacity);

/**
 * @brief   反初始化动态数组管理结构
 * @param   vec [IN] 动态数组管理结构
 * @return  无返回值
 * @note    无
 */
void jvector_uninit(struct jvector *vec);

/**
 * @brief   调整动态数组的容量
 * @param   vec [IN] 动态数组管理结构
 * @param   new_capacity [IN] 动态数组新的容量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_resize(struct jvector *vec, size_t new_capacity);

/**
 * @brief   访问动态数组的元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  返回指针
 * @note    不检查index的合法性，index的合法性由用户保证，速度较快
 */
static inline void *jvector_at_unsafe(struct jvector *vec, size_t index)
{
    return (void *)((char *)vec->data + index * vec->esize);
}

/**
 * @brief   访问动态数组的元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回有效指针；失败返回NULL
 * @note    检查index的合法性
 */
static inline void *jvector_at(struct jvector *vec, size_t index)
{
    if (index >= vec->size)
        return NULL;
    return jvector_at_unsafe(vec, index);
}

/**
 * @brief   动态数组末尾增加一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [IN] 要增加的元素
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_push(struct jvector *vec, const void *element);

/**
 * @brief   动态数组末尾删除一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [OUT] 被删除的元素，可以为空
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_pop(struct jvector *vec, void *element);

/**
 * @brief   动态数组末尾增加多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_pushs(struct jvector *vec, const void *elements, size_t num);

/**
 * @brief   动态数组末尾删除多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [OUT] 被删除的元素数组，可以为空
 * @param   num [INOUT] 被删除的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jvector_pops(struct jvector *vec, void *elements, size_t *num);

/**
 * @brief   动态数组任意位置增加一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   element [IN] 要增加的元素
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于vec->size
 */
int jvector_insert(struct jvector *vec, const void *element, size_t index);

/**
 * @brief   动态数组任意位置删除一个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于等于vec->size
 */
int jvector_erase(struct jvector *vec, size_t index);

/**
 * @brief   动态数组任意位置增加多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于vec->size
 */
int jvector_inserts(struct jvector *vec, const void *elements, size_t num, size_t index);

/**
 * @brief   动态数组任意位置删除多个元素
 * @param   vec [IN] 动态数组管理结构
 * @param   num [INOUT] 被删除的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于等于vec->size
 */
int jvector_erases(struct jvector *vec, size_t *num, size_t index);

#ifdef __cplusplus
}
#endif
