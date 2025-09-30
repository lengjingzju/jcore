/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include "jlist.h"
#include "jpqueue.h"
#include "jheap.h"
#include "jpheap.h"
#include "jtime.h"
#include "jtimer.h"
#include "jthread.h"
#include "jpthread.h"

typedef enum {
    JPTHREAD_WORKER = 0,                // 立即执行的任务
    JPTHREAD_TIMER_ONCE,                // 延迟执行的任务
    JPTHREAD_TIMER_REPEAT,              // 重复执行的任务(运行状态)
    JPTHREAD_TIMER_PAUSED,              // 重复执行的任务(暂停状态)
    JPTHREAD_STOPED                     // 停止状态的任务
} jpthread_task_type;                   // 任务类型定义

typedef enum {
    JPTHREAD_IN_THREAD = 0,             // 任务在线程执行中
    JPTHREAD_IN_LIST,                   // 任务挂载在链表上
    JPTHREAD_IN_QUEUE                   // 任务挂载在优先级队列上
} jpthread_task_state;                  // 任务的状态

typedef struct {
    int running;                        // 运行状态
    int busy_flag;                      // 创建新线程时置位，用于回收逻辑
    int min_threads;                    // 最小线程数
    int stack_size;                     // 线程栈大小
    uint32_t cnt;                       // 任务id递增
    int pending_threads;                // 挂起的的线程数，即挂载到thread_head的节点数
    int pending_workers;                // 等待执行的任务数，即挂载到worker_head的节点数
    jthread_t thd;                      // 主线程id
    jthread_mutex_t mtx;                // 互斥锁
    struct jtimer_ctx ctx;              // 定时器会话管理结构
    jpheap_mgr_t thread_pheap;          // 存储线程结构的内存池
    jpheap_mgr_t task_pheap;            // 存储任务结构的内存池(worker + timer)
    struct jdlist_head thread_head;     // 空闲线程链表挂载节点
    struct jdlist_head worker_head;     // 立即执行的任务的链表挂载节点
    jpqueue_t timer_queue;              // 延迟或重复执行的任务的优先级队列
} jpthread_mgr_t;                       // 线程池管理结构

typedef struct {
    uint32_t id;                        // 任务id
    int index;                          // 任务在优先级队列中的序号
    jpthread_task_type type;            // 任务类型
    jpthread_task_state state;          // 任务所处状态
    uint64_t cycle_ns;                  // 周期纳秒数
    jtime_nt_t wake_nt;                 // 下次执行时间
    jpthread_cb exec_cb;                // 任务执行函数
    jpthread_cb free_cb;                // 资源释放函数
    void *args;                         // 任务执行参数
    struct jdlist_head list;            // 链表节点
} jpthread_task_t;                      // 任务管理结构

typedef struct {
    int running;                        // 线程运行状态
    jthread_t thd;                      // 线程id
    jthread_cond_t cond;                // 条件变量
    jpthread_mgr_t *mgr;                // 线程池管理结构
    struct jdlist_head list;            // 线程挂载节点
} jpthread_thread_t;                    // 任务管理结构

#define DEL_TASK() do {                             \
    task->type = JPTHREAD_STOPED;                   \
    task->id = 0;                                   \
    free_cb = task->free_cb;                        \
    args = task->args;                              \
    jpheap_free(&mgr->task_pheap, (void *)task);    \
    task = NULL;                                    \
} while(0)

#define CORRECT_NS          10000       // 定时器修正时间纳秒，当前时间早于此之内也加入执行队列
static inline int _check_expire(const jtime_nt_t *wake_nt, const jtime_nt_t *nt)
{
    if (nt->sec < wake_nt->sec) {
        return (wake_nt->sec - nt->sec == 1) ? ((nt->nsec + CORRECT_NS < 1000000000llu + wake_nt->nsec) ? 0 : 1) : 0;
    }

    if (nt->sec == wake_nt->sec && nt->nsec + CORRECT_NS < wake_nt->nsec) {
        return 0;
    }

    return 1;
}

/*
 * 优先级队列中线程比较两个节点的回调函数
 */
