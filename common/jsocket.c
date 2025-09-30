/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsocket.h"
#include "jlog.h"

#ifdef _WIN32
#define setsockopt_t            const char *
#else
#define setsockopt_t            const void *
#endif

/* 设置带有print_flag参数的接口是内部jlog使用，防止jlog输出到网络时打印时死锁 */
#define JSOCKET_ERR()           jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d", __func__, __LINE__)
#ifdef _WIN32
#define JSOCKET_ENO()           jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (WSAError=%d)", __func__, __LINE__, WSAGetLastError())
#define JSOCKET_ERRNO(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (WSAError=%d) " fmt, __func__, __LINE__, WSAGetLastError(), ##__VA_ARGS__)
#else
#define JSOCKET_ENO()           jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (%d:%s)", __func__, __LINE__, errno, strerror(errno))
#define JSOCKET_ERRNO(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)
#endif
#define JSOCKET_DEBUG(fmt, ...) jlog_print(JLOG_LEVEL_DEBUG, &g_jcore_mod, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

/* Windows才有的wsa初始化 */
void jsocket_wsa_init(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
}

/* Windows才有的wsa反初始化 */
void jsocket_wsa_uninit(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 设置O_NONBLOCK(非阻塞模式)，接收不到数据时会立即返回-1，并且设置errno为EAGAIN */
int jsocket_fd_nonblock_set(jsocket_fd_t sfd)
{
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sfd, FIONBIO, &mode) != 0) {
        return -1;
    }
    return 0;
#else
    int flags = fcntl(sfd, F_GETFL, 0);
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/* 设置connect/send/sendto的超时 */
int jsocket_send_timeout_set_(jsocket_fd_t sfd, int msec, int print_flag)
{
#ifdef _WIN32
    DWORD timeout = msec;
#else
    struct timeval timeout;
    int sec = msec / 1000;
    timeout.tv_sec = sec;
    timeout.tv_usec = (msec - sec * 1000) * 1000;
#endif

    if (setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, (setsockopt_t)&timeout, sizeof(timeout)) < 0) {
        if (print_flag)
            JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }

    return 0;
}

int jsocket_send_timeout_set(jsocket_fd_t sfd, int msec)
{
    return jsocket_send_timeout_set_(sfd, msec, 1);
}

/* 设置accept/recv/recvfrom的超时 */
int jsocket_recv_timeout_set_(jsocket_fd_t sfd, int msec, int print_flag)
{
#ifdef _WIN32
    DWORD timeout = msec;
#else
    struct timeval timeout;
    int sec = msec / 1000;
    timeout.tv_sec = sec;
    timeout.tv_usec = (msec - sec * 1000) * 1000;
#endif

    if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (setsockopt_t)&timeout, sizeof(timeout)) < 0) {
        if (print_flag)
            JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }

    return 0;
}

int jsocket_recv_timeout_set(jsocket_fd_t sfd, int msec)
{
    return jsocket_recv_timeout_set_(sfd, msec, 1);
}

/* 设置send内核缓冲区的大小 */
int jsocket_send_bufsize_set(jsocket_fd_t sfd, int size)
{
    if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (setsockopt_t)&size, sizeof(size)) < 0) {
        JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }
    return 0;
}

/* 设置recv内核缓冲区的大小 */
int jsocket_recv_bufsize_set(jsocket_fd_t sfd, int size)
{
    if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (setsockopt_t)&size, sizeof(size)) < 0) {
        JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }
    return 0;
}

/*
 * 设置SO_REUSEADDR
 * 作用:
 * 1. 重启服务时，即使以前建立的相同本地端口的连接仍存在(TIME_WAIT)，bind也不出错
 * 2. 允许bind相同的端口，只要ip不同即可
 * 3. 允许udp多播绑定相同的端口和ip
 * TIME_WAIT状态存在的理由:
 * 1. 可靠地实现TCP全双工连接的终止
 * 2. 允许老的重复分节在网络中消逝
 */
int jsocket_so_reuseaddr_set_(jsocket_fd_t sfd, int print_flag)
{
    int optval = 1;

    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (setsockopt_t)&optval, sizeof(optval)) < 0) {
        if (print_flag)
            JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }

    return 0;
}

