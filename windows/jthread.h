/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#include <winbase.h>
#include <synchapi.h>
#include <processthreadsapi.h>
#include "jtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   互斥锁
 * @note    mutex都为jthread_mutex_t的指针
 */
#define jthread_mutex_t                 CRITICAL_SECTION
static inline int jthread_mutex_init(jthread_mutex_t *mutex)
{
    InitializeCriticalSection(mutex);
    return 0;
}
static inline int jthread_mutex_destroy(jthread_mutex_t *mutex)
{
    DeleteCriticalSection(mutex);
    return 0;
}
static inline int jthread_mutex_lock(jthread_mutex_t *mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}
static inline int jthread_mutex_unlock(jthread_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

/**
 * @brief   超时互斥锁
 * @note    1. mutex都为jthread_tmutex_t的指针
 *          2. linux上超时互斥锁和互斥锁没有区别
 *             windows的CRITICAL_SECTION不支持超时，需要使用Mutex(性能较差)支持超时互斥锁机制
 */
#define jthread_tmutex_t                HANDLE
static inline int jthread_tmutex_init(jthread_tmutex_t *mutex)
{
    *mutex = CreateMutexA(NULL, FALSE, NULL);
    return *mutex ? 0 : 1;
}
static inline int jthread_tmutex_destroy(jthread_tmutex_t *mutex)
{
    CloseHandle(*mutex);
    *mutex = NULL;
    return 0;
}
static inline int jthread_tmutex_lock(jthread_tmutex_t *mutex)
{
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0 ? 0 : 1;
}
static inline int jthread_tmutex_unlock(jthread_tmutex_t *mutex)
{
    return ReleaseMutex(*mutex) ? 0 : 1;
}
static inline int jthread_tmutex_timedlock(jthread_tmutex_t *mutex, jtime_nt_t *jnt)
{
    uint64_t cur_ms = jtime_utcmsec_get();
    uint64_t target_ms = jnt->sec * 1000 + jnt->nsec / 1000000;
    return WaitForSingleObject(*mutex, target_ms > cur_ms ? (DWORD)(target_ms - cur_ms) : 0) == WAIT_OBJECT_0 ? 0 : 1;
}
static inline int jthread_tmutex_mtimelock(jthread_tmutex_t *mutex, uint32_t msec)
{
    return WaitForSingleObject(*mutex, (DWORD)msec) == WAIT_OBJECT_0 ? 0 : 1;
}

/**
 * @brief   读写锁
 * @note    rwlock都为jthread_rwlock_t的指针
 */
#define jthread_rwlock_t                SRWLOCK
static inline int jthread_rwlock_init(jthread_rwlock_t *rwlock)
{
    InitializeSRWLock(rwlock);
    return 0;
}
static inline int jthread_rwlock_destroy(jthread_rwlock_t *rwlock)
{
    return 0;
}
static inline int jthread_rwlock_rdlock(jthread_rwlock_t *rwlock)
{
    AcquireSRWLockShared(rwlock);
    return 0;
}
static inline int jthread_rwlock_wrlock(jthread_rwlock_t *rwlock)
{
    AcquireSRWLockExclusive(rwlock);
    return 0;
}
static inline int jthread_rwlock_rdunlock(jthread_rwlock_t *rwlock)
{
    ReleaseSRWLockShared(rwlock);
    return 0;
}
static inline int jthread_rwlock_wrunlock(jthread_rwlock_t *rwlock)
{
    ReleaseSRWLockExclusive(rwlock);
    return 0;
}

/**
 * @brief   自旋锁
 * @note    lock都为jthread_spinlock_t的指针
 */
typedef struct _jthread_spinlock {
    volatile LONG lock;  // 0=未锁定，1=锁定
} jthread_spinlock_t;
static inline int jthread_spin_init(jthread_spinlock_t *lock)
{
    lock->lock = 0;
    return 0;
}
static inline int jthread_spin_destroy(jthread_spinlock_t *lock)
{
    return 0;
}
static inline int jthread_spin_lock(jthread_spinlock_t *lock)
{
    while (InterlockedCompareExchange(&lock->lock, 1, 0) != 0) {
        YieldProcessor();  // 提示CPU让出时间片
    }
    return 0;
}
static inline int jthread_spin_unlock(jthread_spinlock_t *lock)
{
    InterlockedExchange(&lock->lock, 0);
    return 0;
}

/**
 * @brief   条件变量
 * @note    1. cond都为jthread_cond_t的指针，mutex都为jthread_mutex_t的指针
 *          2. 使用超时条件变量时有区别：可选择初始化为系统单调时钟或初始化为日期时钟(默认)
 */
typedef struct _jthread_cond {
    int mono_clock; // 0: 日期时钟，1: 系统单调时钟
    CONDITION_VARIABLE cond_var;
} jthread_cond_t;
static inline int jthread_cond_init(jthread_cond_t *cond, int mono_clock)
{
    cond->mono_clock = mono_clock;
    InitializeConditionVariable(&cond->cond_var);
    return 0;
}
static inline int jthread_cond_destroy(jthread_cond_t *cond)
{
    return 0;
}
static inline int jthread_cond_signal(jthread_cond_t *cond)
{
    WakeConditionVariable(&cond->cond_var);
    return 0;
}
static inline int jthread_cond_broadcast(jthread_cond_t *cond)
{
    WakeAllConditionVariable(&cond->cond_var);
    return 0;
}
static inline int jthread_cond_wait(jthread_cond_t *cond, jthread_mutex_t *mutex)
{
    return SleepConditionVariableCS(&cond->cond_var, mutex, INFINITE) ? 0 : 1;
}
static inline int jthread_cond_timedwait(jthread_cond_t *cond, jthread_mutex_t *mutex, jtime_nt_t *jnt)
{
    uint64_t cur_ms = cond->mono_clock == 0 ? jtime_utcmsec_get() : jtime_monomsec_get();
    uint64_t target_ms = jnt->sec * 1000 + jnt->nsec / 1000000;
    return SleepConditionVariableCS(&cond->cond_var, mutex, target_ms > cur_ms ? (DWORD)(target_ms - cur_ms) : 0) ? 0 : 1;
}
static inline int jthread_cond_mtimewait(jthread_cond_t *cond, jthread_mutex_t *mutex, uint32_t msec)
{
    return SleepConditionVariableCS(&cond->cond_var, mutex, msec) ? 0 : 1;
}

/**
 * @brief   线程信号量
 * @note    sem都为jthread_sem_t的指针
 */
#define jthread_sem_t                   HANDLE
static inline int jthread_sem_init(jthread_sem_t *sem, unsigned int value)
{
    *sem = CreateSemaphoreA(NULL, (LONG)value, MAXLONG, NULL);
    return *sem ? 0 : -1;
}
static inline int jthread_sem_destroy(jthread_sem_t *sem)
{
    CloseHandle(*sem);
    *sem = NULL;
    return 0;
}
static inline int jthread_sem_post(jthread_sem_t *sem)
{
    ReleaseSemaphore(*sem, 1, NULL);
    return 0;
}
static inline int jthread_sem_wait(jthread_sem_t *sem)
{
    return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}
static inline int jthread_sem_timedwait(jthread_sem_t *sem, jtime_nt_t *jnt)
{
    uint64_t cur_ms = jtime_utcmsec_get();
    uint64_t target_ms = jnt->sec * 1000 + jnt->nsec / 1000000;
    return WaitForSingleObject(*sem, target_ms > cur_ms ? (DWORD)(target_ms - cur_ms) : 0) == WAIT_OBJECT_0 ? 0 : -1;
}
static inline int jthread_sem_mtimewait(jthread_sem_t *sem, uint32_t msec)
{
    return WaitForSingleObject(*sem, (DWORD)msec) == WAIT_OBJECT_0 ? 0 : -1;
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
#define jthread_t                       HANDLE

/**
 * @brief   线程函数返回值类型
 */
#define jthread_ret_t                   DWORD

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
#define jthread_yield()                 YieldProcessor()

/**
 * @brief   线程退出
 * @note    retval是存储退出码的void*指针，可以为NULL
 */
static inline void jthread_exit(void *retval)
{
    ExitThread((DWORD)retval);
}

/**
 * @brief   等待线程退出
 * @note    如果没有在线程函数调用jthread_detach，就需要在主线程等待线程退出，否则导致资源泄露
 */
static inline int jthread_join(jthread_t thd)
{
    DWORD retval = 0;
    WaitForSingleObject(thd, INFINITE);
    GetExitCodeThread(thd, &retval);
    CloseHandle(thd);
    return 0;
}

/**
 * @brief   分离本线程
 * @note    如果在线程函数调用了jthread_detach，无需在主线程调用jthread_join
 */
static inline int jthread_detach(void)
{
    return CloseHandle(GetCurrentThread()) ? 0 : 1;
}

/**
 * @brief   设置本线程名
 * @note    Linux上名字长度限制为16字符(含空字符)，Windows 10+ 支持
 */
static inline int jthread_setname(const char *name)
{
    // SetThreadDescription要接受宽字符串
    //return SetThreadDescription(GetCurrentThread(), name) ? 0 : 1;
    return 1;
}

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
        *thd = CreateThread(NULL, 0, fn, arg, 0, NULL);
        return *thd ? 0 : 1;
    } else {
        int min_stack = 1 << 14;
        *thd = CreateThread(NULL, (attr->stack_size > min_stack ? attr->stack_size : min_stack), fn, arg, 0, NULL);
        if (*thd == NULL) {
            return 1;
        }

        if (attr->detach_flag) {
            CloseHandle(*thd);
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