static int _timer_prio_cmp(void *a, void *b)
{
    jpthread_task_t *ta = (jpthread_task_t *)a;
    jpthread_task_t *tb = (jpthread_task_t *)b;
    int typea = ta->type > JPTHREAD_TIMER_REPEAT ? 1 : 0;
    int typeb = tb->type > JPTHREAD_TIMER_REPEAT ? 1 : 0;

    if (typea != typeb)
        return typea - typeb;
    if (ta->wake_nt.sec != tb->wake_nt.sec)
        return ta->wake_nt.sec < tb->wake_nt.sec ? -1 : 1;
    if (ta->wake_nt.nsec != tb->wake_nt.nsec)
        return ta->wake_nt.nsec < tb->wake_nt.nsec ? -1 : 1;

    return 0;
}

/*
 * 优先级队列中线程设置节点索引的回调函数
 */
static void _timer_index_set(void *item, int index)
{
    jpthread_task_t *titem = (jpthread_task_t *)item;
    titem->index = index;
}

/*
 * 任务执行线程，等到条件通知
 */
static jthread_ret_t _thread_run(void *arg)
{
    jpthread_task_t *task = NULL;
    jpthread_thread_t *thread = (jpthread_thread_t *)arg;
    jpthread_mgr_t *mgr= thread->mgr;
    jtime_nt_t nt = {0};
    jpthread_cb free_cb = NULL;
    void *args = NULL;

    jthread_setname("jpthread_task");
    while (1) {
        jthread_mutex_lock(&mgr->mtx);
        if (jdlist_empty(&mgr->worker_head)) {
            while (jdlist_empty(&mgr->worker_head) && mgr->running && thread->running) {
                jdlist_add_tail(&thread->list, &mgr->thread_head);
                ++mgr->pending_threads;
                jthread_cond_wait(&thread->cond, &mgr->mtx);
            }
            if (!mgr->running || !thread->running) {
                jthread_mutex_unlock(&mgr->mtx);
                break;
            }
        }

        task = jdlist_entry(mgr->worker_head.next, jpthread_task_t, list);
        jdlist_del(&task->list);
        --mgr->pending_workers;
        task->state = JPTHREAD_IN_THREAD;
next:
        jthread_mutex_unlock(&mgr->mtx);
        task->exec_cb(task->args); /* 执行任务 */
        if (task->type == JPTHREAD_TIMER_REPEAT)
            jtime_monontime_get(&nt);

        jthread_mutex_lock(&mgr->mtx);
        if (mgr->running) {
            free_cb = NULL;
            args = NULL;

            switch (task->type) {
            case JPTHREAD_TIMER_REPEAT:
                if (!_check_expire(&task->wake_nt, &nt)) {
                    /* 重复型任务未到期需要归还到优先级队列 */
                    task->state = JPTHREAD_IN_QUEUE;
                    jpqueue_add(&mgr->timer_queue, task);
                    if (jpqueue_head(&mgr->timer_queue) == (void *)task)
                        jtimer_wakeup(&mgr->ctx);
                    task = NULL;
                } else {
                    /* 重复型任务到期，直接下次执行 */
                    jtime_ntime_nadd(&task->wake_nt, task->cycle_ns);
                    goto next;
                }
                task = NULL;
                break;

            case JPTHREAD_TIMER_PAUSED:
                /* 暂停的重复型任务直接归还到优先级队列 */
                task->state = JPTHREAD_IN_QUEUE;
                jpqueue_add(&mgr->timer_queue, task);
                task = NULL;
                break;

            default:
                /* 其它任务直接删除 */
                DEL_TASK();
                break;
            }
        } else {
            DEL_TASK();
        }
        jthread_mutex_unlock(&mgr->mtx);

        if (free_cb) {
            free_cb(args);
        }
    }

    /* 销毁线程池时销毁本线程资源 */
    jthread_mutex_lock(&mgr->mtx);
    jthread_cond_destroy(&thread->cond);
    jpheap_free(&mgr->thread_pheap, (void *)thread);
    jthread_mutex_unlock(&mgr->mtx);

    return (jthread_ret_t)0;
}

/*
 * 从链表中获取一个空闲的线程或新建一个线程
 */