int jsocket_so_reuseaddr_set(jsocket_fd_t sfd)
{
    return jsocket_so_reuseaddr_set_(sfd, 1);
}

/* TCP主动关闭的一方资源会延时释放，有2MSL状态，设置so_linger，主动关闭会立即释放资源
 * // l_linger, "posix: The unit is 1 sec"; "bsd: The unit is 1 tick(10ms)"
 * l_onoff  l_linger  closesocket行为         发送队列          超时行为
 *   0         /      立即返回                保持直到发送完成  系统接管套接字并保证将数据发送到对端
 *   1         0      立即返回                直接丢弃          直接发送RST包，自身立即复位，跳过TIMEWAIT, 不用经过2MSL状态，对端收到RST错误
 *                                                              (Linux上只有可能只有在丢弃数据时才发送RST)
 *   1         >0     阻塞直到发送完成或超时  超时内超时发送    超时内发送成功正常关闭(FIN-ACK-FIN-ACK四次握手关闭)
 *                    (sfd需要设置为阻塞)     超时后直接丢弃    超时后同RST, 错误号 EWOULDBLOCK
 */
int jsocket_tcp_nolinger_set(jsocket_fd_t sfd)
{
    struct linger so_linger;

    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    if (setsockopt(sfd, SOL_SOCKET, SO_LINGER, (setsockopt_t)&so_linger, sizeof(so_linger)) < 0) {
        JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }

    return 0;
}

/* 启动TCP_NODELAY，就意味着禁用了Nagle算法，允许小包立即发送 */
int jsocket_tcp_nodelay_set(jsocket_fd_t sfd)
{
    int optval = 1;

    if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (setsockopt_t)&optval, sizeof(optval)) < 0) {
        JSOCKET_ERRNO("fd=%d", sfd);
        return -1;
    }

    return 0;
}

/*
 * 判断TCP连接对端是否断开的方法有以下几种:
 * 1. select()测试到sfd可读，但read/recv读取返回的长度为0，并且(errno != EINTR)，对端TCP连接已经断开
 * 2. 向对方send发送数据，返回-1，并且(errno != EAGAIN || errno != EINTR)
 * 3. 判断TCP的状态，如函数jsocket_tcp_connected_check
 * 4. 启用TCP心跳检测
 * 5. 使用自定义超时检测，一段时间没有数据交互就关闭，jsocket_tcp_heartbeat_set
 */
int jsocket_tcp_connected_check(jsocket_fd_t sfd)
{
#ifdef _WIN32
    int error = 0;
    jsocket_len_t len = sizeof(error);

    if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, (char *)&error, &len) != 0) {
        return -1; // getsockopt 调用失败
    }
    if (error != 0) {
        return -1; // 套接字有错误
    }

    // 进一步确认对端是否关闭：尝试 peek 一下
    char buf[1];
    int ret = recv(sfd, buf, 1, MSG_PEEK);
    if (ret == 0) {
        return -1; // 对端关闭
    }
    if (ret < 0) {
        int wsaerr = WSAGetLastError();
        if (wsaerr == WSAEWOULDBLOCK || wsaerr == WSAEINTR || wsaerr == WSAETIMEDOUT) {
            return 0; // 没有数据，但连接还在
        }
        return -1; // 其他错误
    }
    return 0; // 有数据可读，连接正常
#else
    int flag = -1;
    struct tcp_info info;
    jsocket_len_t len = sizeof(info);

    memset(&info, 0, sizeof(info));
    if (getsockopt(sfd, IPPROTO_TCP, TCP_INFO, &info, &len) == 0) {
        flag = (info.tcpi_state == TCP_ESTABLISHED) ? 0 : -1;
    }
    return flag;
#endif
}

/*
 * 启用TCP心跳检测，设置后，若断开，则在使用该socket读写时立即失败，并返回ETIMEDOUT错误
 * keep_idle: 如该连接在keep_idle秒内没有任何数据往来,则进行探测
 * keep_interval: 探测时发包的时间间隔为keep_interval秒
 * keep_count: 探测尝试的次数keep_count(如果第1次探测包就收到响应了,则后2次的不再发)(Windows是固定10次无法修改)
 */
