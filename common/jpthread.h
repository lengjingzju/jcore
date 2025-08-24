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
#include <stdint.h>
#include <stddef.h>

/**
 * @brief   线程池句柄
 * @note    实际是jpthread_mgr_t指针
 */
typedef void* jpthread_hd;

/**
 * @brief   任务句柄
 * @note    ptr实际是jpthread_task_t指针
 */
typedef struct {
    uint32_t id;                    // 任务ID
    void *ptr;                      // 任务结构体指针
} jpthread_td;

/**
 * @brief   任务回调函数
 * @note    无
 */
typedef void (*jpthread_cb)(void *args);

/**
 * @brief   创建线程池
 * @param   max_threads [IN] 线程池的最大线程数
 * @param   min_threads [IN] 线程池的最小线程数，设为0时接口内部自动更正为1
 * @param   max_tasks [IN] 线程池的最大任务数
 * @param   stack_size [IN] 线程池的线程栈默认大小，为0时默认线程栈为1MB
 * @return  成功返回线程池句柄; 失败返回NULL
 * @note    后续所有操作都是以线程池句柄为基础
 */
jpthread_hd jpthread_init(int max_threads, int min_threads, int max_tasks, int stack_size);

/**
 * @brief   销毁线程池
 * @param   hd [IN] 线程池句柄
 * @param   wait_flag [IN] 是否等待所有任务销毁完成
 * @return  无返回值
 * @note    本调用唤醒给线程池的主线程进入销毁流程：
 *          当wait_flag为0时，直接设置删除位，立即退出
 *          当wait_flag为1时，无需手动删除定时器，不执行剩余的任务，等待所有任务销毁完成；
 *          当wait_flag为-1时，一定要手动删除所有定时器(否则无法退出)，执行剩余的任务，会等待所有任务销毁完成
 */
void jpthread_uninit(jpthread_hd hd, int wait_flag);

/**
 * @brief   往线程池中加入一个任务
 * @param   hd [IN] 线程池句柄
 * @param   exec_cb [IN] 执行任务的回调函数，不能为NULL
 * @param   free_cb [IN] 销毁任务资源args的回调函数，无资源时为NULL
 * @param   args [IN] 任务的回调函数传入的参数
 * @param   cycle_ns [IN] 任务执行的周期(纳秒)
 * @param   wake_ns [IN] 任务延迟执行的时间(纳秒)
 * @return  成功返回任务句柄; 失败返回的任务句柄的ptr和id都为零
 * @note    后续所有任务操作都是以任务句柄为基础，任务类型有三种：
 *          1. 当cycle_ns和wake_ns为零时，创建的是普通任务，普通任务执行完后自动销毁
 *          2. 当cycle_ns为零且wake_ns不为零时，创建的是单次定时器任务，单次定时器任务执行完后自动销毁
 *          3. 当cycle_ns不为零时，创建的是重复定时器任务，需要del显式销毁，可以pause暂停和resume恢复
 *          4. 周期单位为纳秒并不是周期可以设置为小于1ms，而是为减小周期误差，例如视频帧率为30fps，周期使用毫秒单位就误差较大
 */
jpthread_td jpthread_task_add(jpthread_hd hd, jpthread_cb exec_cb, jpthread_cb free_cb, void *args,
    uint64_t cycle_ns, uint64_t wake_ns);
static inline jpthread_td jpthread_worker_add(jpthread_hd hd, jpthread_cb exec_cb, jpthread_cb free_cb, void *args)
{
    return jpthread_task_add(hd, exec_cb, free_cb, args, 0, 0);
}

/**
 * @brief   销毁任务
 * @param   hd [IN] 线程池句柄
 * @param   td [IN] 任务句柄
 * @return  无返回值
 * @note    一般定时器任务才需要调用本接口，重复定时器任务一定要调用本接口强制删除
 */
void jpthread_task_del(jpthread_hd hd, jpthread_td td);

/**
 * @brief   暂停重复定时器任务
 * @param   hd [IN] 线程池句柄
 * @param   td [IN] 任务句柄
 * @return  成功返回0，未操作返回-1
 * @note    用于暂停重复定时器任务
 */
int jpthread_task_pause(jpthread_hd hd, jpthread_td td);

/**
 * @brief   恢复重复定时器任务
 * @param   hd [IN] 线程池句柄
 * @param   td [IN] 任务句柄
 * @param   cycle_ns [IN] 任务执行的新周期(纳秒)，如果为0，不改变原有周期
 * @param   wake_ns [IN] 任务延迟执行的时间(纳秒)
 * @return  成功返回0，未操作返回-1
 * @note    用于恢复重复定时器任务或修改它的执行周期
 */
int jpthread_task_resume(jpthread_hd hd, jpthread_td td, uint64_t cycle_ns, uint64_t wake_ns);

/**
 * @brief   重设重复定时器任务
 * @param   hd [IN] 线程池句柄
 * @param   td [IN] 任务句柄
 * @param   cycle_ns [IN] 任务执行的新周期(纳秒)，如果为0，不改变原有周期
 * @param   wake_ns [IN] 任务延迟执行的时间(纳秒)
 * @return  成功返回0，未操作返回-1
 * @note    reset和resume接口的区别主要体现在对wake_ns的处理上：
 *          1. resume接口如果task不是正在执行，无条件wake_ns后执行
 *          2. reset接口如果task不是正在执行，下次执行时间取原预定时间和wake_ns后时间的较早者
 */
int jpthread_task_reset(jpthread_hd hd, jpthread_td td, uint64_t cycle_ns, uint64_t wake_ns);

#ifdef __cplusplus
}
#endif
