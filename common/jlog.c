/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdio.h>
#include <stdlib.h>
#include "jlog.h"
#include "jtime.h"
#include "jthread.h"
#include "jfs.h"
#include "jsocket.h"
#include "jheap.h"
#include "jperf.h"

extern ssize_t jsocket_send_(jsocket_fd_t sfd, const void *buf, ssize_t blen, int print_flag);
extern jsocket_fd_t jsocket_tcp_client_(const jsocket_jaddr_t *jaddr, int print_flag);
extern int jfs_mkdir_(const char *dname, int print_flag);
extern int jfs_listdir_(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter, int print_flag);

#define PATH_MAX_LEN        4096        // 路径最大长度-1
#define JLOG_SLEEP_MS       1000        // 写线程睡眠时间毫秒
#define JLOG_STACK_SIZE     (64 << 10)  // 默认线程栈大小
#define JLOG_BUF_SIZE       (1 << 19)   // 缓冲大小
#define JLOG_BUF_FACTOR     (4)         // 保留缓冲，写入不及时时直接删除旧缓冲大小为(JLOG_BUF_SIZE >> JLOG_BUF_FACTOR)
#define JLOG_WAKE_SIZE      (128 << 10) // 唤醒写线程的缓冲默认阈值
#define JLOG_RES_SIZE       1024        // 日志缓冲保留默认大小，即一次写log的最大长度

#define JLOG_DEF_FSIZE      (1 << 20)   // 写文件时的默认文件大小
#define JLOG_DEF_FCOUNT     10          // 写文件时的默认文件限制数量
#define JLOG_DEF_FPATH      "jlog"      // 写文件时的默认文件夹路径
#define JLOG_DEF_IPADDR     JSOCKET_LOCALHOST // 连接的日志服务器的默认IP
#define JLOG_DEF_IPPORT     9999        // 连接的日志服务器的默认端口

#define JLOG_TS_SIZE        23          // log时间戳的长度
#define JLOG_WRITE_SIZE     (64 << 10)  // 写文件时单次写大小
#define JLOG_SEND_SIZE      (64 << 10)  // 发送网络时时单次发送大小
#define RESTATDIR_SEC       30          // 重新检查文件系统的时间
#define HEARTBEAT_SEC       30          // 发送到网络的心跳时间
#define RECONNECT_SEC       30          // 发送到网络的重连时间

#ifndef JLOG_TIMESTAMP
#define JLOG_TIMESTAMP      1           // 写入日志时是否带时间戳
#endif
#if JLOG_TIMESTAMP
#define JLOG_HEAD_SIZE      30          // log格式头的长度 JLOG_TS_SIZE + 7，不含module和type
#else
#define JLOG_HEAD_SIZE      6
#endif

typedef struct {
    int size;               // buffer大小
    int wake;               // buffer中有多长日志时唤醒写入线程
    int res;                // 日志缓冲保留默认大小，决定一次写log的最大长度
    int widx;               // 写入开始位置
    int ridx;               // 读取开始位置
    int tail;               // 快达到buffer结尾时不再写入的位置
    int loses;              // 丢缓冲的次数
    int truncs;             // 可能截断的次数
    char *buf;              // buffer指针
#if JLOG_TIMESTAMP
    jtime_t tsec;           // 时间戳
    char tbuf[JLOG_TS_SIZE + 1]; // 时间戳字符串
#endif
} jlog_jbuf_t;

typedef struct {
#define LOGFILE_STRLEN      29
    char name[LOGFILE_STRLEN + 1];
} jlog_fname_t;

typedef struct {
    int fsize;              // 每个log文件的最大大小
    int fcount;             // 最多多少个文件
    int size;               // 当前文件大小
    int dlen;               // 文件夹字符串长度
    jtime_t last_check;     // 上次检查文件输出端是否可用的时间戳
    jfs_fd_t fd;            // 文件描述符
    char path[PATH_MAX_LEN];// 文件存储目录
    jlog_fname_t *fnames;   // 记录log文件名列表
    int fnum;               // 记录log文件名的数量
    int fcnt;               // 当前最早的log文件的位置
} jlog_fcfg_t;

typedef struct {
    jsocket_fd_t fd;        // Socket描述符
    jtime_t last_check;     // 上次检查服务端是否可用的时间戳
    jtime_t last_send;      // 上次发送的时间戳
    jsocket_jaddr_t jaddr;  // 服务器IP地址和端口
} jlog_ncfg_t;

typedef struct {
    int level;              // 打印等级
    int zone_sec;           // 时区偏移秒数
    int changed;            // 参数有变化
    jlog_mode_t mode;       // 输出方式
    jlog_mode_t wanted;     // 将要设置的输出方式
    jlog_fcfg_t fcfg;       // 文件输出配置
    jlog_ncfg_t ncfg;       // 网络输出配置
    jlog_perf_t perf;       // 采集系统信息的配置
} jlog_jcfg_t;

typedef struct {
    int inited;             // 是否初始化了
    jlog_jbuf_t jbuf;       // 缓冲区
    jlog_jcfg_t jcfg;       // 输出配置

    jthread_t tid;          // 线程id
    jthread_mutex_t cmtx;   // 配置互斥锁
    jthread_mutex_t mtx;    // 缓冲互斥锁
    jthread_cond_t cond;    // 缓冲条件变量
} jlog_mgr_t;