static jpthread_thread_t *_thread_wake(jpthread_mgr_t *mgr)
{
    jpthread_thread_t *thread = NULL;
    jthread_attr_t attr = {0};

    /* 如果线程池中有空闲线程，直接取出返回 */
    if (!jdlist_empty(&mgr->thread_head)) {
        thread = jdlist_entry(mgr->thread_head.next, jpthread_thread_t, list);
        jdlist_del(&thread->list);
        --mgr->pending_threads;
        jthread_cond_signal(&thread->cond);
        return thread;
    }
    ++mgr->busy_flag;

    /* 无空闲线程时创建一个新的线程资源 */
    thread = (jpthread_thread_t *)jpheap_alloc(&mgr->thread_pheap);
    if (!thread)
        return NULL;

    thread->running = 1;
    jthread_cond_init(&thread->cond, 0);
    thread->mgr = mgr;
    jdlist_init_head(&thread->list);

    attr.stack_size = mgr->stack_size;
    attr.detach_flag = 1;
    if (jthread_create(&thread->thd, &attr, _thread_run, (void *)thread) != 0) {
        jthread_cond_destroy(&thread->cond);
        jpheap_free(&mgr->thread_pheap, (void *)thread);
        return NULL;
    }
    jthread_cond_signal(&thread->cond);

    return thread;
}

/*
 * 线程池主函数，取优先级队列(优先)或链表中的任务执行
 */
static jthread_ret_t _thread_main(void *arg)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)arg;
    jpthread_task_t *task = NULL;
    jpthread_thread_t *thread = NULL, *nthread = NULL;
    jtime_nt_t nt = {0}, ntt = {0};
    jtime_t last_sec = 0;
    jpthread_cb free_cb = NULL;
    void *args = NULL;

    jthread_setname("jpthread_main");
    while (1) {
        jtime_monontime_get(&nt);
        jthread_mutex_lock(&mgr->mtx);
        if (!mgr->running) {
            jthread_mutex_unlock(&mgr->mtx);
            break;
        }

        /* 查询优先级队列中的任务 */
        while ((task = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue))) {
            if (task->type > JPTHREAD_TIMER_REPEAT)
                break;
            if (!_check_expire(&task->wake_nt, &nt))
                break;

            /* 任务到期，有线程资源时唤醒线程执行 */
            jpqueue_pop(&mgr->timer_queue);
            task->state = JPTHREAD_IN_LIST;
            jdlist_add_tail(&task->list, &mgr->worker_head);
            ++mgr->pending_workers;
            if (task->type == JPTHREAD_TIMER_REPEAT) {
                jtime_ntime_nadd(&task->wake_nt, task->cycle_ns);
            }

            _thread_wake(mgr);

            /* 让出CPU，让任务执行线程执行 */
            if ((mgr->pending_workers & 0xf) == 0) {
                jthread_mutex_unlock(&mgr->mtx);
                jthread_mutex_lock(&mgr->mtx);
            }
        }

        /* 设置上次线程忙的时间 */
        if (mgr->busy_flag) {
            mgr->busy_flag = 0;
            last_sec = nt.sec;
        }

        /* 当10秒没有新线程创建时或没有申请不到线程，且大于最小线程数，唤醒一个线程销毁 */
        if (nt.sec >= last_sec + 10) {
            last_sec = nt.sec - 5;
            if (mgr->thread_pheap.sel > mgr->min_threads && mgr->pending_threads > 1) {
                thread = jdlist_entry(mgr->thread_head.next, jpthread_thread_t, list);
                jdlist_del(&thread->list);
                --mgr->pending_threads;
                thread->running = 0;
                jthread_cond_signal(&thread->cond);
            }
        }

        /* 更新下次唤醒时间，有定时器任务符合更小时间条件开始取定时器时间，否则取默认1s后 */
        ntt.sec = 0;
        ntt.nsec = 0;
        task = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
        if (task && task->type <= JPTHREAD_TIMER_REPEAT) {
            jtime_monontime_get(&nt);
            if (!_check_expire(&task->wake_nt, &nt)) {
                ntt = task->wake_nt;
            } else {
                jthread_mutex_unlock(&mgr->mtx);
                continue;
            }
        }
        jtime_ntime_nadd(&nt, 999999999);
        if (ntt.sec || ntt.nsec) {
            if ((nt.sec > ntt.sec) || (nt.sec == ntt.sec && nt.nsec > ntt.nsec)) {
                nt = ntt;
            }
        }
        jtimer_timeset(&mgr->ctx, &nt);
        jthread_mutex_unlock(&mgr->mtx);

        jtimer_timewait(&mgr->ctx, 1000);
    }

    /* 销毁线程池时唤醒空闲的线程进行销毁 */
    jthread_mutex_lock(&mgr->mtx);
    jdlist_for_each_entry_safe(thread, nthread, &mgr->thread_head, list, jpthread_thread_t) {
        jdlist_del(&thread->list);
        --mgr->pending_threads;
        jthread_cond_signal(&thread->cond);
    }

    /* 销毁线程池时销毁链表中的任务 */
    while (!jdlist_empty(&mgr->worker_head)) {
        task = jdlist_entry(mgr->worker_head.next, jpthread_task_t, list);
        jdlist_del(&task->list);
        --mgr->pending_workers;
        DEL_TASK();
        jthread_mutex_unlock(&mgr->mtx);
        if (free_cb)
            free_cb(args);
        jthread_mutex_lock(&mgr->mtx);
    }

    /* 销毁线程池时销毁优先级队列中的任务 */
    while ((task = (jpthread_task_t *)jpqueue_pop(&mgr->timer_queue))) {
        DEL_TASK();
        jthread_mutex_unlock(&mgr->mtx);
        if (free_cb)
            free_cb(args);
        jthread_mutex_lock(&mgr->mtx);
    }
    jpqueue_uninit(&mgr->timer_queue);

    /* 等待任务线程退出 */
    while (mgr->thread_pheap.sel + mgr->task_pheap.sel) {
        jthread_mutex_unlock(&mgr->mtx);
        jthread_usleep(1);
        jthread_mutex_lock(&mgr->mtx);
    }
    jthread_mutex_unlock(&mgr->mtx);

    /* 销毁其它管理资源 */
    jpheap_uninit(&mgr->task_pheap);
    jpheap_uninit(&mgr->thread_pheap);
    jthread_mutex_destroy(&mgr->mtx);
    jtimer_uninit(&mgr->ctx) ;
    jheap_free((void *)mgr);

    return (jthread_ret_t)0;
}

