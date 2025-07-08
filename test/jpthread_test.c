/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include "jtime.h"
#include "jheap.h"
#include "jthread.h"
#include "jpthread.h"

static void timer_cb(void *args)
{
    char *str = (char *)args;
    printf("%s: %llu\n", str ? str : __func__, (unsigned long long)jtime_monousec_get());
}

static void free_cb(void *args)
{
    jheap_free(args);
}

static void test_cb(void *args)
{

}

static int test_speed(int max_threads)
{
    int i = 0;
    unsigned long long usec = (unsigned long long)jtime_monousec_get();
    jpthread_hd hd = jpthread_init(max_threads, 1, 65536, 0);

    for (i = 0; i < 10000000; ++i)
        jpthread_worker_add(hd, test_cb, NULL, NULL);

    jpthread_uninit(hd, -1);
    printf("threads=%d, interval=%lld\n", max_threads, jtime_monousec_get() - usec);
    jthread_msleep(1);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return test_speed(atoi(argv[1]));

    int i = 0;
    jpthread_hd hd = jpthread_init(6, 1, 128, 0);
    char *args = NULL;

    printf("add timer_cb\n");
    jpthread_td td = jpthread_task_add(hd, timer_cb, NULL, NULL, 30000000, 0);
    jpthread_td tds[10];
    for (i = 0; i < 10; ++i) {
        args = (char *)jheap_malloc(32);
        snprintf(args, 32, "timer%02d", i);
        printf("add timer%02d\n", i);
        tds[i] = jpthread_task_add(hd, timer_cb, free_cb, args, 10000000, i >> 2);
    }
    for (i = 0; i < 10; ++i) {
        args = (char *)jheap_malloc(32);
        snprintf(args, 32, "worker%02d", i);
        printf("add worker%02d\n", i);
        jpthread_worker_add(hd, timer_cb, free_cb, args);
    }

    jthread_msleep(100);
    jpthread_task_pause(hd, td);
    printf("pause timer_cb\n");
    jthread_msleep(100);
    for (i = 0; i < 10; ++i) {
        jpthread_task_del(hd, tds[i]);
        printf("del timer%02d\n", i);
        if (i == 4) {
            printf("resume timer_cb\n");
            jpthread_task_resume(hd, td, 0, 0);
        }
        jthread_msleep(100);
    }
    jpthread_task_del(hd, td);
    printf("del timer_cb\n");
    jpthread_uninit(hd, 1);
    jthread_msleep(1);

    return 0;
}