static jlog_mgr_t g_jlog_mgr;
static const char g_level_str[] = "OFEWIDT";
static const jlog_str_t g_jlog_none = {"N", 1};
static const jlog_str_t g_jlog_mod = {"MOD", 3};
static const jlog_str_t g_of_type = {"OVERFLOW", 8};
static const jlog_str_t g_hb_type = {"HEARTBEAT", 9};
static const jlog_str_t g_perf_type = {"PERF", 4};
const jlog_str_t g_jcore_mod = {"jcore", 5};

#if JLOG_TIMESTAMP
static const char ch_100_lut[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
};
#define FAST_DIV100(n)      (((n) * 5243) >> 19)                            /* 0 <= n < 10000 */

static void jlog_head_timestamp(char *tbuf)
{
    char *p = tbuf;
    int t1, t2;

    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jtime_tm_t tm = {0};

    jtime_utctime_get(&tm, jcfg->zone_sec);

    t1 = FAST_DIV100(tm.year);
    t2 = tm.year - t1 * 100;
    memcpy(p, ch_100_lut + (t1 << 1), 2);
    memcpy(p + 2, ch_100_lut + (t2 << 1), 2);
    p += 4;
    *p++ = '-';

    memcpy(p, ch_100_lut + (tm.month << 1), 2);
    p += 2;
    *p++ = '-';

    memcpy(p, ch_100_lut + (tm.day << 1), 2);
    p += 2;
    *p++ = ' ';

    memcpy(p, ch_100_lut + (tm.hour << 1), 2);
    p += 2;
    *p++ = ':';

    memcpy(p, ch_100_lut + (tm.min << 1), 2);
    p += 2;
    *p++ = ':';

    memcpy(p, ch_100_lut + (tm.sec << 1), 2);
    p += 2;
    *p++ = '.';

    t1 = FAST_DIV100(tm.msec);
    t2 = tm.msec - t1 * 100;
    *p = t1 + '0';
    memcpy(p + 1, ch_100_lut + (t2 << 1), 2);
    p += 3;
}
#endif

static int jlog_head(const char *tbuf, int level, const jlog_str_t *module, const jlog_str_t *type, char *buf, int len)
{
    char *p = buf;

    if (!module)
        module = &g_jlog_none;
    if (!type)
        type = &g_jlog_none;

    if (len <= JLOG_HEAD_SIZE + module->len + type->len) {
        module = &g_jlog_mod;
        type = &g_jlog_mod;
    }

    *p++ = '[';
#if JLOG_TIMESTAMP
    if (tbuf) {
        memcpy(p, tbuf, JLOG_TS_SIZE);
    } else {
        jlog_head_timestamp(p);
    }
    p += JLOG_TS_SIZE;
    *p++ = ' ';
#endif

    *p++ = g_level_str[level];
    *p++ = ' ';

    memcpy(p, module->str, module->len);
    p += module->len;
    *p++ = ' ';

    memcpy(p, type->str, type->len);
    p += type->len;
    *p++ = ']';
    *p++ = ' ';

    return p - buf;
}

static inline int _jlog_wsize_get(jlog_jbuf_t *jbuf)
{
    int len;
    if (jbuf->widx > jbuf->ridx) {
         len = jbuf->size - jbuf->widx;
    } else if (jbuf->widx < jbuf->ridx) {
         len = jbuf->ridx - jbuf->widx;
    } else {
         len = jbuf->tail ? 0 : jbuf->size - jbuf->widx;
    }
    return len;
}

static inline int _jlog_rsize_get(jlog_jbuf_t *jbuf)
{
    int len;
    if (jbuf->widx > jbuf->ridx) {
        len = jbuf->widx - jbuf->ridx;
    } else if (jbuf->widx < jbuf->ridx) {
        len = jbuf->tail - jbuf->ridx + jbuf->widx;
    } else {
        len = jbuf->tail;
    }
    return len;
}

static inline void _jlog_widx_update(jlog_jbuf_t *jbuf, int len)
{
    if (jbuf->widx >= jbuf->ridx) {
        jbuf->widx += len;
        if (jbuf->size - jbuf->widx < jbuf->res) {
            jbuf->tail = jbuf->widx;
            jbuf->widx = 0;
        }
    } else {
        jbuf->widx += len;
    }
}

#if JLOG_TIMESTAMP
static void _jlog_tbuf_update(const jtime_mt_t *mt)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    char *p = jbuf->tbuf;
    jtime_tm_t tm = {0};
    int t1, t2;
    jtime_t t = 0;

    t = mt->sec + jcfg->zone_sec - jbuf->tsec;
    if (t >= 0 && t < 60) {
        p += 17;
    } else {
        jtime_mtime_to_tm(mt, &tm, jcfg->zone_sec);
        jbuf->tsec = mt->sec + jcfg->zone_sec - tm.sec;
        t = tm.sec;

        t1 = FAST_DIV100(tm.year);
        t2 = tm.year - t1 * 100;
        memcpy(p, ch_100_lut + (t1 << 1), 2);
        memcpy(p + 2, ch_100_lut + (t2 << 1), 2);
        p += 4;
        *p++ = '-';

        memcpy(p, ch_100_lut + (tm.month << 1), 2);
        p += 2;
        *p++ = '-';

        memcpy(p, ch_100_lut + (tm.day << 1), 2);
        p += 2;
        *p++ = ' ';

        memcpy(p, ch_100_lut + (tm.hour << 1), 2);
        p += 2;
        *p++ = ':';

        memcpy(p, ch_100_lut + (tm.min << 1), 2);
        p += 2;
        *p++ = ':';
    }

    memcpy(p, ch_100_lut + (t << 1), 2);
    p += 2;
    *p++ = '.';

    t1 = FAST_DIV100(mt->msec);
    t2 = mt->msec - t1 * 100;
    *p = t1 + '0';
    memcpy(p + 1, ch_100_lut + (t2 << 1), 2);
    p += 3;
}
#endif

