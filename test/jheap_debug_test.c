/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include "jheap.h"
#include "jlog_core.h"

static void test_malloc(int alloc)
{
    static char *ptr = NULL;
    if (alloc) {
        ptr = (char *)jheap_malloc_debug(16, __func__, __LINE__);
    } else {
        jheap_free_debug(ptr, __func__, __LINE__);
    }
}

static void test_calloc(int alloc)
{
    static char *ptr = NULL;
    if (alloc) {
        ptr = (char *)jheap_calloc_debug(2, 9, __func__, __LINE__);
    } else {
        jheap_free_debug(ptr, __func__, __LINE__);

    }
}

static void test_realloc(int alloc)
{
    static char *ptr = NULL;
    if (alloc) {
        ptr = (char *)jheap_malloc_debug(16, __func__, __LINE__);
        ptr = (char *)jheap_realloc_debug(ptr, 32, __func__, __LINE__);
    } else {
        jheap_free_debug(ptr, __func__, __LINE__);
    }
}

static void test_strdup(int alloc)
{
    static char *ptr = NULL;
    if (alloc) {
        ptr = jheap_strdup_debug("Leng Jing", __func__, __LINE__);
    } else {
        jheap_free_debug(ptr, __func__, __LINE__);
    }
}

static void test_strndup(int alloc)
{
    static char *ptr = NULL;
    if (alloc) {
        ptr = jheap_strndup_debug("Leng Jing", 4, __func__, __LINE__);
    } else {
        jheap_free_debug(ptr, __func__, __LINE__);
    }
}

int main(void)
{
    char *ptr = NULL;

    jheap_init_debug(4);
    jheap_start_debug();

    SLOG_INFO("\033[32mprint all heap info:\033[0m\n");
    test_malloc(1);
    test_calloc(1);
    test_realloc(1);
    test_strdup(1);
    test_strndup(1);
    jheap_leak_debug(3);

    // 检测内存泄漏
    SLOG_INFO("\033[32mcheck memory leak:\033[0m\n");
    test_malloc(1);
    jheap_leak_debug(0);
    test_malloc(1);
    jheap_leak_debug(0);
    test_malloc(1);
    jheap_leak_debug(0);

    // 检测内存越界
    SLOG_INFO("\033[32mprint all heap info:\033[0m\n");
    ptr = (char *)jheap_malloc_debug(16, __func__, __LINE__);
    ptr[16] = 0;
    jheap_bound_debug();
    jheap_free_debug(ptr, __func__, __LINE__);

    SLOG_INFO("\033[32mprint all heap info:\033[0m\n");
    test_malloc(0);
    test_calloc(0);
    test_realloc(0);
    test_strdup(0);
    test_strndup(0);
    jheap_leak_debug(3);

    // 检查重复释放
    SLOG_INFO("\033[32mcheck dup free:\033[0m\n");
    test_malloc(0);

    jheap_uninit_debug();

    return 0;
}

