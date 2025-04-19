/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdint.h>
#include "jlog.h"
#include "jsocket.h"

int main(void)
{
#ifndef TEST_BUFSIZE
#define TEST_BUFSIZE    128
#endif
    jsocket_fd_t sfd;
    jsocket_jaddr_t jaddr = {0};
    int len = 0, i = 1 + (1<<20);
    char buf[TEST_BUFSIZE] = {0};

    jaddr.domain = AF_INET;
    len = strlen(JSOCKET_LOCALHOST);
    memcpy(jaddr.addr, JSOCKET_LOCALHOST, len);
    jaddr.addr[len] = '\0';
    jaddr.port = 9999;
    jaddr.msec = 0;

    sfd = jsocket_tcp_client(&jaddr);
    if (!jsocket_fd_valid(sfd)) {
        LLOG_ERROR("jsocket_tcp_client() failed!\n");
        return -1;
    }

    while (--i) {
        len = jsocket_send(sfd, buf, TEST_BUFSIZE);
        if (len < 0) {
            jsocket_close(sfd);
            return -1;
        }
    }
    jsocket_close(sfd);
    return 0;
}