int jlog_vprint(int level, const jlog_str_t *module, const jlog_str_t *type, const char *fmt, va_list ap)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    int len = 0, len1 = 0, len2 = 0, rlen = 0;

    if (level > mgr->jcfg.level)
        return 0;

    if (!mgr->inited) {
        return 0;
    }

#if JLOG_TIMESTAMP
    jtime_mt_t mt = {0};
    jtime_utcmtime_geta(&mt);
#endif

next:
    jthread_mutex_lock(&mgr->mtx);
    if (!mgr->inited) {
        jthread_mutex_unlock(&mgr->mtx);
        len2 = vprintf(fmt, ap);
        return len2;
    }

    len = _jlog_wsize_get(jbuf);
    if (len < jbuf->res) {
        jthread_mutex_unlock(&mgr->mtx);
        jthread_cond_signal(&mgr->cond);
        goto next;
    } else {
#if JLOG_TIMESTAMP
        _jlog_tbuf_update(&mt);
        len1 = jlog_head(jbuf->tbuf, level, module, type, jbuf->buf + jbuf->widx, len);
#else
        len1 = jlog_head(NULL, level, module, type, jbuf->buf + jbuf->widx, len);
#endif
        len2 = vsnprintf(jbuf->buf + jbuf->widx + len1, len - len1, fmt, ap);
        jbuf->buf[jbuf->widx + len1 + len2] = '\n';
        if (len1 + len2 + 1 == len) {
            /* 打印可能用完了空闲的buf，日志可能被截断，设置日志截断状态 */
            ++jbuf->truncs;
        }
        _jlog_widx_update(jbuf, len1 + len2 + 1);
    }

    rlen = _jlog_rsize_get(jbuf);
    jthread_mutex_unlock(&mgr->mtx);

    if (rlen >= jbuf->wake)
        jthread_cond_signal(&mgr->cond);

    return len2;
}

int jlog_print(int level, const jlog_str_t *module, const jlog_str_t *type, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = jlog_vprint(level, module, type, fmt, ap);
    va_end(ap);
    return ret;
}

int jlog_write(int level, const jlog_str_t *module, const jlog_str_t *type, const char *buf, int count)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    int len = 0, len1 = 0, len2 = 0, rlen = 0;

    if (level > mgr->jcfg.level)
        return 0;

    if (!mgr->inited) {
        return 0;
    }

#if JLOG_TIMESTAMP
    jtime_mt_t mt = {0};
    jtime_utcmtime_geta(&mt);
#endif

next:
    jthread_mutex_lock(&mgr->mtx);
    if (!mgr->inited) {
        jthread_mutex_unlock(&mgr->mtx);
        return 0;
    }

    len = _jlog_wsize_get(jbuf);
    if (len < jbuf->res) {
        jthread_mutex_unlock(&mgr->mtx);
        jthread_cond_signal(&mgr->cond);
        goto next;
    } else {
#if JLOG_TIMESTAMP
        _jlog_tbuf_update(&mt);
        len1 = jlog_head(jbuf->tbuf, level, module, type, jbuf->buf + jbuf->widx, len);
#else
        len1 = jlog_head(NULL, level, module, type, jbuf->buf + jbuf->widx, len);
#endif
        len2 = count <= len - len1 - 1 ? count : len - len1 - 1;
        memcpy(jbuf->buf + jbuf->widx + len1, buf, len2);
        jbuf->buf[jbuf->widx + len1 + len2] = '\n';
        if (len2 < count) {
            /* 打印用完了空闲的buf，日志可能被截断，设置日志截断状态 */
            ++jbuf->truncs;
        }
        _jlog_widx_update(jbuf, len1 + len2 + 1);
    }

    rlen = _jlog_rsize_get(jbuf);
    jthread_mutex_unlock(&mgr->mtx);

    if (rlen >= jbuf->wake)
        jthread_cond_signal(&mgr->cond);

    return len2;
}

static char *jlog_buf_get(int *len)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    char *buf = NULL;

    jthread_mutex_lock(&mgr->mtx);
    if (jbuf->widx > jbuf->ridx) {
         buf = jbuf->buf + jbuf->ridx;
         *len = jbuf->widx - jbuf->ridx;
    } else if (jbuf->widx < jbuf->ridx) {
         buf = jbuf->buf + jbuf->ridx;
         *len = jbuf->tail - jbuf->ridx;
    } else {
        if (jbuf->tail) {
            buf = jbuf->buf + jbuf->ridx;
            *len = jbuf->tail - jbuf->ridx;
        } else {
            *len = 0;
        }
    }
    jthread_mutex_unlock(&mgr->mtx);

    return buf;
}

