/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stddef.h>
#include <pthread.h>
#include <semaphore.h>
#include "jtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   互斥锁
 * @note    mutex都为jthread_mutex_t的指针
 */
#define jthread_mutex_t                 pthread_mutex_t
#define jthread_mutex_init(mutex)       pthread_mutex_init(mutex, NULL)
#define jthread_mutex_destroy(mutex)    pthread_mutex_destroy(mutex)

#define jthread_mutex_lock(mutex)       pthread_mutex_lock(mutex)
#define jthread_mutex_unlock(mutex)     pthread_mutex_unlock(mutex)

/**
 * @brief   超时互斥锁
 * @note    1. mutex都为jthread_tmutex_t的指针
 *          2. linux上超时互斥锁和互斥锁没有区别
 *             windows的CRITICAL_SECTION不支持超时，需要使用Mutex(性能较差)支持超时互斥锁机制
 */
#define jthread_tmutex_t                 pthread_mutex_t
#define jthread_tmutex_init(mutex)       pthread_mutex_init(mutex, NULL)
#define jthread_tmutex_destroy(mutex)    pthread_mutex_destroy(mutex)

#define jthread_tmutex_lock(mutex)       pthread_mutex_lock(mutex)
#define jthread_tmutex_unlock(mutex)     pthread_mutex_unlock(mutex)
static inline int jthread_tmutex_timedlock(jthread_tmutex_t *mutex, jtime_nt_t *jnt)
{
    struct timespec ts = {.tv_sec = jnt->sec, .tv_nsec = (long)jnt->nsec};
    return pthread_mutex_timedlock(mutex, &ts);
}
static inline int jthread_tmutex_mtimelock(jthread_tmutex_t *mutex, uint32_t msec)
{
    if (msec == 0) // 超时为0时，尝试解锁
        return pthread_mutex_trylock(mutex);

    jtime_nt_t jnt;
    jtime_ntime_after(&jnt, msec, 0); // 互斥锁没有以CLOCK_MONOTONIC为参考的延时
    return jthread_tmutex_timedlock(mutex, &jnt);
}

/**
 * @brief   读写锁
 * @note    rwlock都为jthread_rwlock_t的指针
 */
#define jthread_rwlock_t                pthread_rwlock_t
#define jthread_rwlock_init(rwlock)     pthread_rwlock_init(rwlock, NULL)
#define jthread_rwlock_destroy(rwlock)  pthread_rwlock_destroy(rwlock)

#define jthread_rwlock_rdlock(rwlock)   pthread_rwlock_rdlock(rwlock)
#define jthread_rwlock_wrlock(rwlock)   pthread_rwlock_wrlock(rwlock)
#define jthread_rwlock_rdunlock(rwlock) pthread_rwlock_unlock(rwlock)
#define jthread_rwlock_wrunlock(rwlock) pthread_rwlock_unlock(rwlock)

/**
 * @brief   自旋锁
 * @note    lock都为jthread_spinlock_t的指针
 */
#define jthread_spinlock_t              pthread_spinlock_t
#define jthread_spin_init(lock)         pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
#define jthread_spin_destroy(lock)      pthread_spin_destroy(lock)

#define jthread_spin_lock(lock)         pthread_spin_lock(lock)
#define jthread_spin_unlock(lock)       pthread_spin_unlock(lock)

/**
 * @brief   条件变量
 * @note    1. cond都为jthread_cond_t的指针，mutex都为jthread_mutex_t的指针
 *          2. 使用超时条件变量时有区别：可选择初始化为系统单调时钟或初始化为日期时钟(默认)
 */
typedef struct _jthread_cond {
    int mono_clock; // 0: 日期时钟，1: 系统单调时钟
    pthread_cond_t cond_var;
} jthread_cond_t;
static inline int jthread_cond_init(jthread_cond_t *cond, int mono_clock)
{
    cond->mono_clock = mono_clock;
    if (cond->mono_clock == 0)
        return pthread_cond_init(&cond->cond_var, NULL); // 使用日历时钟

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);  // 使用单调时钟
    int ret = pthread_cond_init(&cond->cond_var, &attr);
    pthread_condattr_destroy(&attr);
    return ret;
}
#define jthread_cond_destroy(cond)      pthread_cond_destroy(&(cond)->cond_var)

#define jthread_cond_signal(cond)       pthread_cond_signal(&(cond)->cond_var)
#define jthread_cond_broadcast(cond)    pthread_cond_broadcast(&(cond)->cond_var)
#define jthread_cond_wait(cond, mutex)  pthread_cond_wait(&(cond)->cond_var, mutex)
static inline int jthread_cond_timedwait(jthread_cond_t *cond, jthread_mutex_t *mutex, jtime_nt_t *jnt)
{
    struct timespec ts = {.tv_sec = jnt->sec, .tv_nsec = (long)jnt->nsec};
    return pthread_cond_timedwait(&cond->cond_var, mutex, &ts);
}
static inline int jthread_cond_mtimewait(jthread_cond_t *cond, jthread_mutex_t *mutex, uint32_t msec)
{
    jtime_nt_t jnt;
    jtime_ntime_after(&jnt, msec, cond->mono_clock);
    return jthread_cond_timedwait(cond, mutex, &jnt);
}

