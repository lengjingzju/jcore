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

#define JPERF_NET_LEN   16      // 网络接口名最大长度
#define JPERF_NET_NUM   16      // 网络设备最大数量

/**
 * @brief   CPU使用信息
 * @note    单位是tick/jiffies，值是乘了CPU核心数的
 */
typedef struct {
    uint64_t run_cost;          // 当前进程运行态耗时或系统的运行态耗时
    uint64_t total_cost;        // 系统运行时间(xCPUs)
} jperf_cpu_usage_t;

/**
 * @brief   内存使用信息
 * @note    单位是KB
 */
typedef struct {
    uint64_t vir_size;          // 虚拟内存占用
    uint64_t phy_size;          // 物理内存占用
    uint64_t total_size;        // 系统总物理内存，获取进程内存时此值无效
} jperf_mem_usage_t;

/**
 * @brief   网络设备发送接收信息
 */
typedef struct {
    uint64_t rx_bytes;          // 接收字节数
    uint64_t tx_bytes;          // 发送字节数
    uint64_t rx_speed;          // 接收速率
    uint64_t tx_speed;          // 发送速率
    char name[JPERF_NET_LEN];   // 网络设备名
} jperf_net_data_t;

/**
 * @brief   所有网络设备发送接收信息
 */
typedef struct {
    int num;                    // 网络设备的记录数量
    uint64_t sys_msec;          // 系统启动以来的毫秒数
    jperf_net_data_t datas[JPERF_NET_NUM]; // 信息数组
} jperf_net_usage_t;

/**
 * @brief   获取当前进程的CPU使用率信息
 * @param   last_usage [INOUT] 上次的CPU信息
 * @return  成功返回使用率百分比值; 失败返回-1
 * @note    需要上次信息用来得出这段时间段的CPU使用率
 */
int jperf_process_cpu_usage(jperf_cpu_usage_t *last_usage);

/**
 * @brief   获取系统的CPU使用率信息
 * @param   last_usage [INOUT] 上次的CPU信息
 * @return  成功返回使用率百分比值; 失败返回-1
 * @note    需要上次信息用来得出这段时间段的CPU使用率
 */
int jperf_system_cpu_usage(jperf_cpu_usage_t *last_usage);

/**
 * @brief   获取当前进程的内存信息
 * @param   mem_usage [OUT] 获取到的内存信息
 * @return  成功返回0; 失败返回-1
 * @note    不会获取到全部物理内存信息
 */
int jperf_process_mem_usage(jperf_mem_usage_t *mem_usage);

/**
 * @brief   获取系统的内存信息
 * @param   mem_usage [OUT] 获取到的内存信息
 * @return  成功返回0; 失败返回-1
 * @note    会获取到全部物理内存信息
 */
int jperf_system_mem_usage(jperf_mem_usage_t *mem_usage);

/**
 * @brief   获取网络设备的发送接收信息
 * @param   last_usage [INOUT] 上次的发送接收信息
 * @return  成功返回0; 失败返回-1
 * @note    需要上次信息用来得出这段时间段的网络速率
 */
int jperf_system_net_usage(jperf_net_usage_t *last_usage);

#ifdef __cplusplus
}
#endif