static void jlog_buf_set(int len)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;

    jthread_mutex_lock(&mgr->mtx);
    jbuf->ridx += len;
    if (jbuf->ridx == jbuf->tail) {
        jbuf->ridx = 0;
        jbuf->tail = 0;
    }
    jthread_mutex_unlock(&mgr->mtx);
}

static void jlog_buf_abandon(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    int len = 0, tmp = 0, rev = 0, max = 0;

    jthread_mutex_lock(&mgr->mtx);
    rev = jbuf->size >> JLOG_BUF_FACTOR;
    max = jbuf->size - (rev >> 1);

    if (jbuf->widx > jbuf->ridx) {
        len = jbuf->widx - jbuf->ridx;
        if (len >= max) {
            jbuf->ridx += rev;
            ++jbuf->loses;
        }
    } else {
        len = jbuf->widx < jbuf->ridx ? jbuf->tail - jbuf->ridx + jbuf->widx : jbuf->tail;
        if (len >= max) {
            if (jbuf->ridx + rev < jbuf->tail) {
                jbuf->ridx += rev;
            } else {
                tmp = jbuf->size - jbuf->ridx;
                jbuf->ridx = tmp >= rev ? 0 : rev - tmp;
                jbuf->tail = 0;
            }
            ++jbuf->loses;
        }
    }
    jthread_mutex_unlock(&mgr->mtx);
}

static int jlog_check_overflow(char *ebuf, int elen)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    int loses = 0, truncs = 0, len = 0;

    jthread_mutex_lock(&mgr->mtx);
    loses = jbuf->loses;
    truncs = jbuf->truncs;
    jbuf->loses = 0;
    jbuf->truncs = 0;
    jthread_mutex_unlock(&mgr->mtx);

    if (loses + truncs) {
        jcfg->zone_sec = jtime_localutc_diff();
        len = jlog_head(NULL, JLOG_LEVEL_WARN, &g_jlog_none, &g_of_type, ebuf, elen);
        len += snprintf(ebuf + len, elen - len, "{\"lose\":%d,\"truncate\":%d}\n", loses, truncs);
        JFS_WRERR(ebuf, len);
    }
    return len;
}

static char *jlog_check_performance(int *len)
{
#define JLOG_PERF_LEN   2048
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_perf_t *perf = &mgr->jcfg.perf;

    static char buf[JLOG_PERF_LEN] = {0};
    static jperf_cpu_usage_t last_self_cpu_usage = {0}, last_sys_cpu_usage = {0};
    static jperf_net_usage_t last_net_usage = {0};
    jperf_mem_usage_t self_mem_usage = {0}, sys_mem_usage = {0};
    static uint64_t last_cpu_msec = 0, last_mem_msec = 0, last_net_msec = 0;
    uint64_t sys_msec = 0;
    int total = 0, cnt = 0, i = 0;

    *len = 0;
    if (!(perf->cpu_cycle + perf->mem_cycle + perf->net_cycle))
        return NULL;

    sys_msec = jtime_utcmsec_geta();

    if (perf->cpu_cycle && sys_msec >= perf->cpu_cycle * 1000 + last_cpu_msec) {
        last_cpu_msec = sys_msec;
        total = jlog_head(NULL, JLOG_LEVEL_INFO, &g_jlog_none, &g_perf_type, buf, JLOG_PERF_LEN);
        buf[total++] = '{';

        total += snprintf(buf + total, JLOG_PERF_LEN - total,
            "\"cpu\":{\"self\":%d,\"system\":%d}",
            jperf_process_cpu_usage(&last_self_cpu_usage), jperf_system_cpu_usage(&last_sys_cpu_usage));
        ++cnt;
    }

    if (perf->mem_cycle && sys_msec >= perf->mem_cycle * 1000 + last_mem_msec) {
        last_mem_msec = sys_msec;
        if (cnt) {
            buf[total++] = ',';
        } else {
            total = jlog_head(NULL, JLOG_LEVEL_INFO, &g_jlog_none, &g_perf_type, buf, JLOG_PERF_LEN);
            buf[total++] = '{';
        }
        jperf_process_mem_usage(&self_mem_usage);
        jperf_system_mem_usage(&sys_mem_usage);

        total += snprintf(buf + total, JLOG_PERF_LEN - total,
            "\"memory\":{\"self\":{\"virtual\":%llu,\"physical\":%llu},\"system\":{\"virtual\":%llu,\"physical\":%llu,\"total\":%llu}}",
            (unsigned long long)self_mem_usage.vir_size, (unsigned long long)self_mem_usage.phy_size,
            (unsigned long long)sys_mem_usage.vir_size, (unsigned long long)sys_mem_usage.phy_size,
            (unsigned long long)sys_mem_usage.total_size);
        ++cnt;
    }

    if (perf->net_cycle && sys_msec >= perf->net_cycle * 1000 + last_net_msec) {
        last_net_msec = sys_msec;
        if (cnt) {
            buf[total++] = ',';
        } else {
            total = jlog_head(NULL, JLOG_LEVEL_INFO, &g_jlog_none, &g_perf_type, buf, JLOG_PERF_LEN);
            buf[total++] = '{';
        }
        jperf_system_net_usage(&last_net_usage);
        memcpy(buf + total, "\"network\":[", 11);
        total += 11;
        for (i = 0; i < last_net_usage.num; ++i) {
            if (i)
                buf[total++] = ',';
            total += snprintf(buf + total, JLOG_PERF_LEN - total,
                "{\"name\":\"%s\",\"recv\":%llu,\"send\":%llu}", last_net_usage.datas[i].name,
                (unsigned long long)last_net_usage.datas[i].rx_speed, (unsigned long long)last_net_usage.datas[i].tx_speed);
        }
        buf[total++] = ']';
        ++cnt;
    }

    if (cnt) {
        buf[total++] = '}';
        buf[total++] = '\n';

        *len = total;
        return buf;
    }

    *len = 0;
    return NULL;
}