jpthread_hd jpthread_init(int max_threads, int min_threads, int max_tasks, int stack_size)
{
    jpthread_mgr_t *mgr = NULL;
    jthread_attr_t attr = {0};

    if (!max_threads || !max_tasks)
        return NULL;

    mgr = (jpthread_mgr_t *)jheap_malloc(sizeof(jpthread_mgr_t));
    if (!mgr)
        return NULL;

    mgr->running = 1;
    mgr->busy_flag = 0;
    mgr->min_threads = min_threads;
    if (!mgr->min_threads)
        mgr->min_threads = 1;
    mgr->stack_size = stack_size;
    if (!mgr->stack_size)
        mgr->stack_size = 1 << 20; /* 默认1MB线程栈 */
    mgr->cnt = 0;

    jthread_mutex_init(&mgr->mtx);
    if (jtimer_init(&mgr->ctx) < 0) {
        goto err0;
    }

    /* 初始化存储线程资源的内存池 */
    mgr->thread_pheap.size = sizeof(jpthread_thread_t);
    mgr->thread_pheap.num = max_threads;
    if (jpheap_init(&mgr->thread_pheap) < 0) {
        goto err1;
    }

    /* 初始化存储任务资源的内存池 */
    mgr->task_pheap.size = sizeof(jpthread_task_t);
    mgr->task_pheap.num = max_tasks;
    if (jpheap_init(&mgr->task_pheap) < 0) {
        goto err2;
    }

    /* 初始化链表和优先级队列 */
    mgr->pending_threads = 0;
    mgr->pending_workers = 0;
    jdlist_init_head(&mgr->thread_head);
    jdlist_init_head(&mgr->worker_head);
    mgr->timer_queue.capacity = max_tasks;
    mgr->timer_queue.cmp = _timer_prio_cmp;
    mgr->timer_queue.iset = _timer_index_set;
    if (jpqueue_init(&mgr->timer_queue) < 0) {
        goto err3;
    }

    /* 创建主线程 */
    attr.stack_size = mgr->stack_size;
    attr.detach_flag = 1;
    if (jthread_create(&mgr->thd, &attr, _thread_main, (void *)mgr) != 0) {
        goto err4;
    }

    return (jpthread_hd)mgr;
err4:
    jpqueue_uninit(&mgr->timer_queue);
err3:
    jpheap_uninit(&mgr->task_pheap);
err2:
    jpheap_uninit(&mgr->thread_pheap);
err1:
    jtimer_uninit(&mgr->ctx) ;
err0:
    jthread_mutex_destroy(&mgr->mtx);
    jheap_free((void *)mgr);
    return NULL;
}

