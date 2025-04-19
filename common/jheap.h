/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JHEAP_DEBUG
#define JHEAP_DEBUG                     0
#endif

#if !JHEAP_DEBUG
#define jheap_free(ptr)                 free(ptr)
#define jheap_malloc(size)              malloc(size)
#define jheap_calloc(nmemb, size)       calloc(nmemb, size)
#define jheap_realloc(ptr, size)        realloc(ptr, size)
#define jheap_strdup(s)                 strdup(s)
#define jheap_strndup(s, n)             strndup(s, n)
#else
#define jheap_free(ptr)                 jheap_free_debug(ptr, __func__, __LINE__)
#define jheap_malloc(size)              jheap_malloc_debug(size, __func__, __LINE__)
#define jheap_calloc(nmemb, size)       jheap_calloc_debug(nmemb, size, __func__, __LINE__)
#define jheap_realloc(ptr, size)        jheap_realloc_debug(ptr, size, __func__, __LINE__)
#define jheap_strdup(s)                 jheap_strdup_debug(s, __func__, __LINE__)
#define jheap_strndup(s, n)             jheap_strndup_debug(s, n, __func__, __LINE__)
#endif

/**
 * @brief   初始化内存调试管理结构
 * @param   tail_num [IN] 尾部校验字节的个数，最大256，用于检测是否越界，可设置为0表示不检测越界
 * @return  成功返回0; 失败返回-1
 * @note    主要是初始化内存池，因为使用内存池可以加快速度
 */
int jheap_init_debug(int tail_num);

/**
 * @brief   反初始化内存调试管理结构
 * @param   无参数
 * @return  无返回值
 * @note    主要是释放内存池分配的内存
 */
void jheap_uninit_debug(void);

/**
 * @brief   开始内存调试
 * @param   无参数
 * @return  无返回值
 * @note    开始内存调试才会记录内存分配信息
 */
void jheap_start_debug(void);

/**
 * @brief   停止内存调试
 * @param   无参数
 * @return  无返回值
 * @note    停止内存调试会清空所有内存分配信息
 */
void jheap_stop_debug(void);

/**
 * @brief   检查是否有内存越界
 * @param   无参数
 * @return  无返回值
 * @note    此功能jheap_init_debug时需要传入非0的tail_num
 */
void jheap_bound_debug(void);

/**
 * @brief   检查内存使用情况，查看是否有内存泄漏
 * @param   choice [IN] 检测打印节点信息的方式：0 未释放且有变化; 1 有变化; 2 未释放; 3 所有;
 *              >3 未释放数大于choice; < 0 未释放数大于-choice且有变化
 * @return  无返回值
 * @note    "未释放"指malloc和free不相等，"有变化"指这段时间有过alloc和free
 */
void jheap_leak_debug(int choice);

/**
 * @brief   修改高频检测选项
 * @param   dup_flag [IN] 是否高频检测当前的ptr和记录的ptr重复，即使用了debug的接口分配了内存，但没有使用jheap_free_debug释放
 * @param   bound_flag [IN] 是否高频检测记录的内存有越界
 * @return  无返回值
 * @note    高频检测指每次分配内存时都检测，显著降低性能
 */
void jheap_setflag_debug(int dup_flag, int bound_flag);

/**
 * @brief   修改线程检测内存方式
 * @param   check_flag [IN] 是否在线程中检测内存情况
 * @param   check_count [IN] 线程中多长时间检测一次内存情况，为0时不修改
 * @return  无返回值
 * @note    线程中检测每 10ms*check_count 检测一次
 */
void jheap_setcheck_debug(int check_flag, int check_count);

/**
 * @brief   修改要跟踪的内存的范围
 * @param   min_limit [IN] 纳入统计的堆内存分配大小下限
 * @param   max_limit [IN] 纳入统计的堆内存分配大小上限
 * @return  无返回值
 * @note    修改范围只跟踪指定范围大小的内存，提升性能
 */
void jheap_setlimit_debug(size_t min_limit, size_t max_limit);

/**
 * @brief   类似标注库对应函数的作用，只是多记录函数名和文件行
 */
void jheap_free_debug(void *ptr, const char *func, int line);
void *jheap_malloc_debug(size_t size, const char *func, int line);
void *jheap_calloc_debug(size_t nmemb, size_t size, const char *func, int line);
void *jheap_realloc_debug(void *ptr, size_t size, const char *func, int line);
char *jheap_strdup_debug(const char *s, const char *func, int line);
char *jheap_strndup_debug(const char *s, size_t n, const char *func, int line);

#ifdef __cplusplus
}
#endif
