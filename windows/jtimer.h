/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <windows.h>
#include "jtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   定时器会话管理结构
 */
struct jtimer_ctx {
    HANDLE timer_handle;   // 定时器句柄
    HANDLE wake_handle;    // 唤醒事件句柄
    LARGE_INTEGER freq;    // 缓存系统时钟
};

/**
 * @brief   初始化定时器
 * @param   ctx [OUT] 定时器会话管理结构
 * @return  成功返回0；失败返回-1
 * @note    无
 */
static inline int jtimer_init(struct jtimer_ctx *ctx)
{
    ctx->timer_handle = CreateWaitableTimer(NULL, FALSE, NULL);
    if (ctx->timer_handle == NULL)
        return -1;
    ctx->wake_handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ctx->wake_handle == NULL) {
        CloseHandle(ctx->timer_handle);
        ctx->timer_handle = NULL;
        return -1;
    }
    QueryPerformanceFrequency(&ctx->freq);

    return 0;
}

/**
 * @brief   反初始化定时器
 * @param   ctx [IN] 定时器会话管理结构
 * @return  无返回值
 * @note    无
 */
static inline void jtimer_uninit(struct jtimer_ctx *ctx)
{
    if (ctx->timer_handle)
        CloseHandle(ctx->timer_handle);
    if (ctx->wake_handle)
        CloseHandle(ctx->wake_handle);
    ctx->timer_handle = NULL;
    ctx->wake_handle = NULL;
}

/**
 * @brief   提前唤醒定时器
 * @param   ctx [IN] 定时器会话管理结构
 * @return  成功返回0；失败返回-1
 * @note    无
 */
static inline int jtimer_wakeup(struct jtimer_ctx *ctx)
{
    SetEvent(ctx->wake_handle);
    return 0;
}

/**
 * @brief   设置定时器唤醒时间
 * @param   ctx [IN] 定时器会话管理结构
 * @param   wake_nt [IN] 唤醒时间，是绝对的系统时钟时间，不是UTC时间
 * @return  无返回值
 * @note    无
 */
static inline void jtimer_timeset(struct jtimer_ctx *ctx, jtime_nt_t *wake_nt)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    uint64_t cur_clock = (counter.QuadPart * 10000000ULL) / ctx->freq.QuadPart;
    uint64_t wake_clock = wake_nt->sec * 10000000ULL + wake_nt->nsec / 100ULL;

    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    if (cur_clock < wake_clock) {
        uli.QuadPart += wake_clock - cur_clock;
    }
    SetWaitableTimer(ctx->timer_handle, (LARGE_INTEGER *)&uli, 0, NULL, NULL, FALSE);
}

/**
 * @brief   等待定时器唤醒
 * @param   ctx [IN] 定时器会话管理结构
 * @param   msec [IN] 等待最大超时时间，小于0时永远等待
 * @return  返回唤醒的原因
 * @note    不要在锁保护区间调用本接口；注意可能要用“与”判断为什么唤醒
 */
static inline int jtimer_timewait(struct jtimer_ctx *ctx, int msec)
{
#define JTIMER_FLAG_EXPIRED     (1 << 0)    // 定时时间到被唤醒
#define JTIMER_FLAG_AWAKENED    (1 << 1)    // 被event唤醒(可能是提前唤醒)
    int ret = 0;
    DWORD wait_result = WaitForMultipleObjects(2, (HANDLE *)ctx, FALSE, msec >= 0 ? msec : INFINITE);

    switch (wait_result) {
    case WAIT_OBJECT_0: // 定时器触发
        ret |= JTIMER_FLAG_EXPIRED;
        break;
    case WAIT_OBJECT_0 + 1: // 唤醒事件触发
        ret |= JTIMER_FLAG_AWAKENED;
        ResetEvent(ctx->wake_handle);  // 手动重置事件，避免重复触发
        break;
    case WAIT_TIMEOUT: // 超时无事件
        break;
    default: // 错误处理
        break;
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
