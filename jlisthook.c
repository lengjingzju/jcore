/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <execinfo.h> // only for glibc
#include "jlist.h"

extern void* __libc_malloc(size_t size);
extern void  __libc_free(void *ptr);

void jhook_stop(void);
void jhook_check_leak(int choice);

#define CHECK_BYTE  0x55

#ifndef CHECK_COUNT
#define CHECK_COUNT 1000    // 检查线程的默认执行时间(CHECK_COUNT * 10ms)
#endif

#ifndef CHECK_FLAG
#define CHECK_FLAG  0       // 检查线程默认是否开启检查
#endif

#ifndef JHOOK_FILE
#define JHOOK_FILE  0       // jhook_check_leak是否输出到文件
#endif
#if JHOOK_FILE
#define PRINT_INFO(fmt, ...)    fprintf(mgr->fp, fmt, ##__VA_ARGS__)
#else
#define PRINT_INFO(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#endif

typedef struct {
    struct jslist list;     // 链表成员
    void *ptr;              // 记录分配的指针
} jhook_data_t;

typedef struct {
    struct jslist list;     // 链表成员
    struct jslist_head head;// 挂载jhook_data_t的链表头
    size_t size;            // 记录分配内存的大小
    void *addrs[2];         // 记录堆函数的调用栈(只记2层)
    uint32_t alloc;         // 记录内存分配的次数
    uint32_t free;          // 记录内存释放的次数
    uint32_t changed;       // 记录有内存申请释放的变动
} jhook_node_t;

typedef struct {
    int inited;             // 是否已经初始化了
    int enabled;            // 是否允许记录
    int dup_flag;           // 是否每次分配内存时检查分配的ptr与记录的ptr重复
    int bound_flag;         // 是否每次分配内存时检查记录的ptr有内存越界
    int check_flag;         // 是否允许在检查线程中打印
    int check_count;        // 检查线程的执行时间(check_count * 10ms)
    int method;             // 栈调用记录方式，0: 使用__builtin_return_address记录; 1: 使用backtrace记录
    size_t min_limit;       // 纳入统计的堆内存分配大小下限
    size_t max_limit;       // 纳入统计的堆内存分配大小上限
    size_t total_size;      // 内存总使用大小
    size_t peak_size;       // 内存峰值使用大小
    int tail_num;           // 尾部校验字节数
    char checks[256];       // 尾部校验比较数组
    struct jslist_head head;// 挂载jhook_node_t的链表头
    pthread_mutex_t mtx;    // 互斥锁
    pthread_t       tid;    // 检查线程
#if JHOOK_FILE
    FILE *fp;               // 写入文件流
#endif
} jhook_mgr_t;

static jhook_mgr_t s_jhook_mgr;

static size_t s_jhook_watch_size = 0;
static void jhook_backtrace(size_t size, void *addr)
{
#define MAX_BACKTRACE_LV    100
    jhook_mgr_t *mgr = &s_jhook_mgr;

    if (size == s_jhook_watch_size) {
        mgr->enabled = 0;

        void *addrs[MAX_BACKTRACE_LV] = {NULL};
        int num = backtrace(addrs, MAX_BACKTRACE_LV);
        char **syms = backtrace_symbols(addrs, num); // 获取人类可读的符号，使用完后要free(syms)

        if (syms) {
            int i = 0;
            PRINT_INFO("---------------backtrace : %p---------------\n", addr);
            for (i = 2; i < num; ++i) {
                PRINT_INFO("%s\n", syms[i]);
            }
            free(syms);
        }

        mgr->enabled = 1;
    }
}

static void *jhook_worker(void *arg __attribute__((unused)))
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    int cnt = 0;

    while (mgr->inited) {
        if (mgr->check_flag) {
            if (++cnt >= mgr->check_count) {
                cnt = 0;
                jhook_check_leak(0);
            }
        }
        usleep(10000);
    }
    return (void *)0;
}

