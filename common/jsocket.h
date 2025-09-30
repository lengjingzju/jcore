/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>
#include <BaseTsd.h>     // for SSIZE_T
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define JSOCKET_LOCALHOST               "127.0.0.1"
#define JSOCKET_ANYHOST                 "0.0.0.0"
#define JSOCKET_IPV4_LEN                16
#define JSOCKET_IPV6_LEN                46
#define JSOCKET_IP_LEN                  46
#define AF_INETAUTO                     0

/**
 * @brief   socket可读结构体
 * @note    创建socket时domain一定要设置类型AF_INET或AF_INET6
 *          AF_INETAUTO只用于jsocket_sockaddr_fill自动判断IP类型
 */
typedef struct {
    int domain;                         // IP类型: AF_INETAUTO / AF_INET / AF_INET6 */
    char addr[JSOCKET_IP_LEN];          // IP地址字符串
    unsigned short port;                // IP端口
    int msec;                           // 超时时间
} jsocket_jaddr_t;

/**
 * @brief   socket内部结构体
 */
typedef struct {
    int domain;                         // IP类型
    union addr_in {
        struct sockaddr     sa;         // 统一IP数据
        struct sockaddr_in  sin;        // IPv4数据
        struct sockaddr_in6 sin6;       // IPv6数据
    } s;
} jsocket_saddr_t;

/**
 * @brief   socket描述符操作
 */
#ifdef _WIN32
#ifndef ssize_t
typedef SSIZE_T ssize_t;
#endif
typedef SOCKET jsocket_fd_t;
typedef int jsocket_len_t;
#define JSOCKET_INVALID_FD              INVALID_SOCKET
#define jsocket_close_fd(fd)            closesocket(fd)
#else
typedef int jsocket_fd_t;
typedef socklen_t jsocket_len_t;
#define JSOCKET_INVALID_FD              (-1)
#define jsocket_close_fd(fd)            close(fd)
#endif

/**
 * @brief   判定socket描述符是否有效
 */
#define jsocket_fd_valid(fd)            ((fd) != JSOCKET_INVALID_FD)

/**
 * @brief   关闭socket描述符
 */
#define jsocket_close(fd)               do { if (jsocket_fd_valid(fd)) jsocket_close_fd(fd); (fd) = JSOCKET_INVALID_FD; } while (0)

/**
 * @brief   初始化socket库
 * @param   无参数
 * @return  无返回值
 * @note    只有Windows才需要初始化socket库，Linux该函数为空
 */
void jsocket_wsa_init(void);

/**
 * @brief   反初始化socket库
 * @param   无参数
 * @return  无返回值
 * @note    只有Windows才有此接口，Linux该函数为空
 */
void jsocket_wsa_uninit(void);

/**
 * @brief   设置非阻塞模式
 * @param   sfd [IN] socket描述符
 * @return  成功返回0; 失败返回-1
 * @note    非阻塞模式，接受不到或发送不了数据时会立即返回-1，并且设置errno为EAGAIN
 */
int jsocket_fd_nonblock_set(jsocket_fd_t sfd);

/**
 * @brief   设置发送超时
 * @param   sfd [IN] socket描述符
 * @param   msec [IN] 超时时间毫秒
 * @return  成功返回0; 失败返回-1
 * @note    发送超时影响connect/send/sendto，设置为0表示永不超时
 */
int jsocket_send_timeout_set(jsocket_fd_t sfd, int msec);

/**
 * @brief   设置接收超时
 * @param   sfd [IN] socket描述符
 * @param   msec [IN] 超时时间毫秒
 * @return  成功返回0; 失败返回-1
 * @note    接收超时影响accept/recv/recvfrom，设置为0表示永不超时
 */
int jsocket_recv_timeout_set(jsocket_fd_t sfd, int msec);

/**
 * @brief   设置内核发送缓冲大小
 * @param   sfd [IN] socket描述符
 * @param   size [IN] 要设置的大小
 * @return  成功返回0; 失败返回-1
 * @note    很少调用此接口
 */
int jsocket_send_bufsize_set(jsocket_fd_t sfd, int size);

/**
 * @brief   设置内核接收缓冲大小
 * @param   sfd [IN] socket描述符
 * @param   size [IN] 要设置的大小
 * @return  成功返回0; 失败返回-1
 * @note    很少调用此接口
 */
int jsocket_recv_bufsize_set(jsocket_fd_t sfd, int size);

/**
 * @brief   设置SO_REUSEADDR
 * @param   sfd [IN] socket描述符
 * @return  成功返回0; 失败返回-1
 * @note    建立服务器一般会调用接口，jsocket_tcp_server和jsocket_tcp_server内部已经调用此接口
 *          SO_REUSEADDR作用:
 *          1. 重启服务时，即使以前建立的相同本地端口的连接仍存在(TIME_WAIT)，bind也不出错
 *          2. 允许bind相同的端口，只要ip不同即可
 *          3. 允许udp多播绑定相同的端口和ip
 */