int jsocket_tcp_heartbeat_set(jsocket_fd_t sfd, int keep_idle, int keep_interval, int keep_count)
{
#ifdef _WIN32
    struct tcp_keepalive alive;
    DWORD bytes;

    alive.onoff = 1;
    alive.keepalivetime = keep_idle * 1000;
    alive.keepaliveinterval = keep_interval * 1000;

    if (WSAIoctl(sfd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive),
                 NULL, 0, &bytes, NULL, NULL) == SOCKET_ERROR) {
        return -1;
    }
    return 0;
#else
    int keep_alive = 1; // 开启keepalive属性
    setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keep_alive, sizeof(int));
    setsockopt(sfd, SOL_TCP, TCP_KEEPIDLE, (void *)&keep_idle, sizeof(int));
    setsockopt(sfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keep_interval, sizeof(int));
    setsockopt(sfd, SOL_TCP, TCP_KEEPCNT, (void *)&keep_count, sizeof(int));
    return 0;
#endif
}

/* 从 addr + port 填写sockaddr_xx结构*/
int jsocket_sockaddr_fill_(jsocket_saddr_t *saddr, const jsocket_jaddr_t *jaddr, int print_flag)
{
    switch (jaddr->domain) {
    case AF_INET:
        memset(saddr, 0, sizeof(jsocket_saddr_t));
        saddr->domain = AF_INET;
        saddr->s.sin.sin_family = saddr->domain;
        saddr->s.sin.sin_port = htons(jaddr->port);
        if (!jaddr->addr[0]) {
            saddr->s.sin.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(saddr->domain, jaddr->addr, &saddr->s.sin.sin_addr) != 1) {
                if (print_flag)
                    JSOCKET_ERRNO("addr=%s, port=%hu", jaddr->addr, jaddr->port);
                return -1;
            }
        }
        break;

    case AF_INET6:
        memset(saddr, 0, sizeof(jsocket_saddr_t));
        saddr->domain = AF_INET6;
        saddr->s.sin6.sin6_family = saddr->domain;
        saddr->s.sin6.sin6_port = htons(jaddr->port);
        if (jaddr->addr[0]) {
            if (inet_pton(saddr->domain, jaddr->addr, &saddr->s.sin6.sin6_addr) != 1) {
                saddr->domain = AF_INETAUTO;
                if (print_flag)
                    JSOCKET_ERRNO("addr=%s, port=%hu", jaddr->addr, jaddr->port);
                return -1;
            }
        }
        break;

    case AF_INETAUTO:
        memset(saddr, 0, sizeof(jsocket_saddr_t));
        saddr->domain = AF_INET;
        saddr->s.sin.sin_family = saddr->domain;
        saddr->s.sin.sin_port = htons(jaddr->port);
        if (!jaddr->addr[0]) {
            saddr->s.sin.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(saddr->domain, jaddr->addr, &saddr->s.sin.sin_addr) != 1) {
                memset(saddr, 0, sizeof(jsocket_saddr_t));
                saddr->domain = AF_INET6;
                saddr->s.sin6.sin6_family = saddr->domain;
                saddr->s.sin6.sin6_port = htons(jaddr->port);
                if (inet_pton(saddr->domain, jaddr->addr, &saddr->s.sin6.sin6_addr) != 1) {
                    saddr->domain = AF_INETAUTO;
                    if (print_flag)
                        JSOCKET_ERRNO("addr=%s, port=%hu", jaddr->addr, jaddr->port);
                    return -1;
                }
            }
        }
        break;

    default:
        if (print_flag)
            JSOCKET_ERR();
        return -1;
    }

    return 0;
}

int jsocket_sockaddr_fill(jsocket_saddr_t *saddr, const jsocket_jaddr_t *jaddr)
{
    return jsocket_sockaddr_fill_(saddr, jaddr, 1);
}