int jhook_init(int tail_num)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    pthread_attr_t attr;

    if (mgr->inited)
        return 0;

    if (tail_num > 256) {
        mgr->tail_num = 256;
    } else {
        mgr->tail_num = tail_num;
    }
    memset(mgr->checks, CHECK_BYTE, sizeof(mgr->checks));

    jslist_init_head(&mgr->head);
    pthread_mutex_init(&mgr->mtx, NULL);
    mgr->inited = 1;
    mgr->enabled = 0;
    mgr->dup_flag = 0;
    mgr->bound_flag = 0;
    mgr->check_flag = CHECK_FLAG;
    mgr->check_count = CHECK_COUNT;
    mgr->method = 0;
    mgr->min_limit = 0;
    mgr->max_limit = 1 << 30;
    mgr->total_size = 0;
    mgr->peak_size = 0;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr,1 << 16);
    pthread_create(&mgr->tid, &attr, jhook_worker, NULL);
    pthread_attr_destroy(&attr);

#if JHOOK_FILE
    char fname[128] = {0};
    char buf[1024] = {0};
    int size = 0;
    unsigned long pid = (unsigned long)getpid();

    snprintf(fname, sizeof(fname), "/proc/%lu/cmdline", pid);
    mgr->fp = fopen(fname, "r");
    if (mgr->fp) {
        size = (int)fread(buf, 1, sizeof(buf) - 1, mgr->fp);
        fclose(mgr->fp);
        mgr->fp = NULL;
    }

    snprintf(fname, sizeof(fname), "heap_memory_info.%lu.log", pid);
    mgr->fp = fopen(fname, "a");
    if (!mgr->fp) {
        return -1;
    }
    if (size > 0) {
        PRINT_INFO("%lu:%s\n", pid, buf);
    }
#endif
    return 0;
}

void jhook_uninit(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    if (!mgr->inited)
        return;

    jhook_stop();
    pthread_mutex_lock(&mgr->mtx);
    mgr->inited = 0;
    mgr->enabled = 0;
    mgr->check_flag = 0;
    pthread_mutex_unlock(&mgr->mtx);
    pthread_mutex_destroy(&mgr->mtx);
    pthread_join(mgr->tid, NULL);
#if JHOOK_FILE
    if (mgr->fp) {
        fclose(mgr->fp);
        mgr->fp = NULL;
    }
#endif
}

void jhook_start(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    if (!mgr->inited)
        return;

    pthread_mutex_lock(&mgr->mtx);
    mgr->enabled = 1;
    pthread_mutex_unlock(&mgr->mtx);
}

void jhook_stop(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    jhook_node_t *sp = NULL, *spos = NULL, *sn = NULL;
    jhook_data_t *p = NULL, *pos = NULL, *n = NULL;

    if (!mgr->inited)
        return;

    pthread_mutex_lock(&mgr->mtx);
    mgr->enabled = 0;
    mgr->total_size = 0;
    mgr->peak_size = 0;
    jslist_for_each_entry_safe(sp, spos, sn, &mgr->head, list) {
        jslist_for_each_entry_safe(p, pos, n, &spos->head, list) {
            jslist_del(&pos->list, &p->list, &spos->head);
            __libc_free(pos);
            pos = p;
        }
        jslist_del(&spos->list, &sp->list, &mgr->head);
        __libc_free(spos);
        spos = sp;
    }
    pthread_mutex_unlock(&mgr->mtx);
}