void jpthread_uninit(jpthread_hd hd, int wait_flag)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;

    /* 清除运行状态，通知主线程销毁资源 */
    if (wait_flag > 0) {
        jthread_mutex_lock(&mgr->mtx);
        mgr->running = 0;
        jtimer_wakeup(&mgr->ctx);
        jthread_mutex_unlock(&mgr->mtx);
        while (mgr->thread_pheap.sel + mgr->task_pheap.sel)
            jthread_usleep(1);
    } else {
        if (wait_flag) {
            while (mgr->task_pheap.sel)
                jthread_usleep(1);
        }
        jthread_mutex_lock(&mgr->mtx);
        mgr->running = 0;
        jtimer_wakeup(&mgr->ctx);
        jthread_mutex_unlock(&mgr->mtx);
        if (wait_flag) {
            while (mgr->thread_pheap.sel)
                jthread_usleep(1);
        }
    }
}

jpthread_td jpthread_task_add(jpthread_hd hd, jpthread_cb exec_cb, jpthread_cb free_cb, void *args,
    uint64_t cycle_ns, uint64_t wake_ns)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;
    jpthread_task_t *task = NULL;
    jpthread_td td = {0};
    jtime_nt_t nt = {0};

    if (!exec_cb)
        return td;
    if (cycle_ns || wake_ns)
        jtime_monontime_get(&nt);

next:
    jthread_mutex_lock(&mgr->mtx);

    /* 获取并初始化任务资源 */
    task = (jpthread_task_t *)jpheap_alloc(&mgr->task_pheap);
    if (!task) {
        jthread_mutex_unlock(&mgr->mtx);
        jthread_usleep(1);
        goto next;
    }

    ++mgr->cnt;
    if (!mgr->cnt)
        ++mgr->cnt;
    td.id = mgr->cnt;
    td.ptr = (void *)task;

    task->id = mgr->cnt;
    task->exec_cb = exec_cb;
    task->free_cb = free_cb;
    task->args = args;

    /* 根据不同的传入时间参数区分不同的任务类型 */
    task->cycle_ns = cycle_ns;
    task->wake_nt = nt;
    task->type = JPTHREAD_WORKER;

    if (wake_ns) {
        task->type = cycle_ns ? JPTHREAD_TIMER_REPEAT : JPTHREAD_TIMER_ONCE;
        /* 延迟执行的任务直接加入到优先级队列 */
        task->state = JPTHREAD_IN_QUEUE;
        jtime_ntime_nadd(&task->wake_nt, wake_ns);
        jpqueue_add(&mgr->timer_queue, task);
        if (jpqueue_head(&mgr->timer_queue) == (void *)task)
            jtimer_wakeup(&mgr->ctx);
    } else {
        if (cycle_ns) {
            task->type = JPTHREAD_TIMER_REPEAT;
            jtime_ntime_nadd(&task->wake_nt, cycle_ns);
        }
        task->state = JPTHREAD_IN_LIST;
        jdlist_add_tail(&task->list, &mgr->worker_head);
        ++mgr->pending_workers;
        _thread_wake(mgr);
    }

    jthread_mutex_unlock(&mgr->mtx);
    return td;
}

void jpthread_task_del(jpthread_hd hd, jpthread_td td)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;
    jpthread_task_t *task = (jpthread_task_t *)td.ptr, *first = NULL;
    jpthread_cb free_cb = NULL;
    void *args = NULL;

    /* 任务在执行线程中时由执行线程销毁任务资源，否则直接销毁 */
    jthread_mutex_lock(&mgr->mtx);
    if (task->id && task->id == td.id) {
        switch (task->state) {
        case JPTHREAD_IN_THREAD:
            task->type = JPTHREAD_STOPED;
            break;
        case JPTHREAD_IN_LIST:
            jdlist_del(&task->list);
            --mgr->pending_workers;
            DEL_TASK();
            break;
        case JPTHREAD_IN_QUEUE:
            first = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
            jpqueue_idel(&mgr->timer_queue, task->index);
            if (first == task)
                jtimer_wakeup(&mgr->ctx);
            DEL_TASK();
            break;
        default:
            break;
        }
    }
    jthread_mutex_unlock(&mgr->mtx);

    if (free_cb)
        free_cb(args);
}

