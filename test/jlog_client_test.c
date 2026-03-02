/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlog.h"
#include "jthread.h"
#include "jini.h"
#include "jheap.h"
#include "jsocket.h"
#ifndef _WIN32
#include <signal.h>
#endif

#ifndef TEST_THREADS
#define TEST_THREADS    1
#endif
#define TEST_TIMES      10000000
#ifndef TEST_BUFSIZE
#define TEST_BUFSIZE    128
#endif
#define JLOG_CFG_FILE   "jlog.ini"

static char buf[TEST_BUFSIZE] = {0};

static jthread_ret_t jlog_client_run(void *arg)
{
    int i = TEST_TIMES + 1;
    while (--i) {
        jwerror(buf, TEST_BUFSIZE - 1);
    }

    return (jthread_ret_t)0;
}

int main(void)
{
    jthread_t *tids;
    jthread_attr_t attr = {0};
    int i = 0, num = 0;
    int ret = 0;

    jsocket_wsa_init();

#ifndef _WIN32
    // 忽略 SIGPIPE 信号
    signal(SIGPIPE, SIG_IGN);
#endif

#if JHEAP_DEBUG
    jheap_init_debug(4);
    jheap_start_debug();
#endif

    memset(buf, '1', TEST_BUFSIZE - 1);
    jlog_init_ini(JLOG_CFG_FILE);

    if (TEST_THREADS == 1) {
        jlog_client_run(NULL);
    } else {
        attr.stack_size = 1 << 14;
        attr.detach_flag = 0;
        tids = (jthread_t *)jheap_malloc(TEST_THREADS * sizeof(jthread_t));
        if (!tids) {
            LLOG_ERROR("malloc failed!\n");
            ret = -1;
            goto end;
        }

        num = TEST_THREADS;
        for (i = 0; i < num; ++i) {
            if (jthread_create(&tids[i], &attr, jlog_client_run, NULL) != 0) {
                LLOG_ERROR("jthread_create() failed!\n");
                ret = -1;
                break;
            }
        }
        num = i;
        for (i = 0; i < num; ++i) {
            jthread_join(tids[i]);
        }
        jheap_free(tids);
    }

end:
    jlog_uninit();
#if JHEAP_DEBUG
    jheap_leak_debug(0);
    jheap_leak_debug(3);
    jheap_uninit_debug();
#endif
    return ret;
}

