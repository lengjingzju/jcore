/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include "jlog_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *str;        // 字符串数据
    int len;                // 字符串长度，不含'\0'
} jlog_str_t;

typedef enum {
    JLOG_TO_AUTO = 0,       // 不改变输出
    JLOG_TO_TTY = 1,        // 输出到终端
    JLOG_TO_FILE = 2,       // 输出到文件
    JLOG_TO_NET = 3         // 输出到网络
} jlog_mode_t;

typedef struct {
    int file_size;          // 每个log文件的最大大小
    int file_count;         // 最多多少个log文件，小于0时不限制文件数量
    const char *file_path;  // 日志存储的文件夹
} jlog_file_t;

typedef struct {
    int is_ipv6;            // 是否为IPV6
    int ip_port;            // IP端口
    const char *ip_addr;    // IP地址
} jlog_net_t;

typedef struct {
    int cpu_cycle;          // 多长时间采集一次CPU信息，0表示不采集
    int mem_cycle;          // 多长时间采集一次内存信息，0表示不采集
    int net_cycle;          // 多长时间采集一次网络信息，0表示不采集
} jlog_perf_t;

typedef struct {
    int buf_size;           // 日志缓冲区大小，只有初始化时缓冲区大小设置才有效
    int wake_size;          // 日志缓冲区有多长日志时唤醒写入线程
    int res_size;           // 日志缓冲保留默认大小，决定一次写log的最大长度
    int level;              // 日志输出等级
    jlog_mode_t mode;       // 日志输出模式
    jlog_file_t file;       // 输出到文件的配置
    jlog_net_t net;         // 输出到网络的配置
    jlog_perf_t perf;       // 采集系统信息的配置
} jlog_cfg_t;

/**
 * @brief   日志初始化
 * @param   cfg [IN] 日志配置参数，可以为NULL，此时使用默认参数
 * @return  成功返回0; 失败返回-1
 * @note    log初始化会分配缓冲，初始化锁，创建后台写入线程等，cfg中某参数为0时会取默认值
 */
int jlog_init(const jlog_cfg_t *cfg);

/**
 * @brief   日志反初始化
 * @param   无参数
 * @return  成功返回0; 失败返回-1
 * @note    销毁缓冲和锁，刷新写入并结束写入线程等
 */
void jlog_uninit(void);

/**
 * @brief   获取日志配置
 * @param   cfg [OUT] 日志配置参数，不可以为NULL
 * @return  成功返回0; 失败返回-1
 * @note    日志系统未初始化时返回-1
 */
int jlog_cfg_get(jlog_cfg_t *cfg);

/**
 * @brief   更新日志配置
 * @param   cfg [IN] 日志配置参数，不可以为NULL
 * @return  成功返回0; 失败返回-1
 * @note    cfg中某参数为0时此项参数不会更新
 */
int jlog_cfg_set(const jlog_cfg_t *cfg);

/**
 * @brief   获取日志输出等级
 * @param   无参数
 * @return  返回日志输出等级
 * @note    此接口一般不会被外部使用
 */
int jlog_level_get(void);

/**
 * @brief   设置日志输出等级
 * @param   level [IN] 要设置的输出等级
 * @return  无返回值
 * @note    jlog_cfg_set无法关闭日志输出，此接口可以
 */
void jlog_level_set(int level);

/**
 * @brief   获取性能监视配置参数
 * @param   perf [OUT] 获取到的配置
 * @return  无返回值
 * @note    无
 */
void jlog_perf_get(jlog_perf_t *perf);

/**
 * @brief   设置性能监视配置参数
 * @param   perf [IN] 要设置的配置
 * @return  无返回值
 * @note    无
 */
void jlog_perf_set(jlog_perf_t *perf);

/**
 * @brief   写入日志到内存缓冲
 * @param   level [IN] 本条日志输出等级
 * @param   module [IN] 产生日志的模块，可以为NULL，此时表示无模块{"N", 1}
 * @param   type [IN] 对日志的分类，可以为NULL，此时表示无类别{"N", 1}
 * @param   buf [IN] 要写入的日志数据，无需加入'\n'，日志系统内部会自动在末尾加上换行符
 * @param   count [IN] 要写入的日志长度
 * @return  返回写入日志体写入长度(不含头)，一般不用关心返回值
 * @note    固定长度的日志使用此接口，速度更快
 */
int jlog_write(int level, const jlog_str_t *module, const jlog_str_t *type, const char *buf, int count);

/**
 * @brief   写入日志到内存缓冲
 * @param   level [IN] 本条日志输出等级
 * @param   module [IN] 产生日志的模块，可以为NULL，此时表示无模块{"N", 1}
 * @param   type [IN] 对日志的分类，可以为NULL，此时表示无类别{"N", 1}
 * @param   fmt [IN] 日志格式化字符串，无需加入'\n'，日志系统内部会自动在末尾加上换行符
 * @param   ap [IN] 日志格式化数据
 * @return  返回写入日志体写入长度(不含头)，一般不用关心返回值
 * @note    类似printf的接口，速度较慢
 */
int jlog_vprint(int level, const jlog_str_t *module, const jlog_str_t *type, const char *fmt, va_list ap);