int jsocket_sockaddr_parse(const jsocket_saddr_t *saddr, jsocket_jaddr_t *jaddr)
{
    int ret = 0;

    switch (saddr->s.sa.sa_family) {
    case AF_INET:
        jaddr->domain = saddr->s.sa.sa_family;
        jaddr->port = ntohs(saddr->s.sin.sin_port);
        if (!inet_ntop(jaddr->domain, &saddr->s.sin.sin_addr, jaddr->addr, JSOCKET_IPV4_LEN)) {
            ret = -1;
        }
        break;
    case AF_INET6:
        jaddr->domain = saddr->s.sa.sa_family;
        jaddr->port = ntohs(saddr->s.sin6.sin6_port);
        if (!inet_ntop(jaddr->domain, &saddr->s.sin6.sin6_addr, jaddr->addr, JSOCKET_IPV6_LEN)) {
            ret = -1;
        }
        break;
    default:
        jaddr->domain = AF_INETAUTO;
        ret = -1;
        break;
    }

    return ret;
}

int jsocket_sockaddr_equal(const jsocket_saddr_t *saddr1, const jsocket_saddr_t *saddr2)
{
    if (saddr1->s.sa.sa_family != saddr2->s.sa.sa_family)
        return 0;
    switch (saddr1->s.sa.sa_family) {
    case AF_INET:
        return (saddr1->s.sin.sin_port == saddr2->s.sin.sin_port)
            && (saddr1->s.sin.sin_addr.s_addr == saddr2->s.sin.sin_addr.s_addr);
    case AF_INET6:
        return (saddr1->s.sin6.sin6_port == saddr2->s.sin6.sin6_port)
            && (memcmp(&saddr1->s.sin6.sin6_addr, &saddr2->s.sin6.sin6_addr, sizeof(saddr1->s.sin6.sin6_addr)) == 0);
    default:
        return (memcmp(&saddr1->s.sa, &saddr2->s.sa, sizeof(saddr1->s.sa)) == 0);
    }
    return 0;
}

int jsocket_sockaddr_get(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr)
{
    jsocket_saddr_t saddr;
    jsocket_len_t len = sizeof(saddr.s.sa);

    if (getsockname(sfd, &saddr.s.sa, &len)) {
        JSOCKET_ENO();
        return -1;
    }
    return jsocket_sockaddr_parse(&saddr, jaddr);
}

int jsocket_peeraddr_get(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr)
{
    jsocket_saddr_t saddr;
    jsocket_len_t len = sizeof(saddr.s.sa);

    if (getpeername(sfd, &saddr.s.sa, &len)) {
        JSOCKET_ENO();
        return -1;
    }
    return jsocket_sockaddr_parse(&saddr, jaddr);
}

/* UDP加入某个广播组之后就可以向这个广播组发送/接收数据，暂时只对IPv4有效 */
int jsocket_udp_broadcast_join(jsocket_fd_t sfd, const char *addr, const char *laddr)
{
    int optval = 1;
    struct ip_mreq mreq;

    if (setsockopt(sfd, SOL_SOCKET, SO_BROADCAST, (setsockopt_t)&optval, sizeof(optval)) < 0) { // 允许发送广播
        JSOCKET_ERRNO("fd=%d, addr=%s", sfd, addr);
        return -1;
    }

    if (laddr && laddr[0]) {
        if (inet_pton(AF_INET, laddr, &mreq.imr_interface.s_addr) <= 0) { // 本机网卡IP地址
            JSOCKET_ERRNO("fd=%d, laddr=%s", sfd, addr);
            return -1;
        }
    } else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);  // INADDR_ANY=0, 本机任意网卡IP地址
    }

    if (inet_pton(AF_INET, addr, &mreq.imr_multiaddr.s_addr) <= 0) { // 广播组IP地址
        JSOCKET_ERRNO("fd=%d, addr=%s", sfd, addr);
        return -1;
    }

    if (setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (setsockopt_t)&mreq, sizeof(mreq)) < 0) { // 加入广播组
        JSOCKET_ERRNO("fd=%d, addr=%s", sfd, addr);
        return -1;
    }

    return 0;
}

