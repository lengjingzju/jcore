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
struct jpheap_mgr {
    int size;               // 每个内存单元的大小
    int num;                // 内存单元总数
    int sel;                // 当前空闲的序号，sel等于num时表示内存耗尽
    int sys;                // 如果内存池用尽，sys为0时直接返回NULL; 为其它值时继续从系统内存分配
    void *idx;              // 内存池状态数组，指针类型如下: num <= 256 uint8_t; num <= 65536 uint16_t; other uint32_t
    char *begin;            // 内存池开始地址
    char *end;              // 内存池结束地址
    void* (*alloc)(struct jpheap_mgr *mgr); // 内存申请函数
    void (*free)(struct jpheap_mgr *mgr, void *ptr); // 内存释放函数
};
typedef struct jpheap_mgr jpheap_mgr_t;

/**
 * @brief   初始化内存池管理结构
 * @param   mgr [INOUT] 内存池管理结构，用户必须填写: size, num, sys
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jpheap_init(jpheap_mgr_t *mgr);

/**
 * @brief   反初始化内存池管理结构
 * @param   mgr [IN] 内存池管理结构
 * @return  无返回值
 * @note    反初始化释放所有分配的内存
 */
void jpheap_uninit(jpheap_mgr_t *mgr);

/**
 * @brief   从内存池申请内存
 * @param   mgr [IN] 内存池管理结构
 * @return  成功返回非0指针; 失败返回NULL
 * @note    如果内存池用尽，会从系统内存申请
 */
static inline void *jpheap_alloc(jpheap_mgr_t *mgr)
{
    return mgr->alloc(mgr);
}

/**
 * @brief   从内存池释放内存
 * @param   mgr [IN] 内存池管理结构
 * @param   ptr [IN] 要释放的指针
 * @return  无返回值
 * @note    无
 */
static inline void jpheap_free(jpheap_mgr_t *mgr, void *ptr)
{
    mgr->free(mgr, ptr);
}

#ifdef __cplusplus
}
#endif