static int jlog_filter_file(const char *fname, unsigned int flen, unsigned int ftype)
{
#define LOGFILE_SUFFIX      "_j.log"
    return (flen == LOGFILE_STRLEN && strcmp(fname + JLOG_TS_SIZE, LOGFILE_SUFFIX) == 0);
}

static void jlog_free_file(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_fcfg_t *fcfg = &mgr->jcfg.fcfg;

    if (fcfg->fnames)
        jheap_free(fcfg->fnames);
    fcfg->fnames = NULL;
    fcfg->fnum = 0;
    fcfg->fcnt = 0;
}

static int jlog_new_file(void)
{
#define LOGFILE_FORMAT      "%04hu-%02hhu-%02hhu_%02hhu-%02hhu-%02hhu-%03hu_j.log"
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_fcfg_t *fcfg = &jcfg->fcfg;
    jfs_dirent_t *dirs = NULL;
    int num = 0, fmax = 0, i = 0, cnt = 0;
    int dlen = 0;
    char path[PATH_MAX_LEN];// 文件存储目录
    jtime_tm_t tm = {0};
    jtime_t cur;

    cur = jtime_utcsec_get();
    if (cur - fcfg->last_check < RESTATDIR_SEC) {
        goto err;
    }
    fcfg->last_check = cur;

    jthread_mutex_lock(&mgr->cmtx);
    fmax = fcfg->fcount;
    dlen = fcfg->dlen;
    memcpy(path, fcfg->path, dlen + 1);
    jthread_mutex_unlock(&mgr->cmtx);

    if (jfs_mkdir_(path, 0) < 0) {
        goto err;
    }

    if (fmax) {
        if (fcfg->fnames && fcfg->fnum == fmax) {
            if (fcfg->fnames[fcfg->fcnt].name[0]) {
                if (dlen + LOGFILE_STRLEN < PATH_MAX_LEN - 1) {
                    memcpy(path + dlen, fcfg->fnames[fcfg->fcnt].name, LOGFILE_STRLEN);
                    path[dlen + LOGFILE_STRLEN] = '\0';
                    jfs_rmfile(path);
                    path[dlen] = '\0';
                    fcfg->fnames[fcfg->fcnt].name[0] = '\0';
                }
            }
        } else {
            jlog_free_file();
            fcfg->fnames = (jlog_fname_t *)jheap_malloc(fmax * sizeof(jlog_fname_t));
            if (!fcfg->fnames) {
                goto err;
            }
            fcfg->fnum = fmax;
            fcfg->fcnt = 0;

            jfs_listdir_(path, &dirs, &num, jlog_filter_file, 0);
            jfs_sortdir(dirs, num, JFS_SORT_BY_TIME);
            for (i = num - fmax; i >= 0; --i) {
                if (dlen + LOGFILE_STRLEN < PATH_MAX_LEN - 1) {
                    memcpy(path + dlen, dirs[i].name, LOGFILE_STRLEN);
                    path[dlen + LOGFILE_STRLEN] = '\0';
                    jfs_rmfile(path);
                    path[dlen] = '\0';
                }
            }

            cnt = num - fmax + 1;
            if (cnt < 0)
                cnt = 0;
            for (i = cnt; i < num; ++i)
                memcpy(fcfg->fnames[i - cnt].name, dirs[i].name, LOGFILE_STRLEN + 1);
            fcfg->fcnt = i - cnt;
            for (i = fcfg->fcnt; i < fcfg->fnum; ++i)
                fcfg->fnames[i].name[0] = '\0';

            jfs_freedir(&dirs, &num);
        }
    }

    jcfg->zone_sec = jtime_localutc_diff();
    jtime_utctime_get(&tm, jcfg->zone_sec);
    snprintf(path + dlen, PATH_MAX_LEN - dlen, LOGFILE_FORMAT,
        tm.year, tm.month, tm.day, tm.hour, tm.min, tm.sec, tm.msec);
    fcfg->fd = jfs_open(path, "a+");
    if (!jfs_fd_valid(fcfg->fd)) {
        path[dlen] = '\0';
        goto err;
    }
    if (fmax) {
        memcpy(fcfg->fnames[fcfg->fcnt].name, path + dlen, LOGFILE_STRLEN + 1);
        if (++fcfg->fcnt == fcfg->fnum)
            fcfg->fcnt = 0;
    }
    path[dlen] = '\0';

    return 0;
err:
    return -1;
}