jsocket_fd_t jsocket_udp_create(const jsocket_jaddr_t *jaddr, const char *laddr)
{
    jsocket_fd_t sfd;
    jsocket_saddr_t saddr;

    if (jaddr->domain != AF_INET && jaddr->domain != AF_INET6) {
        JSOCKET_ERRNO("Please set domain to AF_INET or AF_INET6");
        return JSOCKET_INVALID_FD;
    }

    if ((sfd = socket(jaddr->domain, SOCK_DGRAM, IPPROTO_UDP)) == JSOCKET_INVALID_FD) {
        JSOCKET_ENO();
        return JSOCKET_INVALID_FD;
    }

    if ((jaddr->addr[0]) || jaddr->port) {
        if (jsocket_sockaddr_fill(&saddr, jaddr) < 0) {
            JSOCKET_ERR();
            goto err;
        }

        // ipv4 D类地址(224.0.0.0-239.255.255.255)为多播地址
        if (jaddr->addr[0] && atoi(jaddr->addr) >= 224) {
            if (jsocket_udp_broadcast_join(sfd, jaddr->addr, laddr) < 0) {
                JSOCKET_ERR();
                goto err;
            }
            // 嵌入式bsd_tcpip库不能直接绑定多播地址, 需要把ip地址变为0
            // Linux 不必做这一步，并且不做更好
            //saddr.s.sin.sin_addr.s_addr = htonl(INADDR_ANY);
        }

        if (jsocket_so_reuseaddr_set(sfd) < 0) {
            JSOCKET_ERR();
            goto err;
        }
        if (bind(sfd, &saddr.s.sa, sizeof(saddr.s.sa)) < 0) {
            JSOCKET_ERRNO("addr=%s, port=%hu", jaddr->addr, jaddr->port);
            goto err;
        }
        JSOCKET_DEBUG("UDP %d bind on %s:%hu", sfd, jaddr->addr, jaddr->port);
    }

    if (jaddr->msec) {
        jsocket_send_timeout_set(sfd, jaddr->msec);
        jsocket_recv_timeout_set(sfd, jaddr->msec);
    }

    return sfd;
err:
    jsocket_close_fd(sfd);
    return JSOCKET_INVALID_FD;
}

jsocket_fd_t jsocket_tcp_create_(const jsocket_jaddr_t *jaddr, int print_flag)
{
    jsocket_fd_t sfd;
    jsocket_saddr_t saddr;

    if (jaddr->domain != AF_INET && jaddr->domain != AF_INET6) {
        if (print_flag)
            JSOCKET_ERRNO("Please set domain to AF_INET or AF_INET6");
        return JSOCKET_INVALID_FD;
    }

    if ((sfd = socket(jaddr->domain, SOCK_STREAM, IPPROTO_TCP)) == JSOCKET_INVALID_FD) {
        if (print_flag)
            JSOCKET_ENO();
        return JSOCKET_INVALID_FD;
    }

    if ((jaddr->addr[0]) || jaddr->port) {
        if (jsocket_sockaddr_fill_(&saddr, jaddr, print_flag) < 0) {
            if (print_flag)
                JSOCKET_ERR();
            goto err;
        }

        if (jsocket_so_reuseaddr_set_(sfd, print_flag) < 0) {
            if (print_flag)
                JSOCKET_ERR();
            goto err;
        }
        if (bind(sfd, &saddr.s.sa, sizeof(saddr.s.sa)) < 0) {
            if (print_flag)
                JSOCKET_ERRNO("addr=%s, port=%hu", jaddr->addr, jaddr->port);
            goto err;
        }
        if (print_flag)
            JSOCKET_DEBUG("TCP %d bind on %s:%hu", sfd, jaddr->addr, jaddr->port);
    }

    if (jaddr->msec) {
        jsocket_send_timeout_set_(sfd, jaddr->msec, print_flag);
        jsocket_recv_timeout_set_(sfd, jaddr->msec, print_flag);
    }

    return sfd;
err:
    jsocket_close_fd(sfd);
    return JSOCKET_INVALID_FD;
}

jsocket_fd_t jsocket_tcp_create(const jsocket_jaddr_t *jaddr)
{
    return jsocket_tcp_create_(jaddr, 1);
}

int jsocket_tcp_listen(jsocket_fd_t sfd, int num)
{
    if (listen(sfd, num) < 0) {
        JSOCKET_ERRNO("sfd=%d listen%d", sfd, num);
        return -1;
    }
    return 0;
}