void jhook_check_bound(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    jhook_node_t *node = NULL;
    jhook_data_t *data = NULL;
    int enabled = 0;

    if (!mgr->inited)
        return;

    pthread_mutex_lock(&mgr->mtx);
    if (!mgr->tail_num)
        goto end;

    jslist_for_each_entry(node, &mgr->head, list) {
        jslist_for_each_entry(data, &node->head, list) {
            if (memcmp(mgr->checks, (char *)data->ptr + node->size, mgr->tail_num) != 0) {
                enabled = mgr->enabled;
                mgr->enabled = 0;
                PRINT_INFO("\033[31mptr(%p size=%u addr=%p|%p) out of bound!\033[0m\n", data->ptr,
                    (uint32_t)node->size, node->addrs[0], node->addrs[1]);
                mgr->enabled = enabled;
            }
        }
    }
end:
    pthread_mutex_unlock(&mgr->mtx);
}

void jhook_check_leak(int choice)
{
#define PRINT_NODE(node)        PRINT_INFO("%-8u %-8u %-8u %-8u %p|%p\n", \
    (uint32_t)node->size, node->alloc, node->free, node->alloc - node->free, node->addrs[0], node->addrs[1])

    jhook_mgr_t *mgr = &s_jhook_mgr;
    jhook_node_t *node = NULL;
    jhook_node_t *nodes = NULL;
    int i = 0, cnt = 0, num = 0;

    if (!mgr->inited)
        return;

    pthread_mutex_lock(&mgr->mtx);
    jslist_for_each_entry(node, &mgr->head, list) {
        ++num;
    }

    if (num) {
        nodes = (jhook_node_t *)__libc_malloc(num * sizeof(jhook_node_t));
        if (nodes) {
            jslist_for_each_entry(node, &mgr->head, list) {
                memcpy(nodes + cnt, node, sizeof(jhook_node_t));
                ++cnt;
                node->changed = 0;
            }
        }
    }
    pthread_mutex_unlock(&mgr->mtx);

    if (!nodes)
        return;

    PRINT_INFO("--------------------------------------------------------\n");
    PRINT_INFO("%-8s %-8s %-8s %-8s addr\n", "size", "alloc", "free", "diff");
    switch (choice) {
    case 0:
        for (i = 0; i < cnt; ++i) {
            node = nodes + i;
            if (node->changed && node->alloc != node->free) {
                PRINT_NODE(node);
            }
        }
        break;
    case 1:
        for (i = 0; i < cnt; ++i) {
            node = nodes + i;
            if (node->changed) {
                PRINT_NODE(node);
            }
        }
        break;
    case 2:
        for (i = 0; i < cnt; ++i) {
            node = nodes + i;
            if (node->alloc != node->free) {
                PRINT_NODE(node);
            }
        }
        break;
    case 3:
        for (i = 0; i < cnt; ++i) {
            node = nodes + i;
            PRINT_NODE(node);
        }
        break;
    default:
        if (choice > 0) {
            for (i = 0; i < cnt; ++i) {
                node = nodes + i;
                if ((int)(node->alloc - node->free) >= choice) {
                    PRINT_NODE(node);
                }
            }
        } else {
            for (i = 0; i < cnt; ++i) {
                node = nodes + i;
                if (node->changed && (int)(node->alloc - node->free) >= -choice) {
                    PRINT_NODE(node);
                }
            }
        }
        break;
    }
    PRINT_INFO("----------- total=%-10u peak=%-10u -----------\n", (uint32_t)mgr->total_size, (uint32_t)mgr->peak_size);

    __libc_free(nodes);
#if JHOOK_FILE
    fflush(mgr->fp);
#endif
}

void jhook_set_flag(int dup_flag, int bound_flag)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    mgr->dup_flag = dup_flag;
    mgr->bound_flag = bound_flag;
}

void jhook_set_check(int check_flag, int check_count)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    mgr->check_flag = check_flag;
    if (check_count)
        mgr->check_count = check_count;
}

void jhook_set_method(int method)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    mgr->method = method;
}

void jhook_set_limit(size_t min_limit, size_t max_limit)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;

    if (!mgr->inited)
        return;

    mgr->min_limit = min_limit;
    mgr->max_limit = max_limit;
}