int jpthread_task_pause(jpthread_hd hd, jpthread_td td)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;
    jpthread_task_t *task = (jpthread_task_t *)td.ptr, *first = NULL;
    int ret = 0;

    /* 只有运行的重复型任务才能暂停 */
    jthread_mutex_lock(&mgr->mtx);
    if (task->id && task->id == td.id && task->type == JPTHREAD_TIMER_REPEAT) {
        task->type = JPTHREAD_TIMER_PAUSED;
        switch (task->state) {
            case JPTHREAD_IN_LIST:
                task->state = JPTHREAD_IN_QUEUE;
                jdlist_del(&task->list);
                --mgr->pending_workers;
                jpqueue_add(&mgr->timer_queue, task);
                break;
            case JPTHREAD_IN_QUEUE:
                first = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
                jpqueue_idel(&mgr->timer_queue, task->index);
                jpqueue_add(&mgr->timer_queue, task);
                if (first == task)
                    jtimer_wakeup(&mgr->ctx);
                break;
            default:
                break;
        }
    } else {
        ret = -1;
    }
    jthread_mutex_unlock(&mgr->mtx);

    return ret;
}

int jpthread_task_resume(jpthread_hd hd, jpthread_td td, uint64_t cycle_ns, uint64_t wake_ns)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;
    jpthread_task_t *task = (jpthread_task_t *)td.ptr, *first1 = NULL, *first2 = NULL;
    jtime_nt_t nt = {0};
    int ret = 0;

    /* 只有重复型任务才能恢复执行 */
    jtime_monontime_get(&nt);
    jthread_mutex_lock(&mgr->mtx);
    if (task->id && task->id == td.id && (task->type == JPTHREAD_TIMER_PAUSED || task->type == JPTHREAD_TIMER_REPEAT)) {
        first1 = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);

        task->type = JPTHREAD_TIMER_REPEAT;
        task->wake_nt = nt;
        jtime_ntime_nadd(&task->wake_nt, wake_ns);
        if (cycle_ns)
            task->cycle_ns = cycle_ns;

        switch (task->state) {
            case JPTHREAD_IN_LIST:
                task->state = JPTHREAD_IN_QUEUE;
                jdlist_del(&task->list);
                --mgr->pending_workers;
                break;
            case JPTHREAD_IN_QUEUE:
                jpqueue_idel(&mgr->timer_queue, task->index);
                break;
            default:
                goto end;
        }

        jpqueue_add(&mgr->timer_queue, task);
        first2 = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
        if (first1 == task || first2 == task)
            jtimer_wakeup(&mgr->ctx);
    } else {
        ret = -1;
    }

end:
    jthread_mutex_unlock(&mgr->mtx);

    return ret;
}

int jpthread_task_reset(jpthread_hd hd, jpthread_td td, uint64_t cycle_ns, uint64_t wake_ns)
{
    jpthread_mgr_t *mgr = (jpthread_mgr_t *)hd;
    jpthread_task_t *task = (jpthread_task_t *)td.ptr, *first1 = NULL, *first2 = NULL;
    jtime_nt_t nt = {0};
    int ret = 0;

    /* 只有重复型任务才能重设 */
    jtime_monontime_get(&nt);
    jthread_mutex_lock(&mgr->mtx);
    if (task->id && task->id == td.id && (task->type == JPTHREAD_TIMER_PAUSED || task->type == JPTHREAD_TIMER_REPEAT)) {
        if (cycle_ns)
            task->cycle_ns = cycle_ns;

        if (task->state == JPTHREAD_IN_QUEUE) {
            jtime_nt_t wake_nt = nt;
            jtime_ntime_nadd(&wake_nt, wake_ns);
            if (task->type == JPTHREAD_TIMER_PAUSED || _check_expire(&task->wake_nt, &wake_nt)) {
                task->type = JPTHREAD_TIMER_REPEAT;
                task->wake_nt = wake_nt;

                first1 = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
                jpqueue_idel(&mgr->timer_queue, task->index);
                jpqueue_add(&mgr->timer_queue, task);
                first2 = (jpthread_task_t *)jpqueue_head(&mgr->timer_queue);
                if (first1 == task || first2 == task)
                    jtimer_wakeup(&mgr->ctx);
            }
        } else {
            task->type = JPTHREAD_TIMER_REPEAT;
            task->wake_nt = nt;
            jtime_ntime_nadd(&task->wake_nt, wake_ns);
        }
    } else {
        ret = -1;
    }

    jthread_mutex_unlock(&mgr->mtx);

    return ret;
}