/**
 * @brief   线程信号量
 * @note    sem都为jthread_sem_t的指针
 */
#define jthread_sem_t                   sem_t
#define jthread_sem_init(sem, value)    sem_init(sem, 0, value)
#define jthread_sem_destroy(sem)        sem_destroy(sem)

#define jthread_sem_post(sem)           sem_post(sem)
#define jthread_sem_wait(sem)           sem_wait(sem)
static inline int jthread_sem_timedwait(jthread_sem_t *sem, jtime_nt_t *jnt)
{
    struct timespec ts = {.tv_sec = jnt->sec, .tv_nsec = (long)jnt->nsec};
    return sem_timedwait(sem, &ts);
}
static inline int jthread_sem_mtimewait(jthread_sem_t *sem, uint32_t msec)
{
    if (msec == 0) // 超时为0时，尝试等待信号量
        return sem_trywait(sem);

    jtime_nt_t jnt;
    jtime_ntime_after(&jnt, msec, 0); // 信号量没有以CLOCK_MONOTONIC为参考的延时
    return jthread_sem_timedwait(sem, &jnt);
}

/**
 * @brief   线程属性
 */
typedef struct {
    int stack_size;                     // 线程堆栈大小，最小要为16KB
    int thread_prio;                    // 线程优先级，目前暂不关心此参数
    int detach_flag;                    // detach标记，设置detach后，无需在子进程再调用jthread_detach，也无需在父进程调用jthread_join
} jthread_attr_t;

/**
 * @brief   线程ID类型
 */
#define jthread_t                       pthread_t

/**
 * @brief   线程函数返回值类型
 */
#define jthread_ret_t                   void*

/**
 * @brief   线程函数指针
 */
typedef jthread_ret_t (*jthread_run_t)(void*);

/**
 * @brief   线程毫秒级睡眠
 */
#define jthread_msleep(msec)            jtime_msleep(msec)

/**
 * @brief   线程微秒级睡眠
 */
#define jthread_usleep(usec)            jtime_usleep(usec)

/**
 * @brief   线程让出CPU
 * @note    yield和sleep的区别
 *          1. yield让出CPU后仍可能立即返回并继续执行原进程，不会进入阻塞状态
 *          2. sleep使线程进入阻塞状态，直到指定时间结束或被信号唤醒
 */
#define jthread_yield()                 pthread_yield()

/**
 * @brief   线程退出
 * @note    retval是存储退出码的void*指针，可以为NULL
 */
#define jthread_exit(retval)            pthread_exit(retval)

/**
 * @brief   等待线程退出
 * @note    如果没有在线程函数调用jthread_detach，就需要在主线程等待线程退出，否则导致资源泄露
 */
#define jthread_join(thd)               pthread_join(thd, NULL)

/**
 * @brief   分离本线程
 * @note    如果在线程函数调用了jthread_detach，无需在主线程调用jthread_join
 */
#define jthread_detach()                pthread_detach(pthread_self())

/**
 * @brief   设置本线程名
 * @note    名字长度限制为16字符(含空字符)，是一个非标准的pthread库函数，可能不是所有系统都实现了该函数
 */
#define jthread_setname(name)           pthread_setname_np(pthread_self(), name)

/**
 * @brief   创建线程
 * @param   thd [OUT] 保存线程ID
 * @param   attr [IN] 线程的属性
 * @param   fn [IN] 线程函数指针
 * @param   arg [IN] 传给线程函数的值
 * @return  成功返回0; 失败返回1
 * @note    注意传递给线程函数的arg需要是堆变量或静态变量
 */
static inline int jthread_create(jthread_t *thd, const jthread_attr_t *attr, jthread_run_t fn, void *arg)
{
    int ret = 0;

    if (!attr) {
        ret = pthread_create(thd, NULL, fn, arg);
    } else {
        pthread_attr_t tattr;
        int min_stack = 1 << 14;

        if (pthread_attr_init(&tattr)) {
            return 1;
        }
        if (pthread_attr_setstacksize(&tattr, (size_t)(attr->stack_size > min_stack ? attr->stack_size : min_stack))) {
            pthread_attr_destroy(&tattr);
            return 1;
        }
        ret = pthread_create(thd, &tattr, fn, arg);
        if (!ret && attr->detach_flag)
            pthread_detach(*thd);
        pthread_attr_destroy(&tattr);
    }

    return ret == 0 ? 0 : 1;
}

#ifdef __cplusplus
}
#endif
