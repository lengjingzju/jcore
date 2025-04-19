/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "jthread.h"
#include "jrbtree.h"
#include "jheap.h"

#define CHECK_BYTE  0x55

#ifndef CHECK_COUNT
#define CHECK_COUNT  1000   // 检查线程的默认执行时间(CHECK_COUNT * 10ms)
#endif

#ifndef CHECK_FLAG
#define CHECK_FLAG      0   // 检查线程默认是否开启检查
#endif

#ifndef JHOOK_FILE
#define JHOOK_FILE      0   // jhook_check_leak是否输出到文件
#endif
#if JHOOK_FILE
#define PRINT_INFO(fmt, ...)    fprintf(mgr->fp, fmt, ##__VA_ARGS__)
#else
#define PRINT_INFO(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#endif

typedef struct {
    struct jrbtree tree;    // 红黑树成员
    void *ptr;              // 记录分配的指针
    void *node;             // 记录jheap_node_t的指针
} jheap_data_t;

typedef struct {
    size_t size;            // 记录分配内存的大小
    const char *func;       // 记录调用分配内存的函数名
    int line;               // 记录调用分配内存的文件行号
} jheap_nkey_t;

typedef struct {
    struct jrbtree tree;    // 红黑树成员
    size_t size;            // 记录分配内存的大小
    const char *func;       // 记录调用分配内存的函数名
    int line;               // 记录调用分配内存的文件行号
    uint32_t alloc;         // 记录内存分配的次数
    uint32_t free;          // 记录内存释放的次数
    uint32_t changed;       // 记录有内存申请释放的变动
} jheap_node_t;

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
    struct jrbtree_root data_root;  // 挂载jheap_data_t的红黑树根节点
    struct jrbtree_root node_root;  // 挂载jheap_node_t的红黑树根节点
    jthread_mutex_t mtx;    // 互斥锁
    jthread_t       tid;    // 检查线程
#if JHOOK_FILE
    FILE *fp;               // 写入文件流
#endif
} jheap_mgr_t;

static jheap_mgr_t s_jheap_mgr;

static int data_key_cmp(struct jrbtree *node, const void *key)
{
    jheap_data_t *data = jrbtree_entry(node, jheap_data_t, tree);

    if (data->ptr > key)
        return 1;
    if (data->ptr < key)
        return -1;
    return 0;
}

static int data_node_cmp(struct jrbtree *a, struct jrbtree *b)
{
    jheap_data_t *adata = jrbtree_entry(a, jheap_data_t, tree);
    jheap_data_t *bdata = jrbtree_entry(b, jheap_data_t, tree);

    if (adata->ptr > bdata->ptr)
        return 1;
    if (adata->ptr < bdata->ptr)
        return -1;
    return 0;
}

static int node_key_cmp(struct jrbtree *a, const void *key)
{
    jheap_node_t *adata = jrbtree_entry(a, jheap_node_t, tree);
    const jheap_nkey_t *bdata = (const jheap_nkey_t *)key;

    if (adata->size > bdata->size)
        return 1;
    if (adata->size < bdata->size)
        return -1;

    if (adata->func > bdata->func)
        return 1;
    if (adata->func < bdata->func)
        return -1;

    if (adata->line > bdata->line)
        return 1;
    if (adata->line < bdata->line)
        return -1;

    return 0;
}

static int node_node_cmp(struct jrbtree *a, struct jrbtree *b)
{
    jheap_node_t *adata = jrbtree_entry(a, jheap_node_t, tree);
    jheap_node_t *bdata = jrbtree_entry(b, jheap_node_t, tree);

    if (adata->size > bdata->size)
        return 1;
    if (adata->size < bdata->size)
        return -1;

    if (adata->func > bdata->func)
        return 1;
    if (adata->func < bdata->func)
        return -1;

    if (adata->line > bdata->line)
        return 1;
    if (adata->line < bdata->line)
        return -1;

    return 0;
}

static jthread_ret_t jheap_worker(void *arg __attribute__((unused)))
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    int cnt = 0;

    while (mgr->inited) {
        if (mgr->check_flag) {
            if (++cnt >= mgr->check_count) {
                cnt = 0;
                jheap_leak_debug(0);
            }
        }
        jthread_msleep(10);
    }
    return (jthread_ret_t)0;
}

int jheap_init_debug(int tail_num)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    jthread_attr_t attr;

    if (mgr->inited)
        return 0;

    if (tail_num > 256) {
        mgr->tail_num = 256;
    } else {
        mgr->tail_num = tail_num;
    }
    memset(mgr->checks, CHECK_BYTE, sizeof(mgr->checks));

    mgr->data_root.num = 0;
    mgr->data_root.head = NULL;
    mgr->data_root.key_cmp = data_key_cmp;
    mgr->data_root.node_cmp = data_node_cmp;

    mgr->node_root.num = 0;
    mgr->node_root.head = NULL;
    mgr->node_root.key_cmp = node_key_cmp;
    mgr->node_root.node_cmp = node_node_cmp;

    jthread_mutex_init(&mgr->mtx);
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

    memset(&attr, 0, sizeof(attr));
    jthread_create(&mgr->tid, &attr, jheap_worker, NULL);

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