jsocket_fd_t jsocket_tcp_accept(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr)
{
    jsocket_fd_t afd;
    jsocket_saddr_t saddr = {0};
    jsocket_jaddr_t taddr = {0};
    jsocket_len_t len = sizeof(saddr.s.sa);

    if ((afd = accept(sfd, &saddr.s.sa, &len)) == JSOCKET_INVALID_FD) {
        JSOCKET_ERRNO("sfd=%d", sfd);
        return JSOCKET_INVALID_FD;
    }

    if (jaddr) {
        if (jaddr->msec) {
            jsocket_send_timeout_set(afd, jaddr->msec);
            jsocket_recv_timeout_set(afd, jaddr->msec);
        }
    } else {
        jaddr = &taddr;
    }
    jsocket_sockaddr_parse(&saddr, jaddr);

    JSOCKET_DEBUG("TCP %d accept %s:%hu as %d", sfd, jaddr->addr, jaddr->port, afd);
    return afd;
}

/*
 * TCP客户端必须调用connect连接到服务器，使用send/recv和服务器交互
 * UDP本端无需调用connect连接到对端，此时使用sendto/recvfrom和对端交互
 * 但UDP本端调用connect连接到对端后，此时可使用send/recv和对端交互
 */
int jsocket_connect(jsocket_fd_t sfd, const jsocket_jaddr_t *jaddr)
{
    jsocket_saddr_t saddr = {0};

    if (jsocket_sockaddr_fill(&saddr, jaddr) < 0) {
        JSOCKET_ERR();
        goto err;
    }
    if (connect(sfd, &saddr.s.sa, sizeof(saddr.s.sa)) < 0) {
        JSOCKET_ENO();
        goto err;
    }

    JSOCKET_DEBUG("SOCK %d connect to %s:%hu", sfd, jaddr->addr, jaddr->port);
    return 0;
err:
    return -1;
}

jsocket_fd_t jsocket_udp_server(const jsocket_jaddr_t *jaddr)
{
    return jsocket_udp_create(jaddr, NULL);
}

jsocket_fd_t jsocket_tcp_server(const jsocket_jaddr_t *jaddr, int num)
{
    jsocket_fd_t sfd;

    sfd = jsocket_tcp_create(jaddr);
    if (sfd == JSOCKET_INVALID_FD) {
        JSOCKET_ERR();
        return JSOCKET_INVALID_FD;
    }

    if (jsocket_tcp_listen(sfd, num) < 0) {
        JSOCKET_ERR();
        goto err;
    }

    return sfd;
err:
    jsocket_close_fd(sfd);
    return JSOCKET_INVALID_FD;
}

jsocket_fd_t jsocket_udp_client(const jsocket_jaddr_t *jaddr)
{
    jsocket_fd_t sfd;
    jsocket_jaddr_t taddr = {0};

    taddr.domain = jaddr->domain;
    taddr.msec = jaddr->msec;
    sfd = jsocket_udp_create(&taddr, NULL);
    if (sfd == JSOCKET_INVALID_FD) {
        JSOCKET_ERR();
        return JSOCKET_INVALID_FD;
    }

    if (jsocket_connect(sfd, jaddr) < 0) {
        JSOCKET_ERR();
        goto err;
    }

    return sfd;
err:
    jsocket_close_fd(sfd);
    return JSOCKET_INVALID_FD;
}

jsocket_fd_t jsocket_tcp_client_(const jsocket_jaddr_t *jaddr, int print_flag)
{
    jsocket_fd_t sfd;
    jsocket_jaddr_t taddr = {0};

    taddr.domain = jaddr->domain;
    taddr.msec = jaddr->msec;
    sfd = jsocket_tcp_create_(&taddr, print_flag);
    if (sfd == JSOCKET_INVALID_FD) {
        if (print_flag)
            JSOCKET_ERR();
        return JSOCKET_INVALID_FD;
    }

    if (jsocket_connect(sfd, jaddr) < 0) {
        if (print_flag)
            JSOCKET_ERR();
        goto err;
    }

    return sfd;
err:
    jsocket_close_fd(sfd);
    return JSOCKET_INVALID_FD;
}

jsocket_fd_t jsocket_tcp_client(const jsocket_jaddr_t *jaddr)
{
    return jsocket_tcp_client_(jaddr, 1);
}

