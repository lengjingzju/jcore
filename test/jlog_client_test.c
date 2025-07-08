/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <signal.h>
#include "jlog.h"
#include "jthread.h"
#include "jini.h"
#include "jheap.h"

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
    jlog_cfg_t cfg = {0};
    void *hd = NULL;
    int i = 0, num = 0;
    int ret = 0;

    // 忽略 SIGPIPE 信号
    signal(SIGPIPE, SIG_IGN);

#if JHEAP_DEBUG
    jheap_init_debug(4);
    jheap_start_debug();
#endif

    memset(buf, '1', TEST_BUFSIZE - 1);

    hd = jini_init(JLOG_CFG_FILE);
    if (!hd) {
        LLOG_ERROR("init cfg failed!\n");
        ret = -1;
        goto end0;
    }

    cfg.buf_size = jini_get_int(hd, "jlog", "buf_size", 1024) << 10;
    cfg.wake_size = jini_get_int(hd, "jlog", "wake_size", 64) << 10;
    cfg.res_size = jini_get_int(hd, "jlog", "res_size", 1024);
    cfg.level = jini_get_int(hd, "jlog", "level", JLOG_LEVEL_INFO);
    cfg.mode = (jlog_mode_t)jini_get_int(hd, "jlog", "mode", JLOG_TO_NET);

    cfg.file.file_size = jini_get_int(hd, "jlog", "file_size", 1024) << 10;
    cfg.file.file_count = jini_get_int(hd, "jlog", "file_count", 10);
    cfg.file.file_path = jini_get(hd, "jlog", "file_path", "jlog");

    cfg.net.is_ipv6 = jini_get_int(hd, "jlog", "is_ipv6", 0);
    cfg.net.ip_port = jini_get_int(hd, "jlog", "ip_port", 9999);
    cfg.net.ip_addr = jini_get(hd, "jlog", "ip_addr", "127.0.0.1");

    cfg.perf.cpu_cycle = jini_get_int(hd, "jlog", "cpu_cycle", 0);
    cfg.perf.mem_cycle = jini_get_int(hd, "jlog", "mem_cycle", 0);
    cfg.perf.net_cycle = jini_get_int(hd, "jlog", "net_cycle", 0);

    jlog_init(&cfg);

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
    jini_uninit(hd);
end0:
#if JHEAP_DEBUG
    jheap_leak_debug(0);
    jheap_leak_debug(3);
    jheap_uninit_debug();
#endif
    return ret;
}

