/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "jtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   定时器会话管理结构
 */
struct jtimer_ctx {
    int epoll_fd;   // epoll描述符
    int timer_fd;   // 定时器描述符
    int wake_fd;    // 事件唤醒描述符
};

/**
 * @brief   初始化定时器
 * @param   ctx [OUT] 定时器会话管理结构
 * @return  成功返回0；失败返回-1
 * @note    无
 */
static inline int jtimer_init(struct jtimer_ctx *ctx)
{
    ctx->epoll_fd = -1;
    ctx->timer_fd = -1;
    ctx->wake_fd = -1;

    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd < 0)
        return -1;

    ctx->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (ctx->timer_fd < 0) {
        close(ctx->epoll_fd);
        ctx->epoll_fd = -1;
        return -1;
    }

    ctx->wake_fd = eventfd(0, EFD_NONBLOCK);
    if (ctx->wake_fd < 0) {
        close(ctx->epoll_fd);
        ctx->epoll_fd = -1;
        close(ctx->timer_fd);
        ctx->timer_fd = -1;
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = ctx->timer_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->timer_fd, &ev);
    ev.data.fd = ctx->wake_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->wake_fd, &ev);

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
    if (ctx->epoll_fd >= 0) {
        close(ctx->epoll_fd);
        ctx->epoll_fd = -1;
    }
    if (ctx->timer_fd >= 0) {
        close(ctx->timer_fd);
        ctx->timer_fd = -1;
    }
    if (ctx->wake_fd >= 0) {
        close(ctx->wake_fd);
        ctx->wake_fd = -1;
    }
}

/**
 * @brief   提前唤醒定时器
 * @param   ctx [IN] 定时器会话管理结构
 * @return  成功返回0；失败返回-1
 * @note    无
 */
static inline int jtimer_wakeup(struct jtimer_ctx *ctx)
{
    uint64_t val = 1;
    return write(ctx->wake_fd, &val, sizeof(val)) < 0 ? -1 : 0;
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
    /* 设置唤醒时间 */
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    its.it_value.tv_sec = wake_nt->sec;
    its.it_value.tv_nsec = wake_nt->nsec;
    timerfd_settime(ctx->timer_fd, TFD_TIMER_ABSTIME, &its, NULL);
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
    struct epoll_event events[2];
    int n = epoll_wait(ctx->epoll_fd, events, 2, msec >= 0 ? msec : -1);
    int i = 0;
    int ret = 0;
    uint64_t val = 0;
    for (i = 0; i < n; ++i) {
        if (events[i].data.fd == ctx->timer_fd) {
            ret |= read(ctx->timer_fd, &val, sizeof(val)) < 0 ? 0 : JTIMER_FLAG_EXPIRED;
        } else if (events[i].data.fd == ctx->wake_fd) {
            ret |= read(ctx->wake_fd, &val, sizeof(val)) < 0 ? 0 : JTIMER_FLAG_AWAKENED;
        }
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