int jsocket_so_reuseaddr_set(jsocket_fd_t sfd);

/**
 * @brief   设置关闭TCP时立即释放资源
 * @param   sfd [IN] socket描述符
 * @return  成功返回0; 失败返回-1
 * @note    主要用于资源有限的嵌入式系统，socket或accept后调用，立即释放资源
 *          TCP主动关闭的一方资源会延时释放有2MSL状态，设置so_linger关闭，主动关闭会立即释放资源
 *          socket或accept后调用，正常关闭时主动关闭的这端有2MSL状态，本接口关闭这个状态
 *          TIME_WAIT状态存在的理由:
 *          1. 可靠地实现TCP全双工连接的终止
 *          2. 允许老的重复分节在网络中消逝
 */
int jsocket_tcp_nolinger_set(jsocket_fd_t sfd);

/**
 * @brief   设置允许TCP小包立即发送
 * @param   sfd [IN] socket描述符
 * @return  成功返回0; 失败返回-1
 * @note    启动TCP_NODELAY，就意味着禁用了Nagle算法，允许小包立即发送
 */
int jsocket_tcp_nodelay_set(jsocket_fd_t sfd);

/**
 * @brief   检查TCP是否还是连接状态
 * @param   sfd [IN] socket描述符
 * @return  连接状态返回0; 已断开返回-1
 * @note    无
 */
int jsocket_tcp_connected_check(jsocket_fd_t sfd);

/**
 * @brief   设置TCP心跳
 * @param   sfd [IN] socket描述符
 * @param   keep_idle [IN] 如该连接在keep_idle秒内没有任何数据往来,则进行探测
 * @param   keep_interval [IN] 探测时发包的时间间隔为keep_interval秒
 * @param   keep_count [IN] 探测尝试的次数keep_count(如果第1次探测包就收到响应了,则后2次的不再发)
 * @return  成功返回0; 失败返回-1
 * @note    启动TCP_NODELAY，就意味着禁用了Nagle算法，允许小包立即发送
 */
int jsocket_tcp_heartbeat_set(jsocket_fd_t sfd, int keep_idle, int keep_interval, int keep_count);

/**
 * @brief   将可读的IP地址和端口转换为socket内部的数据结构体
 * @param   saddr [OUT] socket内部的数据结构体
 * @param   jaddr [IN] 可读的IP地址和端口
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jsocket_sockaddr_fill(jsocket_saddr_t *saddr, const jsocket_jaddr_t *jaddr);

/**
 * @brief   socket内部的数据结构体解析成可读的IP地址和端口
 * @param   saddr [IN] socket内部的数据结构体
 * @param   jaddr [OUT] 可读的IP地址和端口
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jsocket_sockaddr_parse(const jsocket_saddr_t *saddr, jsocket_jaddr_t *jaddr);

/**
 * @brief   比较socket内部的数据结构体是否相等
 * @param   saddr1 [IN] socket内部的数据结构体1
 * @param   saddr2 [IN] socket内部的数据结构体2
 * @return  相等返回1; 不等返回0
 * @note    无
 */
int jsocket_sockaddr_equal(const jsocket_saddr_t *saddr1, const jsocket_saddr_t *saddr2);

/**
 * @brief   获取本端可读的IP地址和端口
 * @param   sfd [IN] socket描述符
 * @param   jaddr [OUT] 可读的IP地址和端口
 * @return  成功返回0; 失败返回-1
 * @note    如果没有绑定IP和端口，UDP返回的端口是0，需要第一次发送接收才会分配端口; TCP返回有值的端口
 *          套接字如果没有绑定IP，返回的IP地址是0.0.0.0
 */
int jsocket_sockaddr_get(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr);

/**
 * @brief   获取对端可读的IP地址和端口
 * @param   sfd [IN] socket描述符
 * @param   jaddr [OUT] 可读的IP地址和端口
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jsocket_peeraddr_get(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr);

/**
 * @brief   加入多播
 * @param   sfd [IN] socket描述符
 * @param   addr [IN] 多播IP地址
 * @param   laddr [IN] 加入多播组的本地网卡IP地址，为NULL时取系统默认值
 * @return  成功返回0; 失败返回-1
 * @note    目前只用于IPv4的多播，D类地址(224.0.0.0-239.255.255.255)为多播地址
 */
int jsocket_udp_broadcast_join(jsocket_fd_t sfd, const char *addr, const char *laddr);

/**
 * @brief   创建UDP套接字
 * @param   jaddr [IN] 绑定的IP地址和端口
 * @param   laddr [IN] 加入多播组的本地网卡IP地址，为NULL时取系统默认值
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    jaddr的addr或port有值时会自动调用bind，是多播地址时会自动加入多播
 */
jsocket_fd_t jsocket_udp_create(const jsocket_jaddr_t *jaddr, const char *laddr);

/**
 * @brief   创建TCP套接字
 * @param   jaddr [IN] 可读的IP地址和端口
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    jaddr的addr或port有值时会自动调用bind
 */
