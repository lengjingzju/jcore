/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include "jlog.h"
#include "jini.h"
#include "jheap.h"
#include <stdlib.h>

void print_jlog_cfg(void *hd)
{
    jlog_cfg_t cfg = {0};

    cfg.buf_size = jini_get_int(hd, "jlog", "buf_size", 1024) << 10;
    cfg.wake_size = jini_get_int(hd, "jlog", "wake_size", 64) << 10;
    cfg.level = jini_get_int(hd, "jlog", "level", JLOG_LEVEL_INFO);
    cfg.mode = (jlog_mode_t)jini_get_int(hd, "jlog", "mode", JLOG_TO_NET);

    cfg.file.file_size = jini_get_int(hd, "jlog", "file_size", 1024) << 10;
    cfg.file.file_count = jini_get_int(hd, "jlog", "file_count", 10);
    cfg.file.file_path = jini_get(hd, "jlog", "file_path", "jlog");

    cfg.net.is_ipv6 = jini_get_int(hd, "jlog", "is_ipv6", 0);
    cfg.net.ip_port = jini_get_int(hd, "jlog", "ip_port", 9999);
    cfg.net.ip_addr = jini_get(hd, "jlog", "ip_addr", "127.0.0.1");

    SLOG_INFO("buf_size = %d\n", cfg.buf_size >> 10);
    SLOG_INFO("wake_size = %d\n", cfg.wake_size >> 10);
    SLOG_INFO("level = %d\n", cfg.level);
    SLOG_INFO("mode = %d\n", cfg.mode);
    SLOG_INFO("file_size = %d\n", cfg.file.file_size >> 10);
    SLOG_INFO("file_count = %d\n", cfg.file.file_count);
    SLOG_INFO("file_path = %s\n", cfg.file.file_path);
    SLOG_INFO("is_ipv6 = %d\n", cfg.net.is_ipv6);
    SLOG_INFO("ip_port = %d\n", cfg.net.ip_port);
    SLOG_INFO("ip_addr = %s\n", cfg.net.ip_addr);
    SLOG_INFO("\n");
}

int main(void)
{
    void *hd = NULL;
    int ret = 0;

#if JHEAP_DEBUG
    jheap_init_debug(4);
    jheap_start_debug();
#endif

    hd = jini_init("jlog.ini");
    if (!hd) {
        LLOG_ERROR("init cfg failed!\n");
        ret = -1;
        goto end;
    }
    SLOG_INFO("----jlog.ini----\n");
    print_jlog_cfg(hd);

    SLOG_INFO("changed ip_port to 8888 and ip_addr to 192.168.0.10\n");
    jini_set_int(hd, "jlog", "ip_port", 8888);
    jini_set(hd, "jlog", "ip_addr", "192.168.0.10");

    SLOG_INFO("add test section\n");
    jini_set(hd, "test", "test1", "  ");
    jini_set(hd, "test", "test2", "#");
    jini_set(hd, "test", "test3", ";");
    jini_set(hd, "test", "test4", "123\n;ab\rc");
    jini_set(hd, "test", "test5", "leng jing");
    jini_flush(hd, "jlog2.ini");
    jini_uninit(hd);

    hd = jini_init("jlog2.ini");
    if (!hd) {
        LLOG_ERROR("init cfg failed!\n");
        ret = -1;
        goto end;
    }
    SLOG_INFO("----jlog2.ini----\n");
    print_jlog_cfg(hd);
    jini_flush(hd, "jlog3.ini");
    jini_uninit(hd);

end:
#if JHEAP_DEBUG
    jheap_leak_debug(0);
    jheap_leak_debug(3);
    jheap_uninit_debug();
#endif
    return ret;
}