static inline int jlog_check_file(void)
{
    if (jfs_fd_valid(g_jlog_mgr.jcfg.fcfg.fd))
        return 0;
    return jlog_new_file();
}

static inline int jlog_write_file(char *buf, int rlen)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    int wlen = 0;

    wlen = jfs_write(jcfg->fcfg.fd, buf, rlen);
    if (wlen < 0) {
        jfs_close(jcfg->fcfg.fd);
        jcfg->fcfg.size = 0;
        return -1;
    }

    jcfg->fcfg.size += wlen;
    if (jcfg->fcfg.size < jcfg->fcfg.fsize)
        return wlen;

    jfs_close(jcfg->fcfg.fd);
    jcfg->fcfg.size = 0;
    jcfg->fcfg.last_check = 0;
    if (jlog_new_file() < 0)
        return -1;

    return wlen;
}

static int jlog_check_network(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_ncfg_t *ncfg = &jcfg->ncfg;
    char *buf = NULL;
    jtime_t cur;

    cur = jtime_utcsec_get();
    if (jsocket_fd_valid(ncfg->fd)) {
        int rlen = 0, wlen = 0;
        if (cur - ncfg->last_send >= HEARTBEAT_SEC && !(buf = jlog_buf_get(&rlen))) {
            char ebuf[128];
            jcfg->zone_sec = jtime_localutc_diff();
            rlen = jlog_head(NULL, JLOG_LEVEL_WARN, &g_jlog_none, &g_hb_type, ebuf, sizeof(ebuf));
            ebuf[rlen++] = '\n';
            wlen = jsocket_send_(ncfg->fd, ebuf, rlen, 0);
            if (wlen < 0) {
                jsocket_close(ncfg->fd);
                goto err;
            }
            ncfg->last_send = cur;
        }
    } else {
        if (cur - ncfg->last_check < RECONNECT_SEC) {
            goto err;
        }
        ncfg->last_check = cur;
        ncfg->last_send = cur;
        ncfg->fd = jsocket_tcp_client_(&ncfg->jaddr, 0);
        if (!jsocket_fd_valid(ncfg->fd)) {
            goto err;
        }
        jcfg->zone_sec = jtime_localutc_diff();
    }

    return 0;
err:
    return -1;
}

static void jlog_flush(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    int rlen = 0, wlen = 0;
    int send_flag = 0, change_flag = 0;
    char *buf = NULL;
    char ebuf[128];

    jthread_mutex_lock(&mgr->cmtx);
    if (jcfg->changed) {
        jcfg->changed = 0;
        change_flag = 1;
    }
    jthread_mutex_unlock(&mgr->cmtx);

    if (change_flag) {
        jfs_close(jcfg->fcfg.fd);
        jcfg->fcfg.size = 0;
        jcfg->fcfg.last_check = 0;
        jlog_free_file();
        jsocket_close(jcfg->ncfg.fd);
        jcfg->ncfg.last_check = 0;
        jcfg->mode = jcfg->wanted;
    }

    switch (jcfg->mode) {
    case JLOG_TO_FILE:
        if (jlog_check_file() < 0) {
            break;
        }

        rlen = jlog_check_overflow(ebuf, sizeof(ebuf));
        if (rlen) {
            wlen = jlog_write_file(ebuf, rlen);
            if (wlen < 0) {
                break;
            }
        }

        buf = jlog_check_performance(&rlen);
        if (buf) {
            wlen = jlog_write_file(buf, rlen);
            if (wlen < 0) {
                break;
            }
        }

        while ((buf = jlog_buf_get(&rlen))) {
            wlen = jlog_write_file(buf, rlen < JLOG_WRITE_SIZE ? rlen : JLOG_WRITE_SIZE);
            if (wlen < 0) {
                break;
            }
            jlog_buf_set(wlen);
        }
        break;

    case JLOG_TO_NET:
        if (jlog_check_network() < 0) {
            break;
        }

        send_flag = 0;
        rlen = jlog_check_overflow(ebuf, sizeof(ebuf));
        if (rlen) {
            wlen = jsocket_send_(jcfg->ncfg.fd, ebuf, rlen, 0);
            if (wlen < 0) {
                jsocket_close(jcfg->ncfg.fd);
                break;
            }
            send_flag = 1;
        }

        buf = jlog_check_performance(&rlen);
        if (buf) {
            wlen = jsocket_send_(jcfg->ncfg.fd, buf, rlen, 0);
            if (wlen < 0) {
                jsocket_close(jcfg->ncfg.fd);
                break;
            }
            send_flag = 1;
        }

        while ((buf = jlog_buf_get(&rlen))) {
            wlen = jsocket_send_(jcfg->ncfg.fd, buf, rlen < JLOG_SEND_SIZE ? rlen : JLOG_SEND_SIZE, 0);
            if (wlen < 0) {
                jsocket_close(jcfg->ncfg.fd);
                break;
            } else if (wlen > 0) {
                jlog_buf_set(wlen);
            }
            send_flag = 1;
        }
        if (send_flag)
            jcfg->ncfg.last_send = jtime_utcsec_get();
        break;

    default:
        rlen = jlog_check_overflow(ebuf, sizeof(ebuf));
        if (rlen) {
            JFS_WRERR(ebuf, rlen);
        }

        buf = jlog_check_performance(&rlen);
        if (buf) {
            JFS_WROUT(buf, rlen);
        }

        while ((buf = jlog_buf_get(&rlen))) {
            JFS_WROUT(buf, rlen);
            wlen = rlen;
            jlog_buf_set(wlen);
        }
        break;
    }
    jlog_buf_abandon();
}

