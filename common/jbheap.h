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
#include <string.h>
#include "jlist.h"

typedef struct jbheap_node {
    struct jslist list;     // 单项链表节点
    char *ptr;              // 内存块指针
    int size;               // 内存块总大小
    int cur;                // 当前使用的内存大小
} jbheap_node_t;

typedef struct jbheap_mgr {
    struct jslist_head head;// 单项链表节点
    int size;               // 内存块默认大小
    int align;              // 内存对齐方式，2的align次方对齐，例如: -1，对齐到指针大小，0 不对齐，1 2字节对齐...
    int flag;               // 内存搜索方式，0: 只找当前节点，内存不满足就新建一个新节点; 1: 则是查找节点看能否分配
    jbheap_node_t *cur;     // 当前正在分配的节点
    void *(*alloc)(struct jbheap_mgr *mgr, int size); // 从内存块申请内存的函数指针
} jbheap_mgr_t;

/**
 * @brief   初始化内存块管理结构
 * @param   mgr [INOUT] 用户需要填写的配置: size, align, flag
 * @return  无返回值
 * @note    无
 */
void jbheap_init(jbheap_mgr_t *mgr);

/**
 * @brief   反初始化内存块管理结构
 * @param   mgr [IN] 内存块管理结构
 * @return  无返回值
 * @note    反初始化释放所有的内存块节点等
 */
void jbheap_uninit(jbheap_mgr_t *mgr);

/**
 * @brief   告知内存块的总大小
 * @param   mgr [IN] 内存块管理结构
 * @param   used [OUT] 内存块中已使用的内存总大小，可以为NULL表示不获取
 * @return  分配给内存块的内存总大小
 * @note    无
 */
int jbheap_tell(jbheap_mgr_t *mgr, int *used);

/**
 * @brief   将所有内存块realloc到已使用的大小
 * @param   mgr [IN] 内存块管理结构
 * @return  无返回值
 * @note    无
 */
void jbheap_adjust(jbheap_mgr_t *mgr);

/**
 * @brief   从内存块申请内存
 * @param   mgr [IN] 内存块管理结构
 * @param   size [IN] 需要申请的内存大小
 * @return  成功返回非0指针; 失败返回NULL
 * @note    align为0时，不做对齐检查，不遍历所有节点，速度快，如果从节点申请的内存全部是同一种类型的结构，可用此接口
 *          align不为0时，会按照mgr的align做对齐检查，会按照mgr的flag决定是否遍历所有节点
 */
static inline void *jbheap_alloc(jbheap_mgr_t *mgr, int size)
{
    return mgr->alloc(mgr, size);
}

/**
 * @brief   复制字符串数据到从内存块申请的内存
 * @param   mgr [IN] 内存块管理结构
 * @param   str [IN] 要复制的字符串数据
 * @return  成功返回非0指针; 失败返回NULL
 * @note    见jbheap_alloc说明
 */
static inline char *jbheap_strdup(jbheap_mgr_t *mgr, const char *str)
{
    int size = (int)strlen(str) + 1;
    char *ptr = (char *)mgr->alloc(mgr, size);
    if (ptr)
        memcpy((void *)ptr, (const void *)str, size);
    return ptr;
}

/**
 * @brief   复制指定数据到从内存块申请的内存
 * @param   mgr [IN] 内存块管理结构
 * @param   data [IN] 要复制的内存数据
 * @param   size [IN] 需要申请的内存大小
 * @return  成功返回非0指针; 失败返回NULL
 * @note    见jbheap_alloc说明
 */
static inline void *jbheap_datadup(jbheap_mgr_t *mgr, const void *data, int size)
{
    void *ptr = mgr->alloc(mgr, size);
    if (ptr)
        memcpy(ptr, data, size);
    return ptr;
}

#ifdef __cplusplus
}
#endif
