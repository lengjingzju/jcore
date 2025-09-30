/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include "jperf.h"

int jperf_process_cpu_usage(jperf_cpu_usage_t *last_usage)
{
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    ULARGE_INTEGER kernel, user;
    jperf_cpu_usage_t cpu_usage = {0};
    int percent = 0;

    if (!GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser))
        return -1;

    kernel.LowPart = ftKernel.dwLowDateTime;
    kernel.HighPart = ftKernel.dwHighDateTime;
    user.LowPart = ftUser.dwLowDateTime;
    user.HighPart = ftUser.dwHighDateTime;

    cpu_usage.run_cost = (kernel.QuadPart + user.QuadPart) / 10000; // ms
    cpu_usage.total_cost = GetTickCount64() * GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    if (last_usage->total_cost && cpu_usage.total_cost > last_usage->total_cost) {
        percent = (int)((cpu_usage.run_cost - last_usage->run_cost) * 100 /
                        (cpu_usage.total_cost - last_usage->total_cost));
    }
    *last_usage = cpu_usage;
    return percent;
}

int jperf_system_cpu_usage(jperf_cpu_usage_t *last_usage)
{
    FILETIME idleTime, kernelTime, userTime;
    ULARGE_INTEGER idle, kernel, user;
    jperf_cpu_usage_t cpu_usage = {0};
    int percent = 0;

    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return -1;

    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    cpu_usage.run_cost = (kernel.QuadPart + user.QuadPart - idle.QuadPart) / 10000;
    cpu_usage.total_cost = (kernel.QuadPart + user.QuadPart) / 10000;

    if (last_usage->total_cost && cpu_usage.total_cost > last_usage->total_cost) {
        percent = (int)((cpu_usage.run_cost - last_usage->run_cost) * 100 /
                        (cpu_usage.total_cost - last_usage->total_cost));
    }
    *last_usage = cpu_usage;
    return percent;
}

int jperf_process_mem_usage(jperf_mem_usage_t *mem_usage)
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        return -1;

    mem_usage->phy_size = pmc.WorkingSetSize >> 10;
    mem_usage->vir_size = pmc.PrivateUsage >> 10;
    mem_usage->total_size = 0;
    return 0;
}

int jperf_system_mem_usage(jperf_mem_usage_t *mem_usage)
{
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex))
        return -1;

    mem_usage->total_size = statex.ullTotalPhys >> 10;
    mem_usage->phy_size = (statex.ullTotalPhys - statex.ullAvailPhys) >> 10;
    mem_usage->vir_size = (statex.ullTotalPageFile - statex.ullAvailPageFile) >> 10;
    return 0;
}

int jperf_system_net_usage(jperf_net_usage_t *last_usage)
{
    memset(last_usage, 0, sizeof(*last_usage));
    return -1;
#if 0
    PMIB_IF_TABLE2 pIfTable;
    if (GetIfTable2(&pIfTable) != NO_ERROR)
        return -1;

    jperf_net_usage_t net_usage = {0};
    net_usage.sys_msec = GetTickCount64();

    for (ULONG i = 0; i < pIfTable->NumEntries && net_usage.num < JPERF_NET_NUM; i++) {
        MIB_IF_ROW2 row = pIfTable->Table[i];
        jperf_net_data_t *d = &net_usage.datas[net_usage.num];

        d->rx_bytes = row.InOctets;
        d->tx_bytes = row.OutOctets;
        WideCharToMultiByte(CP_ACP, 0, row.Alias, -1, d->name, JPERF_NET_LEN, NULL, NULL);

        net_usage.num++;
    }
    FreeMibTable(pIfTable);

    if (last_usage->sys_msec && net_usage.sys_msec > last_usage->sys_msec) {
        uint64_t diff = net_usage.sys_msec - last_usage->sys_msec;
        for (int i = 0; i < net_usage.num; i++) {
            for (int j = 0; j < last_usage->num; j++) {
                if (strcmp(net_usage.datas[i].name, last_usage->datas[j].name) == 0) {
                    net_usage.datas[i].rx_speed =
                        (net_usage.datas[i].rx_bytes - last_usage->datas[j].rx_bytes) * 1000 / diff;
                    net_usage.datas[i].tx_speed =
                        (net_usage.datas[i].tx_bytes - last_usage->datas[j].tx_bytes) * 1000 / diff;
                    break;
                }
            }
        }
    }
    memcpy(last_usage, &net_usage, sizeof(net_usage));
    return 0;
#endif
}
