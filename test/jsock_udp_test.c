/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jsocket.h"
#include <sys/signal.h>
#define BUF_SIZE 1000

static char buf[BUF_SIZE] = {0};

int main(void)
{
    jsocket_jaddr_t addr = {0}, addr2 = {0};

    signal(SIGPIPE, SIG_IGN);

    addr2.domain = AF_INET;
    addr.msec = 10000;
    addr.domain = AF_INET;
    strcpy(addr.addr, "192.168.224.199");
    addr.port = 10000;
    addr.msec = 0;

    jsocket_fd_t sfd = jsocket_udp_create(&addr2, NULL);

    if (sfd < 0) {
        printf("jsocket_udp_create failed!\n");
        return -1;
    }

    jsocket_send_bufsize_set(sfd, 2 * BUF_SIZE);
    jsocket_recv_bufsize_set(sfd, 2 * BUF_SIZE);

    jsocket_saddr_t saddr;
    if (jsocket_sockaddr_fill(&saddr, &addr) < 0) {
        printf("jsocket_sockaddr_fill failed!\n");
        jsocket_close(sfd);
        return -1;
    }

    int i = 0;
    while (1) {
        ssize_t size = jsocket_sendto(sfd, buf, BUF_SIZE, &saddr);
        printf("%d %lld\n", i++, (long long)size);
        usleep(10000);
    }

    jsocket_close(sfd);
    return 0;
}