/**
 * @brief   写入日志到内存缓冲
 * @param   level [IN] 本条日志输出等级
 * @param   module [IN] 产生日志的模块，可以为NULL，此时表示无模块{"N", 1}
 * @param   type [IN] 对日志的分类，可以为NULL，此时表示无类别{"N", 1}
 * @param   fmt [IN] 日志格式化字符串，无需加入'\n'，日志系统内部会自动在末尾加上换行符
 * @param   ... [IN] 日志格式化数据
 * @return  返回写入日志体写入长度(不含头)，一般不用关心返回值
 * @note    类似printf的接口，速度较慢
 */
int jlog_print(int level, const jlog_str_t *module, const jlog_str_t *type, const char *fmt, ...);

/**
 * @brief   派生接口，大多数情况下都是使用派生接口
 */
#define jlog_wfatal(mod, type, buf, cnt) jlog_write(JLOG_LEVEL_FATAL, mod, type, buf, cnt)
#define jlog_werror(mod, type, buf, cnt) jlog_write(JLOG_LEVEL_ERROR, mod, type, buf, cnt)
#define jlog_wwarn( mod, type, buf, cnt) jlog_write(JLOG_LEVEL_WARN,  mod, type, buf, cnt)
#define jlog_winfo( mod, type, buf, cnt) jlog_write(JLOG_LEVEL_INFO,  mod, type, buf, cnt)
#define jlog_wdebug(mod, type, buf, cnt) jlog_write(JLOG_LEVEL_DEBUG, mod, type, buf, cnt)
#define jlog_wtrace(mod, type, buf, cnt) jlog_write(JLOG_LEVEL_TRACE, mod, type, buf, cnt)

#define jlog_fatal( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_FATAL, mod, type, fmt, ##__VA_ARGS__)
#define jlog_error( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_ERROR, mod, type, fmt, ##__VA_ARGS__)
#define jlog_warn(  mod, type, fmt, ...) jlog_print(JLOG_LEVEL_WARN,  mod, type, fmt, ##__VA_ARGS__)
#define jlog_info(  mod, type, fmt, ...) jlog_print(JLOG_LEVEL_INFO,  mod, type, fmt, ##__VA_ARGS__)
#define jlog_debug( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_DEBUG, mod, type, fmt, ##__VA_ARGS__)
#define jlog_trace( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_TRACE, mod, type, fmt, ##__VA_ARGS__)

#define JLOG_FATAL( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_FATAL, mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JLOG_ERROR( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_ERROR, mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JLOG_WARN(  mod, type, fmt, ...) jlog_print(JLOG_LEVEL_WARN,  mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JLOG_INFO(  mod, type, fmt, ...) jlog_print(JLOG_LEVEL_INFO,  mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JLOG_DEBUG( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_DEBUG, mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JLOG_TRACE( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_TRACE, mod, type, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define JLOG_FATNO( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_FATAL, mod, type, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)
#define JLOG_ERRNO( mod, type, fmt, ...) jlog_print(JLOG_LEVEL_ERROR, mod, type, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)

/**
 * @brief   无类别、无模块的派生接口
 */
#define jwfatal(buf, cnt) jlog_write(JLOG_LEVEL_FATAL, NULL, NULL, buf, cnt)
#define jwerror(buf, cnt) jlog_write(JLOG_LEVEL_ERROR, NULL, NULL, buf, cnt)
#define jwwarn( buf, cnt) jlog_write(JLOG_LEVEL_WARN,  NULL, NULL, buf, cnt)
#define jwinfo( buf, cnt) jlog_write(JLOG_LEVEL_INFO,  NULL, NULL, buf, cnt)
#define jwdebug(buf, cnt) jlog_write(JLOG_LEVEL_DEBUG, NULL, NULL, buf, cnt)
#define jwtrace(buf, cnt) jlog_write(JLOG_LEVEL_TRACE, NULL, NULL, buf, cnt)

#define jfatal( fmt, ...) jlog_print(JLOG_LEVEL_FATAL, NULL, NULL, fmt, ##__VA_ARGS__)
#define jerror( fmt, ...) jlog_print(JLOG_LEVEL_ERROR, NULL, NULL, fmt, ##__VA_ARGS__)
#define jwarn(  fmt, ...) jlog_print(JLOG_LEVEL_WARN,  NULL, NULL, fmt, ##__VA_ARGS__)
#define jinfo(  fmt, ...) jlog_print(JLOG_LEVEL_INFO,  NULL, NULL, fmt, ##__VA_ARGS__)
#define jdebug( fmt, ...) jlog_print(JLOG_LEVEL_DEBUG, NULL, NULL, fmt, ##__VA_ARGS__)
#define jtrace( fmt, ...) jlog_print(JLOG_LEVEL_TRACE, NULL, NULL, fmt, ##__VA_ARGS__)

#define JFATAL( fmt, ...) jlog_print(JLOG_LEVEL_FATAL, NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JERROR( fmt, ...) jlog_print(JLOG_LEVEL_ERROR, NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JWARN(  fmt, ...) jlog_print(JLOG_LEVEL_WARN,  NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JINFO(  fmt, ...) jlog_print(JLOG_LEVEL_INFO,  NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JDEBUG( fmt, ...) jlog_print(JLOG_LEVEL_DEBUG, NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define JTRACE( fmt, ...) jlog_print(JLOG_LEVEL_TRACE, NULL, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define JFATNO( fmt, ...) jlog_print(JLOG_LEVEL_FATAL, NULL, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)
#define JERRNO( fmt, ...) jlog_print(JLOG_LEVEL_ERROR, NULL, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)

extern const jlog_str_t g_jcore_mod; // jcore字符串定义

#ifdef __cplusplus
}
#endif
