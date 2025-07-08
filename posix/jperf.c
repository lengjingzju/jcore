/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "jtime.h"
#include "jperf.h"

int jperf_process_cpu_usage(jperf_cpu_usage_t *last_usage)
{
    jperf_cpu_usage_t cpu_usage = {0};
    long utime, stime, cutime, cstime;
    int percent = 0;

    FILE *fp = fopen("/proc/self/stat", "r");
    if (!fp) {
        return -1;
    }
    /* 以时钟周期为单位: utime 用户态运行时间；stime 核心态运行时间；
       cutime：等待子进程的用户态时间累计值； cstime：等待子进程的核心态时间累计值 */
    fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld %ld %ld", &utime, &stime, &cutime, &cstime);
    fclose(fp);

    cpu_usage.run_cost = utime + stime + cutime + cstime;
    cpu_usage.total_cost = jtime_monomsec_get() * sysconf(_SC_NPROCESSORS_ONLN) * sysconf(_SC_CLK_TCK) / 1000;
    if (last_usage->total_cost && cpu_usage.total_cost > last_usage->total_cost)
        percent = (cpu_usage.run_cost - last_usage->run_cost) * 100 / (cpu_usage.total_cost - last_usage->total_cost);
    *last_usage = cpu_usage;

    return percent;
}

int jperf_system_cpu_usage(jperf_cpu_usage_t *last_usage)
{
    jperf_cpu_usage_t cpu_usage = {0};
    long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    int percent = 0;

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }
    /* 以时钟周期为单位: user 用户态下运行的时间；nice 用户态下以低优先级运行的时间；system 核心态下运行的时间；
       idle 空闲时间； iowait 等待I/O操作的时间； irq 硬中断时间； softirq 软中断时间； steal 虚拟机偷取时间；
       guest 作为客人OS运行的时间； guest_nice 作为客人OS运行的低优先级时间；*/
    fscanf(fp, "%*s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
    fclose(fp);

    cpu_usage.run_cost = user + nice + system + irq + softirq + steal;
    cpu_usage.total_cost = cpu_usage.run_cost + idle + iowait;
    if (last_usage->total_cost && cpu_usage.total_cost > last_usage->total_cost)
        percent = (cpu_usage.run_cost - last_usage->run_cost) * 100 / (cpu_usage.total_cost - last_usage->total_cost);
    *last_usage = cpu_usage;

    return percent;
}

int jperf_process_mem_usage(jperf_mem_usage_t *mem_usage)
{
    int max = 2, cnt = 0;
    char line[256];

    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmSize:", 7) == 0) { /* 虚拟内存占用 */
            mem_usage->vir_size = (uint64_t)atol(line + 7);
        } else if (strncmp(line, "VmRSS:", 6) == 0) { /* 物理内存占用 */
            mem_usage->phy_size = (uint64_t)atol(line + 6);
        } else {
            continue;
        }
        if (++cnt == max)
            break;
    }
    fclose(fp);

    return 0;
}

int jperf_system_mem_usage(jperf_mem_usage_t *mem_usage)
{
    int max = 6, cnt = 0;
    long MemTotal = 0, MemFree = 0, Buffers = 0, Cached = 0, SwapTotal = 0, SwapFree = 0;
    char line[256];

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) { /* 系统物理内存的总量 */
            MemTotal = atol(line + 9);
        } else if (strncmp(line, "MemFree:", 8) == 0) { /* 空闲的物理内存 */
            MemFree = atol(line + 8);
        } else if (strncmp(line, "Buffers:", 8) == 0) { /* 用于缓冲的内存量 */
            Buffers = atol(line + 8);
        } else if (strncmp(line, "Cached:", 7) == 0) { /* 用于缓存的内存量 */
            Cached = atol(line + 7);
        } else if (strncmp(line, "SwapTotal:", 10) == 0) { /* 交换空间总量 */
            SwapTotal = atol(line + 10);
        } else if (strncmp(line, "SwapFree:", 9) == 0) { /* 空闲的交换空间 */
            SwapFree = atol(line + 9);
        } else {
            continue;
        }
        if (++cnt == max)
            break;
    }
    fclose(fp);

    mem_usage->phy_size = MemTotal - (MemFree + Buffers + Cached);
    mem_usage->vir_size = mem_usage->phy_size + (SwapTotal - SwapFree);
    mem_usage->total_size = MemTotal;

    return 0;
}

int jperf_system_net_usage(jperf_net_usage_t *last_usage)
{
    jperf_net_usage_t net_usage = {0};
    long long int rx_bytes, tx_bytes;
    uint64_t diff = 0;
    int len = 0, i = 0, j = 0;
    char *tmp1 = NULL, *tmp2 = NULL;
    char line[256];

    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return -1;
    }

    /* 去掉两行表格头 */
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        tmp1 = line;
        while (*tmp1 == ' ')
            ++tmp1;
        tmp2 = strchr(tmp1, ':');
        if (!tmp2)
            break;
        len = tmp2 - tmp1;
        if (len >= JPERF_NET_LEN)
            len = JPERF_NET_LEN - 1;
        sscanf(line, "%*s %lld %*d %*d %*d %*d %*d %*d %*d %lld", &rx_bytes, &tx_bytes);

        net_usage.datas[net_usage.num].rx_bytes = rx_bytes;
        net_usage.datas[net_usage.num].tx_bytes = tx_bytes;
        memcpy(net_usage.datas[net_usage.num].name, tmp1, len);
        net_usage.datas[net_usage.num].name[len] = '\0';

        if (++net_usage.num == JPERF_NET_NUM)
            break;
    }
    fclose(fp);

    net_usage.sys_msec = jtime_monomsec_get();

    if (last_usage->sys_msec && net_usage.sys_msec > last_usage->sys_msec) {
        diff = net_usage.sys_msec - last_usage->sys_msec;
        for (i = 0; i < net_usage.num; ++i) {
            if (i < last_usage->num && strcmp(net_usage.datas[i].name, last_usage->datas[i].name) == 0) {
                j = i;
            } else {
                for (j = 0; j < last_usage->num; ++j) {
                    if (strcmp(net_usage.datas[i].name, last_usage->datas[j].name) == 0)
                        break;
                }
                if (j == last_usage->num) {
                    continue;
                }
            }
            net_usage.datas[i].rx_speed = (net_usage.datas[i].rx_bytes - last_usage->datas[j].rx_bytes) * 1000 / diff;
            net_usage.datas[i].tx_speed = (net_usage.datas[i].tx_bytes - last_usage->datas[j].tx_bytes) * 1000 / diff;
        }
    }
    memcpy(last_usage, &net_usage, sizeof(net_usage));

    return 0;
}
