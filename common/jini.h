/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   加载ini文件并初始化配置
 * @param   fname [IN] 配置文件名，可以为NULL
 * @return  成功返回非0指针; 失败返回NULL
 * @note    配置文件不存在时也是返回非0句柄，此时只是初始化配置
 */
void *jini_init(const char *fname);

/**
 * @brief   反初始化配置
 * @param   hd [IN] 句柄
 * @return  无返回值
 * @note    反初始化释放所有的配置节点等
 */
void jini_uninit(void *hd);

/**
 * @brief   刷新配置到文件
 * @param   hd [IN] 句柄
 * @param   fname [IN] 配置文件名，有文件名时总是强制刷新到指定的文件
 * @return  成功返回0; 失败返回-1
 * @note    fname为NULL时，如果配置有改变，则刷新到jini_init时记录的文件名，为空字符串时不管配置有没有变，总是强制刷新
 *          写文件时先写入xxx.bak文件，再重命名为xxx
 */
int jini_flush(void* hd, const char *fname);

/**
 * @brief   删除指定的配置节点
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @return  无返回值
 * @note    无
 */
void jini_del_node(void *hd, const char *section, const char *key);

/**
 * @brief   删除指定的配置配置段
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @return  无返回值
 * @note    无
 */
void jini_del_section(void *hd, const char *section);

/**
 * @brief   删除所有的配置
 * @param   hd [IN] 句柄
 * @return  无返回值
 * @note    无
 */
void jini_del_all(void *hd);

/**
 * @brief   获取指定配置的字符串值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   dval [IN] 默认值
 * @return  返回获取到的字符串值
 * @note    如果没有找到指定配置节点，返回默认值dval
 */
const char *jini_get(void *hd, const char *section, const char *key, const char *dval);

/**
 * @brief   获取指定配置的整数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   dval [IN] 默认值
 * @return  返回获取到的整数值
 * @note    如果没有找到指定配置节点，返回默认值dval
 */
int32_t jini_get_int(void *hd, const char *section, const char *key, int32_t dval);

/**
 * @brief   获取指定配置的大整数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   dval [IN] 默认值
 * @return  返回获取到的大整数值
 * @note    如果没有找到指定配置节点，返回默认值dval
 */
int64_t jini_get_lint(void *hd, const char *section, const char *key, int64_t dval);

/**
 * @brief   获取指定配置的浮点数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   dval [IN] 默认值
 * @return  返回获取到的浮点数值
 * @note    如果没有找到指定配置节点，返回默认值dval
 */
double jini_get_double(void *hd, const char *section, const char *key, double dval);

/**
 * @brief   设置指定配置的字符串值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   val [IN] 要设置的值
 * @return  成功返回0; 失败返回-1
 * @note    如果没有找到指定配置节点，则增加配置；否则修改配置
 */
int jini_set(void *hd, const char *section, const char *key, const char *val);

/**
 * @brief   设置指定配置的整数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   val [IN] 要设置的值
 * @return  成功返回0; 失败返回-1
 * @note    如果没有找到指定配置节点，则增加配置；否则修改配置
 */
int jini_set_int(void *hd, const char *section, const char *key, int32_t val);

/**
 * @brief   设置指定配置的大整数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   val [IN] 要设置的值
 * @return  成功返回0; 失败返回-1
 * @note    如果没有找到指定配置节点，则增加配置；否则修改配置
 */
int jini_set_lint(void *hd, const char *section, const char *key, int64_t val);

/**
 * @brief   设置指定配置的浮点数值
 * @param   hd [IN] 句柄
 * @param   section [IN] 段名
 * @param   key [IN] 节点键名
 * @param   val [IN] 要设置的值
 * @return  成功返回0; 失败返回-1
 * @note    如果没有找到指定配置节点，则增加配置；否则修改配置
 */
int jini_set_double(void *hd, const char *section, const char *key, double val);

#ifdef __cplusplus
}
#endif