ssize_t jsocket_sendto(jsocket_fd_t sfd, const void *buf, ssize_t blen, const jsocket_saddr_t *saddr)
{
    ssize_t size = 0;
    int flag = 0; // Linux支持MSG_NOSIGNAL，作用是对端已经关闭再写，不产生SIGPIPE信号

#ifdef _WIN32
    size = sendto(sfd, (const char *)buf, (int)blen, flag, &saddr->s.sa, sizeof(saddr->s.sa));
#else
    size = sendto(sfd, buf, blen, flag, &saddr->s.sa, sizeof(saddr->s.sa));
#endif
    if (size != blen) {
        JSOCKET_ERRNO("%ld!=%ld, sendto(%d) failed!", (long)blen, (long)size, sfd);
    }

    return size;
}

ssize_t jsocket_recvfrom(jsocket_fd_t sfd, void *buf, size_t blen, jsocket_saddr_t *saddr)
{
    ssize_t size = 0;
    jsocket_saddr_t taddr;
    jsocket_len_t len = sizeof(saddr->s.sa);

    if (!saddr)
        saddr = &taddr;
#ifdef _WIN32
    size = recvfrom(sfd, (char *)buf, (int)blen, 0, &saddr->s.sa, &len);
#else
    size = recvfrom(sfd, buf, blen, 0, &saddr->s.sa, &len);
#endif
    if (size < 0) {
        JSOCKET_ERRNO("recvfrom(%d) failed!", sfd);
    }

    return size;
}

ssize_t jsocket_send_(jsocket_fd_t sfd, const void *buf, ssize_t blen, int print_flag)
{
    ssize_t size = 0;
    int flag = 0; // Linux支持MSG_NOSIGNAL，作用是对端已经关闭再写，不产生SIGPIPE信号

#ifdef _WIN32
    size = send(sfd, (const char *)buf, (int)blen, flag);
#else
    size = send(sfd, buf, blen, flag);
#endif
    if (size != blen) {
#ifdef _WIN32
        int wsaerr = WSAGetLastError();
        if (size < 0 && (wsaerr == WSAEWOULDBLOCK || wsaerr == WSAEINTR || wsaerr == WSAETIMEDOUT))
            return 0;
#else
        if (size < 0 && (errno == EAGAIN || errno == EINTR || errno == ETIMEDOUT))
            return 0;
#endif
        if (print_flag)
            JSOCKET_ERRNO("%ld!=%ld, send(%d) failed!", (long)blen, (long)size, sfd);
    }

    return size;
}

ssize_t jsocket_send(jsocket_fd_t sfd, const void *buf, ssize_t blen)
{
    return jsocket_send_(sfd, buf, blen, 1);
}

ssize_t jsocket_recv(jsocket_fd_t sfd, void *buf, size_t blen)
{
    ssize_t size = 0;
#ifdef _WIN32
    size = recv(sfd, (char *)buf, (int)blen, 0);
#else
    size = recv(sfd, buf, blen, 0);
#endif
    if (size < 0) {
        JSOCKET_ERRNO("recv(%d) failed!", sfd);
    }

    return size;
}

ssize_t jsocket_udp_sendmsg(const jsocket_jaddr_t *jaddr, const void *buf, ssize_t blen)
{
    ssize_t ret;
    jsocket_fd_t sfd;
    jsocket_saddr_t saddr;
    jsocket_jaddr_t taddr = {0};

    taddr.domain = jaddr->domain;
    taddr.msec = jaddr->msec;
    sfd = jsocket_udp_create(&taddr, NULL);
    if (sfd == JSOCKET_INVALID_FD) {
        JSOCKET_ERR();
        return -1;
    }

    if (jsocket_sockaddr_fill(&saddr, jaddr) < 0) {
        JSOCKET_ERR();
        goto err;
    }

    ret = jsocket_sendto(sfd, buf, blen, &saddr);
    jsocket_close_fd(sfd);
    return ret;
err:
    jsocket_close_fd(sfd);
    return -1;
}

ssize_t jsocket_tcp_sendmsg(const jsocket_jaddr_t *jaddr, const void *buf, ssize_t blen)
{
    ssize_t ret;
    jsocket_fd_t sfd;

    sfd = jsocket_tcp_client(jaddr);
    if (sfd == JSOCKET_INVALID_FD) {
        JSOCKET_ERR();
        return -1;
    }

    ret = jsocket_send(sfd, buf, blen);
    jsocket_close_fd(sfd);
    return ret;
}