void jhook_addptr(void *ptr, size_t size, void *addr)
{
#define MAX_BACKTRACE   4

    jhook_mgr_t *mgr = &s_jhook_mgr;
    jhook_node_t *node = NULL;
    jhook_data_t *p = NULL, *pos = NULL, *n = NULL;
    void *addrs[MAX_BACKTRACE] = {NULL};

    if (!mgr->enabled)
        return;

    if (mgr->bound_flag)
        jhook_check_bound();

    pthread_mutex_lock(&mgr->mtx);
    if (size < mgr->min_limit || size > mgr->max_limit) {
        pthread_mutex_unlock(&mgr->mtx);
        return;
    }

    if (mgr->method) {
        mgr->enabled = 0;
        backtrace(addrs, MAX_BACKTRACE);
        mgr->enabled = 1;
    } else {
        addrs[2] = addr;
    }
    jhook_backtrace(size, addrs[2]);

    if (mgr->tail_num)
        memset((char *)ptr + size, CHECK_BYTE, mgr->tail_num);

    if (mgr->dup_flag) {
        jslist_for_each_entry(node, &mgr->head, list) {
            jslist_for_each_entry_safe(p, pos, n, &node->head, list) {
                if (pos->ptr == ptr) {
                    jslist_del(&pos->list, &p->list, &node->head);
                    __libc_free(pos);
                    ++node->free;
                    node->changed = 1;
                    mgr->total_size -= node->size;
                    goto next;
                }
            }
        }
    }
next:

    pos = (jhook_data_t *)__libc_malloc(sizeof(jhook_data_t));
    if (!pos) {
        goto end;
    }
    pos->ptr = ptr;

    jslist_for_each_entry(node, &mgr->head, list) {
        if (node->size == size && node->addrs[0] == addrs[2] && node->addrs[1] == addrs[3]) {
            ++node->alloc;
            node->changed = 1;
            jslist_add_head_tail(&pos->list, &node->head);
            mgr->total_size += node->size;
            goto end;
        }
    }

    node = (jhook_node_t *)__libc_malloc(sizeof(jhook_node_t));
    if (!node) {
        __libc_free(pos);
        goto end;
    }
    jslist_init_head(&node->head);
    node->size = size;
    node->addrs[0] = addrs[2];
    node->addrs[1] = addrs[3];
    node->alloc = 1;
    node->free = 0;
    node->changed = 1;
    jslist_add_head_tail(&node->list, &mgr->head);
    jslist_add_head_tail(&pos->list, &node->head);
    mgr->total_size += node->size;

end:
    if (mgr->peak_size < mgr->total_size)
        mgr->peak_size = mgr->total_size;
    pthread_mutex_unlock(&mgr->mtx);
}

void jhook_delptr(void *ptr)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    jhook_node_t *node = NULL;
    jhook_data_t *p = NULL, *pos = NULL, *n = NULL;

    if (!ptr)
        return;
    if (!mgr->enabled)
        return;

    pthread_mutex_lock(&mgr->mtx);
    jslist_for_each_entry(node, &mgr->head, list) {
        jslist_for_each_entry_safe(p, pos, n, &node->head, list) {
            if (pos->ptr == ptr) {
                jslist_del(&pos->list, &p->list, &node->head);
                __libc_free(pos);
                ++node->free;
                node->changed = 1;
                mgr->total_size -= node->size;
                pos = p;
                goto end;
            }
        }
    }
end:
    pthread_mutex_unlock(&mgr->mtx);
}

int jhook_tailnum(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    return mgr->tail_num;
}

#if CHECK_FLAG
void before_main(void) __attribute__((constructor));
void before_main(void)
{
    jhook_mgr_t *mgr = &s_jhook_mgr;
    if (!mgr->inited) {
        jhook_init(0);
        jhook_start();
    }
}

void after_main(void) __attribute__((destructor));
void after_main(void)
{
    jhook_check_leak(2);
}
#endif