static jthread_ret_t jlog_run(void *args)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jcfg_t *jcfg = &mgr->jcfg;

    jthread_setname("jlog_flush");
    while (mgr->inited) {
        jthread_mutex_lock(&mgr->cmtx);
        jthread_cond_mtimewait(&mgr->cond, &mgr->cmtx, JLOG_SLEEP_MS, 1);
        jthread_mutex_unlock(&mgr->cmtx);
        jlog_flush();
    }
    jlog_flush();

    jfs_close(jcfg->fcfg.fd);
    jsocket_close(jcfg->ncfg.fd);
    jlog_free_file();

    return (jthread_ret_t)0;
}

int jlog_init(const jlog_cfg_t *cfg)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_fcfg_t *fcfg = &jcfg->fcfg;
    jlog_ncfg_t *ncfg = &jcfg->ncfg;
    jlog_cfg_t tcfg = {0};
    int len = 0;
    jthread_attr_t attr = {0};

    if (mgr->inited)
        return 0;

    if (!cfg)
        cfg = &tcfg;

    jcfg->level = cfg->level;
    if (!jcfg->level)
        jcfg->level = JLOG_LEVEL_DEF;

    jcfg->mode = cfg->mode;
    if (jcfg->mode == JLOG_TO_AUTO)
        jcfg->mode = JLOG_TO_TTY;
    jcfg->wanted = jcfg->mode;
    jcfg->zone_sec = jtime_localutc_diff();

    fcfg->fd = JFS_INVALID_FD;
    fcfg->fsize = cfg->file.file_size;
    if (!fcfg->fsize)
        fcfg->fsize = JLOG_DEF_FSIZE;
    fcfg->fcount = cfg->file.file_count;
    if (!fcfg->fcount)
        fcfg->fcount = JLOG_DEF_FCOUNT;
    if (fcfg->fcount < 0)
        fcfg->fcount = 0;
    if (cfg->file.file_path && cfg->file.file_path[0]) {
        fcfg->dlen = strlen(cfg->file.file_path);
        memcpy(fcfg->path, cfg->file.file_path, fcfg->dlen);
        fcfg->path[fcfg->dlen] = '\0';
    } else {
        fcfg->dlen = strlen(JLOG_DEF_FPATH);
        memcpy(fcfg->path, JLOG_DEF_FPATH, fcfg->dlen);
        fcfg->path[fcfg->dlen] = '\0';
    }
    jfs_sp_adapted(fcfg->path);
    if (fcfg->path[fcfg->dlen - 1] != JFS_SP) {
        fcfg->path[fcfg->dlen++] = JFS_SP;
        fcfg->path[fcfg->dlen] = '\0';
    }
    fcfg->last_check = 0;
    fcfg->fnames = NULL;
    fcfg->fnum = 0;
    fcfg->fcnt = 0;

    ncfg->fd = JSOCKET_INVALID_FD;
    ncfg->jaddr.domain = cfg->net.is_ipv6 ? AF_INET6 : AF_INET;
    ncfg->jaddr.port = cfg->net.ip_port;
    if (!ncfg->jaddr.port)
        ncfg->jaddr.port = JLOG_DEF_IPPORT;
    if (cfg->net.ip_addr && cfg->net.ip_addr[0]) {
        len = strlen(cfg->net.ip_addr);
        memcpy(ncfg->jaddr.addr, cfg->net.ip_addr, len);
        ncfg->jaddr.addr[len] = '\0';
    } else {
        len = strlen(JLOG_DEF_IPADDR);
        memcpy(ncfg->jaddr.addr, JLOG_DEF_IPADDR, len);
        ncfg->jaddr.addr[len] = '\0';
    }
    ncfg->jaddr.msec = 1000;
    ncfg->last_check = 0;

    jcfg->perf = cfg->perf;

    jbuf->size = cfg->buf_size;
    if (!jbuf->size)
        jbuf->size = JLOG_BUF_SIZE;
    jbuf->wake = cfg->wake_size;
    if (!jbuf->wake)
        jbuf->wake = JLOG_WAKE_SIZE;
    if (jbuf->wake * 2 > jbuf->size)
        jbuf->wake = jbuf->size >> 1;
    jbuf->res = cfg->res_size;
    if (jbuf->res < JLOG_RES_SIZE)
        jbuf->res = JLOG_RES_SIZE;
    jbuf->buf = (char *)jheap_malloc(jbuf->size);
    if (!jbuf->buf) {
        jbuf->size = 0;
        return -1;
    }

    jthread_mutex_init(&mgr->cmtx);
    jthread_mutex_init(&mgr->mtx);
    jthread_cond_create(&mgr->cond, 1);
    mgr->inited = 1;

    attr.stack_size = JLOG_STACK_SIZE;
    jthread_create(&mgr->tid, &attr, jlog_run, NULL);
    return 0;
}