jsocket_fd_t jsocket_tcp_create(const jsocket_jaddr_t *jaddr);

/**
 * @brief   设置TCP监听数目
 * @param   sfd [IN] socket描述符
 * @param   num [IN] 监听的数目
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jsocket_tcp_listen(jsocket_fd_t sfd, int num);

/**
 * @brief   设置TCP监听数目
 * @param   sfd [IN] socket描述符
 * @param   jaddr [OUT] 可读的IP地址和端口，可以为NULL，不为NULL时激励对端的IP地址和端口
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    无
 */
jsocket_fd_t jsocket_tcp_accept(jsocket_fd_t sfd, jsocket_jaddr_t *jaddr);

/**
 * @brief   连接到服务器
 * @param   sfd [IN] socket描述符
 * @param   jaddr [IN] 服务器的可读的IP地址和端口
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jsocket_connect(jsocket_fd_t sfd, const jsocket_jaddr_t *jaddr);

/**
 * @brief   创建UDP服务器
 * @param   jaddr [IN] 服务器可读的IP地址和端口
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    无
 */
jsocket_fd_t jsocket_udp_server(const jsocket_jaddr_t *jaddr);

/**
 * @brief   创建TCP服务器
 * @param   jaddr [IN] 服务器可读的IP地址和端口
 * @param   num [IN] 监听的数目
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    无
 */
jsocket_fd_t jsocket_tcp_server(const jsocket_jaddr_t *jaddr, int num);

/**
 * @brief   创建UDP客户端
 * @param   jaddr [IN] 对端服务器可读的IP地址和端口
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    接口内部已经调用connect连接到服务器
 */
jsocket_fd_t jsocket_udp_client(const jsocket_jaddr_t *jaddr);

/**
 * @brief   创建TCP客户端
 * @param   jaddr [IN] 对端服务器可读的IP地址和端口
 * @return  成功返回socket描述符; 失败返回JSOCKET_INVALID_FD
 * @note    接口内部已经调用connect连接到服务器
 */
jsocket_fd_t jsocket_tcp_client(const jsocket_jaddr_t *jaddr);

/**
 * @brief   发送UDP数据
 * @param   sfd [IN] socket描述符
 * @param   buf [IN] 要发送的buffer
 * @param   blen [IN] 要发送的buffer长度
 * @param   saddr [IN] 对端socket内部的数据结构体
 * @return  成功返回发送的数据长度; 失败返回-1
 * @note    无
 */
ssize_t jsocket_sendto(jsocket_fd_t sfd, const void *buf, ssize_t blen, const jsocket_saddr_t *saddr);

/**
 * @brief   接收UDP数据
 * @param   sfd [IN] socket描述符
 * @param   buf [IN] 接收存储数据的buffer
 * @param   blen [IN] 接收存储数据的buffer长度
 * @param   saddr [OUT] 对端socket内部的数据结构体
 * @return  成功返回接收的数据长度; 失败返回-1
 * @note    无
 */
ssize_t jsocket_recvfrom(jsocket_fd_t sfd, void *buf, size_t blen, jsocket_saddr_t *saddr);

/**
 * @brief   发送数据
 * @param   sfd [IN] socket描述符
 * @param   buf [IN] 要发送的buffer
 * @param   blen [IN] 要发送的buffer长度
 * @return  成功返回发送的数据长度; 失败返回-1
 * @note    socket已connect或accept才能调用此接口
 */
ssize_t jsocket_send(jsocket_fd_t sfd, const void *buf, ssize_t blen);

/**
 * @brief   接收数据
 * @param   sfd [IN] socket描述符
 * @param   buf [IN] 接收存储数据的buffer
 * @param   blen [IN] 接收存储数据的buffer长度
 * @return  成功返回接收的数据长度; 失败返回-1
 * @note    socket已connect或accept才能调用此接口
 */
ssize_t jsocket_recv(jsocket_fd_t sfd, void *buf, size_t blen);

/**
 * @brief   创建临时socket发送UDP数据后关闭
 * @param   jaddr [IN] 对端服务器可读的IP地址和端口
 * @param   buf [IN] 要发送的buffer
 * @param   blen [IN] 要发送的buffer长度
 * @return  成功返回发送的数据长度; 失败返回-1
 * @note    无
 */
ssize_t jsocket_udp_sendmsg(const jsocket_jaddr_t *jaddr, const void *buf, ssize_t blen);

/**
 * @brief   创建临时socket发送TCP数据后关闭
 * @param   jaddr [IN] 对端服务器可读的IP地址和端口
 * @param   buf [IN] 要发送的buffer
 * @param   blen [IN] 要发送的buffer长度
 * @return  成功返回发送的数据长度; 失败返回-1
 * @note    无
 */
ssize_t jsocket_tcp_sendmsg(const jsocket_jaddr_t *jaddr, const void *buf, ssize_t blen);

#ifdef __cplusplus
}
#endif