void jheap_uninit_debug(void)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;

    if (!mgr->inited)
        return;

    jheap_stop_debug();
    jthread_mutex_lock(&mgr->mtx);
    mgr->inited = 0;
    mgr->enabled = 0;
    mgr->check_flag = 0;
    jthread_mutex_unlock(&mgr->mtx);
    jthread_mutex_destroy(&mgr->mtx);
    jthread_join(mgr->tid);

#if JHOOK_FILE
    if (mgr->fp) {
        fclose(mgr->fp);
        mgr->fp = NULL;
    }
#endif
}

void jheap_start_debug(void)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;

    if (!mgr->inited)
        return;

    jthread_mutex_lock(&mgr->mtx);
    mgr->enabled = 1;
    jthread_mutex_unlock(&mgr->mtx);
}

void jheap_stop_debug(void)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    struct jrbtree *t = NULL;
    jheap_data_t *data = NULL;
    jheap_node_t *node = NULL;

    if (!mgr->inited)
        return;

    jthread_mutex_lock(&mgr->mtx);
    mgr->enabled = 0;
    mgr->total_size = 0;
    mgr->peak_size = 0;
    while ((t = jrbtree_first(&mgr->data_root))) {
        jrbtree_del(&mgr->data_root, t);
        data = jrbtree_entry(t, jheap_data_t, tree);
        free(data);
    }
    while ((t = jrbtree_first(&mgr->node_root))) {
        jrbtree_del(&mgr->node_root, t);
        node = jrbtree_entry(t, jheap_node_t, tree);
        free(node);
    }
    jthread_mutex_unlock(&mgr->mtx);
}

void jheap_bound_debug(void)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    struct jrbtree *t = NULL;
    jheap_data_t *data = NULL;
    jheap_node_t *node = NULL;

    if (!mgr->inited)
        return;

    jthread_mutex_lock(&mgr->mtx);
    if (!mgr->tail_num)
        goto end;

    for (t = jrbtree_first(&mgr->data_root); t != NULL; t = jrbtree_next(t)) {
        data = jrbtree_entry(t, jheap_data_t, tree);
        node = (jheap_node_t *)data->node;
        if (memcmp(mgr->checks, (char *)data->ptr + node->size, mgr->tail_num) != 0) {
            PRINT_INFO("\033[31mptr(%p size=%u %s:%d) out of bound!\033[0m\n", data->ptr,
                (uint32_t)node->size, node->func, node->line);
        }
    }
end:
    jthread_mutex_unlock(&mgr->mtx);
}

void jheap_leak_debug(int choice)
{
#define PRINT_NODE(node)    PRINT_INFO("%-8u %-8u %-8u %-8u %s:%d\n", \
    (uint32_t)node->size, node->alloc, node->free, node->alloc - node->free, node->func, node->line)

    jheap_mgr_t *mgr = &s_jheap_mgr;
    struct jrbtree *t = NULL;
    jheap_node_t *node = NULL;
    jheap_node_t *nodes = NULL;
    int i = 0, cnt = 0;

    if (!mgr->inited)
        return;

    jthread_mutex_lock(&mgr->mtx);
    if (mgr->node_root.num) {
        nodes = (jheap_node_t *)malloc(mgr->node_root.num * sizeof(jheap_node_t));
        if (nodes) {
            for (t = jrbtree_first(&mgr->node_root); t != NULL; t = jrbtree_next(t)) {
                node = jrbtree_entry(t, jheap_node_t, tree);
                memcpy(nodes + cnt, node, sizeof(jheap_node_t));
                ++cnt;
                node->changed = 0;
            }
        }
    }
    jthread_mutex_unlock(&mgr->mtx);

    if (!nodes)
        return;

    PRINT_INFO("--------------------------------------------------------\n");
    PRINT_INFO("%-8s %-8s %-8s %-8s func:line\n", "size", "alloc", "free", "diff");
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

    free(nodes);
#if JHOOK_FILE
    fflush(mgr->fp);
#endif
}

void jheap_setflag_debug(int dup_flag, int bound_flag)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;

    mgr->dup_flag = dup_flag;
    mgr->bound_flag = bound_flag;
}

void jheap_setcheck_debug(int check_flag, int check_count)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;

    mgr->check_flag = check_flag;
    if (check_count)
        mgr->check_count = check_count;
}

void jheap_setlimit_debug(size_t min_limit, size_t max_limit)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;

    if (!mgr->inited)
        return;

    mgr->min_limit = min_limit;
    mgr->max_limit = max_limit;
}

