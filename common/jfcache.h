/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include "jfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   打开缓冲机制文件的句柄
 * @param   fname [IN] 要打开的文件
 * @param   mode [IN] 打开文件使用的模式 "r" "w" "a" "r+" "w+" "a+"
 * @param   bsize [IN] 缓冲内存的大小
 * @return  成功返回非0指针; 失败返回NULL
 * @note    缓冲机制主要是为了加快小块数据的读写
 */
void* jfcache_open(const char *fname, const char *mode, size_t bsize);

/**
 * @brief   释放缓冲机制文件的句柄
 * @param   hd [IN] 句柄
 * @return  成功返回0; 失败返回-1
 * @note    写入模式时关闭句柄会把缓冲中的数据写入物理设备，此时可能出错
 */
int jfcache_close(void* hd);

/**
 * @brief   刷新写入数据到物理设备
 * @param   hd [IN] 句柄
 * @return  成功返回0; 失败返回-1
 * @note    写入模式才会进行刷新操作
 */
int jfcache_sync(void* hd);

/**
 * @brief   读取数据
 * @param   hd [IN] 句柄
 * @param   buf [OUT] 读取数据存储的buffer
 * @param   size [IN] 存储的buffer的长度
 * @return  成功返回读取长度; 失败返回-1
 * @note    读取数据先读缓冲，缓冲不命中才从物理设备读
 */
ssize_t jfcache_read(void* hd, char *buf, size_t size);

/**
 * @brief   写入数据
 * @param   hd [IN] 句柄
 * @param   buf [IN] 要写入的数据
 * @param   size [IN] 要写入的数据长度
 * @return  成功返回写入长度; 失败返回-1
 * @note    写入数据先写入缓冲，缓冲满才刷新到物理设备
 */
ssize_t jfcache_write(void* hd, const char *buf, size_t size);

/**
 * @brief   告诉文件偏移
 * @param   hd [IN] 句柄
 * @return  成功返回文件偏移; 失败返回-1
 * @note    文件偏移不是物理的偏移，而是逻辑偏移
 */
ssize_t jfcache_tell(void* hd);

/**
 * @brief   修改文件偏移
 * @param   hd [IN] 句柄
 * @param   ssize_t [IN] 新的偏移
 * @param   whence [IN] 偏移的参考
 * @return  成功返回文件偏移; 失败返回-1
 * @note    可能是只改变逻辑偏移，不会进行物理偏移操作
 */
ssize_t jfcache_seek(void* hd, ssize_t offset, int whence);

#ifdef __cplusplus
}
#endif