void jlog_uninit(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;

    if (!mgr->inited)
        return;
    jthread_mutex_lock(&mgr->mtx);
    mgr->inited = 0;
    jthread_mutex_unlock(&mgr->mtx);
    jthread_cond_signal(&mgr->cond);

    jthread_join(mgr->tid);
    jthread_mutex_destroy(&mgr->cmtx);
    jthread_mutex_destroy(&mgr->mtx);
    jthread_cond_destroy(&mgr->cond);

    jheap_free(jbuf->buf);
    memset(jbuf, 0, sizeof(jlog_jbuf_t));
}

int jlog_cfg_get(jlog_cfg_t *cfg)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_fcfg_t *fcfg = &jcfg->fcfg;
    jlog_ncfg_t *ncfg = &jcfg->ncfg;

    if (!cfg)
        return -1;

    cfg->buf_size = jbuf->size;
    cfg->wake_size = jbuf->wake;
    cfg->res_size = jbuf->res;
    cfg->level = jcfg->level;
    cfg->mode = jcfg->mode;

    cfg->file.file_size = fcfg->fsize;
    cfg->file.file_count = fcfg->fcount;
    if (cfg->file.file_count == 0)
        cfg->file.file_count = -1;
    cfg->file.file_path = fcfg->path;

    cfg->net.is_ipv6 = ncfg->jaddr.domain == AF_INET6 ? 1 : 0;
    cfg->net.ip_port = ncfg->jaddr.port;
    cfg->net.ip_addr = ncfg->jaddr.addr;

    cfg->perf = jcfg->perf;

    return 0;
}

int jlog_cfg_set(const jlog_cfg_t *cfg)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    jlog_jbuf_t *jbuf = &mgr->jbuf;
    jlog_jcfg_t *jcfg = &mgr->jcfg;
    jlog_fcfg_t *fcfg = &jcfg->fcfg;
    jlog_ncfg_t *ncfg = &jcfg->ncfg;
    int len = 0;
    char *tmp = NULL;

    if (!cfg)
        return -1;

    jthread_mutex_lock(&mgr->cmtx);

    if (cfg->level)
        jcfg->level = cfg->level;

    if (cfg->mode != JLOG_TO_AUTO) {
        if (jcfg->mode != cfg->mode || jcfg->wanted != cfg->mode)
            jcfg->changed = 1;
        jcfg->wanted = cfg->mode;
    }

    if (cfg->file.file_size)
        fcfg->fsize = cfg->file.file_size;
    if (cfg->file.file_count) {
        fcfg->fcount = cfg->file.file_count;
        if (fcfg->fcount < 0)
            fcfg->fcount = 0;
    }
    if (cfg->file.file_path && cfg->file.file_path[0]) {
        len = strlen(cfg->file.file_path);
        tmp = (char *)jheap_malloc(len + 2);
        if (tmp) {
            memcpy(tmp, cfg->file.file_path, len);
            tmp[len] = '\0';
            jfs_sp_adapted(tmp);
            if (tmp[len] != JFS_SP) {
                tmp[len++] = JFS_SP;
                tmp[len] = '\0';
            }
            if (len != fcfg->dlen || strncmp(tmp, fcfg->path, len) != 0) {
                if (jcfg->wanted == JLOG_TO_FILE)
                    jcfg->changed = 1;
                fcfg->dlen = len;
                memcpy(fcfg->path, tmp, fcfg->dlen);
            }
            jheap_free(tmp);
            tmp = NULL;
        }
    }

    if (cfg->net.ip_port && ncfg->jaddr.port != cfg->net.ip_port) {
        if (jcfg->wanted == JLOG_TO_NET)
            jcfg->changed = 1;
        ncfg->jaddr.port = cfg->net.ip_port;
    }
    if (cfg->net.ip_addr && cfg->net.ip_addr[0] && strcmp(ncfg->jaddr.addr, cfg->net.ip_addr) != 0) {
        if (jcfg->wanted == JLOG_TO_NET)
            jcfg->changed = 1;
        ncfg->jaddr.domain = cfg->net.is_ipv6 ? AF_INET6 : AF_INET;
        len = strlen(cfg->net.ip_addr);
        memcpy(ncfg->jaddr.addr, cfg->net.ip_addr, len);
        ncfg->jaddr.addr[len] = '\0';
    }

    jcfg->perf = cfg->perf;

    if (cfg->wake_size && cfg->wake_size * 2 <= jbuf->size)
        jbuf->wake = cfg->wake_size;
    if (cfg->res_size > JLOG_RES_SIZE)
        jbuf->res = cfg->res_size;
    jthread_mutex_unlock(&mgr->cmtx);

    return 0;
}

int jlog_level_get(void)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    return mgr->jcfg.level;
}

void jlog_level_set(int level)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    mgr->jcfg.level = level;
}

void jlog_perf_get(jlog_perf_t *perf)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    *perf = mgr->jcfg.perf;
}

void jlog_perf_set(jlog_perf_t *perf)
{
    jlog_mgr_t *mgr = &g_jlog_mgr;
    mgr->jcfg.perf = *perf;
}
