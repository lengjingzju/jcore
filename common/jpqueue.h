/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stddef.h>
#include "jheap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   优先级队列实现为最小堆
 * @note    最小堆是一棵完全二叉树，所有父节点的值小于或等于两个子节点的值
 */
typedef struct {
    int size;                           // 当前队列大小
    int capacity;                       // 队列的容量
    void **array;                       // 存储数据的指针
    int (*cmp)(void *a, void *b);       // 数据比较函数
    void (*iset)(void *item, int index);// 设置索引函数
} jpqueue_t;

/**
 * @brief   默认设置索引函数(内部函数)
 * @note    防止设置索引函数为空
 */
static inline void _jpqueue_iset(void *item, int index) { }

/**
 * @brief   初始化优先级队列
 * @param   pq [INOUT] 用户需要填写的配置: capacity, cmp
 * @return  成功返回0; 失败返回-1
 * @note    优先级队列最小值是第一个元素
 */
static inline int jpqueue_init(jpqueue_t *pq)
{
    if (!pq->capacity || !pq->cmp) {
        return -1;
    }

    if (!pq->iset)
        pq->iset = _jpqueue_iset;

    pq->size = 0;
    pq->array = (void **)jheap_malloc(pq->capacity * sizeof(void *));
    if (!pq->array) {
        return -1;
    }
    pq->array[0] = NULL;


    return 0;
}

/**
 * @brief   反初始化优先级队列
 * @param   pq [INOUT] 优先级队列句柄
 * @return  无返回值
 * @note    无
 */
static inline void jpqueue_uninit(jpqueue_t *pq)
{
    if (pq) {
        if (pq->array) {
            jheap_free(pq->array);
            pq->array = NULL;
        }
        pq->size = 0;
    }
}

/**
 * @brief   交换两个元素的值(内部函数)
 * @param   pq [IN] 优先级队列句柄
 * @param   a [INOUT] 元素的指针的指针
 * @param   b [INOUT] 元素的指针的指针
 * @return  无返回值
 * @note    无
 */
static inline void _jpqueue_swap(void **a, void **b)
{
    void *temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief   向上调整维护最小堆性质(内部函数)
 * @param   pq [IN] 优先级队列句柄
 * @param   i [IN] 元素的位置
 * @return  无返回值
 * @note    无
 */
static inline void _jpqueue_fixup(jpqueue_t *pq, int i)
{
    int parent = (i - 1) / 2;
    while (i != 0 && pq->cmp(pq->array[i], pq->array[parent]) < 0) {
        _jpqueue_swap(&pq->array[i], &pq->array[parent]);
        pq->iset(pq->array[i], i);
        pq->iset(pq->array[parent], parent);
        i = parent;
        parent = (i - 1) / 2;
    }
}

/**
 * @brief   向下调整维护最小堆性质(内部函数)
 * @param   pq [IN] 优先级队列句柄
 * @param   i [IN] 元素的位置
 * @return  无返回值
 * @note    无
 */
static inline void _jpqueue_fixdown(jpqueue_t *pq, int i)
{
    int min = i, left, right;

    while (1) {
        left = 2 * i + 1;
        right = left + 1;

        if (left < pq->size && pq->cmp(pq->array[left], pq->array[min]) < 0)
            min = left;

        if (right < pq->size && pq->cmp(pq->array[right], pq->array[min]) < 0)
            min = right;

        if (min == i)
            break;

        _jpqueue_swap(&pq->array[i], &pq->array[min]);
        pq->iset(pq->array[i], i);
        pq->iset(pq->array[min], min);
        i = min;
    }
}

/**
 * @brief   获取队列中的值最小的元素
 * @param   pq [IN] 优先级队列句柄
 * @return  有元素时返回元素; 无元素时返回NULL
 * @note    无
 */
static inline void *jpqueue_head(jpqueue_t *pq)
{
    return pq->array[0];
}

/**
 * @brief   提取并返回队列中的值最小的元素
 * @param   pq [IN] 优先级队列句柄
 * @return  有元素时返回元素; 无元素时返回NULL
 * @note    无
 */
static inline void *jpqueue_pop(jpqueue_t *pq)
{
    void *root = pq->array[0];

    switch (pq->size) {
    case 0: return NULL;
    case 1: pq->size = 0; pq->array[0] = NULL; return root;
    default: break;
    }

    pq->array[0] = pq->array[--pq->size];
    pq->iset(pq->array[0], 0);
    _jpqueue_fixdown(pq, 0);

    return root;
}

/**
 * @brief   向队列中插入一个新元素
 * @param   pq [IN] 优先级队列句柄
 * @param   item [IN] 要插入的元素
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
static inline int jpqueue_add(jpqueue_t *pq, void *item)
{
    int i = pq->size;

    if (pq->size == pq->capacity) {
        return -1;
    }
    ++pq->size;
    pq->array[i] = item;
    pq->iset(item, i);
    _jpqueue_fixup(pq, i);

    return 0;
}

/**
 * @brief   向队列中删除某位置的一个元素
 * @param   pq [IN] 优先级队列句柄
 * @param   i [IN] 元素的位置
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
static inline int jpqueue_idel(jpqueue_t *pq, int i)
{
    --pq->size;
    if (i != pq->size) {
        pq->array[i] = pq->array[pq->size];
        pq->iset(pq->array[i], i);
        _jpqueue_fixdown(pq, i);
        _jpqueue_fixup(pq, i);
    }
    pq->array[pq->size] = NULL;

    return 0;
}

/**
 * @brief   向队列中删除一个元素
 * @param   pq [IN] 优先级队列句柄
 * @param   item [IN] 要删除的元素
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
static inline int jpqueue_del(jpqueue_t *pq, void *item)
{
    int i = 0;

    for (i = 0; i < pq->size; ++i) {
        if (pq->array[i] == item)
            return jpqueue_idel(pq, i);
    }
    return -1;
}

#ifdef __cplusplus
}
#endif
