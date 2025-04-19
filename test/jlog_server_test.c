/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdint.h>
#include "jlog.h"
#include "jheap.h"
#include "jthread.h"
#include "jsocket.h"

static jthread_ret_t jlog_serv_run(void *arg)
{
#define RECV_LEN 8192

    jsocket_fd_t afd = *(jsocket_fd_t *)arg;
    char buf[RECV_LEN] = {0};
    int len = 0;
    uint64_t length = 0;

    jheap_free(arg);
    while (1) {
        len = jsocket_recv(afd, buf, RECV_LEN);
        if (len <= 0)
            break;
        length += len;
    }

    LLOG_INFO("socket %d exit: total length = %llu\n", afd, (unsigned long long)length);
    jsocket_close(afd);
    return (jthread_ret_t)0;
}

int main(void)
{
    jthread_t tid;
    jsocket_fd_t sfd, afd;
    jsocket_fd_t *arg = NULL;
    jthread_attr_t attr = {0};
    jsocket_jaddr_t jaddr = {0}, taddr = {0};
    int ret = 0;

#if JHEAP_DEBUG
    jheap_init_debug(4);
    jheap_start_debug();
#endif

    jaddr.domain = AF_INET;
    jaddr.port = 9999;
    jaddr.msec = 0;

    attr.stack_size = 1 << 15;
    attr.detach_flag = 1;

    sfd = jsocket_tcp_server(&jaddr, 1000000);
    if (!jsocket_fd_valid(sfd)) {
        LLOG_ERROR("jsocket_tcp_server() failed!\n");
        ret = -1;
        goto end;
    }

    while (1) {
        afd = jsocket_tcp_accept(sfd, &taddr);
        if (!jsocket_fd_valid(afd)) {
            LLOG_ERROR("jsocket_tcp_accept() failed!\n");
            continue;
        }
        LLOG_INFO("accept %s:%hu as %d.\n", taddr.addr, taddr.port, afd);

        arg = (jsocket_fd_t *)jheap_malloc(sizeof(jsocket_fd_t));
        if (!arg) {
            jsocket_close(afd);
            LLOG_ERROR("malloc failed!\n");
            continue;
        }
        *arg = afd;

        if (jthread_create(&tid, &attr, jlog_serv_run, arg) < 0) {
            jsocket_close(afd);
            jheap_free(arg);
            LLOG_ERROR("jthread_create() failed!\n");
            continue;
        }
    }

    jsocket_close(sfd);

end:
#if JHEAP_DEBUG
    jheap_leak_debug(0);
    jheap_leak_debug(3);
    jheap_uninit_debug();
#endif
    return ret;
}