void jheap_addptr_debug(void *ptr, size_t size, const char *func, int line)
{
#define MAX_BACKTRACE   4

    jheap_mgr_t *mgr = &s_jheap_mgr;
    struct jrbtree *t = NULL;
    jheap_data_t *data = NULL;
    jheap_node_t *node = NULL;
    jheap_nkey_t key = {0, NULL, 0};

    if (!mgr->enabled)
        return;

    if (mgr->bound_flag)
        jheap_bound_debug();

    jthread_mutex_lock(&mgr->mtx);
    if (size < mgr->min_limit || size > mgr->max_limit) {
        jthread_mutex_unlock(&mgr->mtx);
        return;
    }

    if (mgr->tail_num)
        memset((char *)ptr + size, CHECK_BYTE, mgr->tail_num);

    if (mgr->dup_flag) {
        t = jrbtree_search(&mgr->data_root, ptr);
        if (t) {
            jrbtree_del(&mgr->data_root, t);
            data = jrbtree_entry(t, jheap_data_t, tree);
            node = (jheap_node_t *)data->node;
            ++node->free;
            node->changed = 1;
            mgr->total_size -= node->size;
            free(data);
        }
    }

    key.size = size;
    key.func = func;
    key.line = line;

    t = jrbtree_search(&mgr->node_root, &key);
    if (t) {
        node = jrbtree_entry(t, jheap_node_t, tree);
        ++node->alloc;
        node->changed = 1;
        mgr->total_size += node->size;
    } else {
        node = (jheap_node_t *)malloc(sizeof(jheap_node_t));
        if (!node) {
            goto end;
        }
        node->size = size;
        node->func = func;
        node->line = line;
        node->alloc = 1;
        node->free = 0;
        node->changed = 1;
        jrbtree_add(&mgr->node_root, &node->tree);
        mgr->total_size += node->size;
    }

    data = (jheap_data_t *)malloc(sizeof(jheap_data_t));
    if (!data) {
        goto end;
    }
    data->ptr = ptr;
    data->node = (void *)node;
    jrbtree_add(&mgr->data_root, &data->tree);

end:
    if (mgr->peak_size < mgr->total_size)
        mgr->peak_size = mgr->total_size;
    jthread_mutex_unlock(&mgr->mtx);
}

void jheap_delptr_debug(void *ptr, bool del_node, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    struct jrbtree *t = NULL;
    jheap_data_t *data = NULL;
    jheap_node_t *node = NULL;

    if (!ptr)
        return;
    if (!mgr->enabled)
        return;

    jthread_mutex_lock(&mgr->mtx);
    t = jrbtree_search(&mgr->data_root, ptr);
    if (t) {
        jrbtree_del(&mgr->data_root, t);
        data = jrbtree_entry(t, jheap_data_t, tree);
        node = (jheap_node_t *)data->node;
        ++node->free;
        node->changed = 1;
        mgr->total_size -= node->size;
        free(data);
        if (del_node && node->alloc == node->free) {
            jrbtree_del(&mgr->node_root, &node->tree);
            free(node);
        }
    }
    jthread_mutex_unlock(&mgr->mtx);
    if (!t) {
        PRINT_INFO("\033[31mfree ptr(%p %s:%d) not found!\033[0m\n", ptr, func, line);
    }
}

void jheap_free_debug(void *ptr, const char *func, int line)
{
    if (ptr) {
        jheap_delptr_debug(ptr, false, func, line);
        free(ptr);
    }
}

void *jheap_malloc_debug(size_t size, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    void *ptr = malloc(size + mgr->tail_num);

    if (!ptr)
        return NULL;
    jheap_addptr_debug(ptr, size, func, line);
    return ptr;
}

void *jheap_calloc_debug(size_t nmemb, size_t size, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    size_t tsize = nmemb * size;
    void *ptr = malloc(tsize + mgr->tail_num);

    if (!ptr)
        return NULL;
    memset(ptr, 0, tsize);
    jheap_addptr_debug(ptr, tsize, func, line);
    return ptr;
}

void *jheap_realloc_debug(void *ptr, size_t size, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    void *nptr = ptr;
    jheap_delptr_debug(ptr, true, func, line);
    nptr = realloc(ptr, size + mgr->tail_num);
    if (nptr) {
        jheap_addptr_debug(nptr, size, func, line);
    } else {
        jheap_addptr_debug(ptr, size, func, line);
    }
    return nptr;
}

char *jheap_strdup_debug(const char *s, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    size_t size = strlen(s) + 1;
    char *ptr = (char *)malloc(size + mgr->tail_num);

    if (!ptr)
        return NULL;
    memcpy(ptr, s, size - 1);
    ptr[size - 1] = '\0';
    jheap_addptr_debug((void *)ptr, size, func, line);
    return ptr;
}

char *jheap_strndup_debug(const char *s, size_t n, const char *func, int line)
{
    jheap_mgr_t *mgr = &s_jheap_mgr;
    size_t len = strlen(s);
    size_t size = (n < len ? n : len) + 1;
    char *ptr = (char *)malloc(size + mgr->tail_num);

    if (!ptr)
        return NULL;
    memcpy(ptr, s, size - 1);
    ptr[size - 1] = '\0';
    jheap_addptr_debug((void *)ptr, size, func, line);
    return ptr;
}
