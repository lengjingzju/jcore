/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include "stdlib.h"
#include "jlog.h"
#include "jfs.h"
#include "jfcache.h"
#include "jheap.h"

#define JFCACHE_ERROR(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    jfs_fd_t fd;            // 文件描述符
    int flag;               // -1, invalid; 0, read; 1, write
    size_t logic_off;       // 文件逻辑偏移
    size_t phy_off;         // 文件物理偏移
    size_t size;            // 实际使用的缓冲大小
    size_t bsize;           // 缓冲的总大小
    char *buf;              // 文件缓冲
} jfcache_t;

void* jfcache_open(const char *fname, const char *mode, size_t bsize)
{
    jfcache_t *jfc = NULL;

    if (!bsize) {
        bsize = 8192;
    }
    if (!(jfc = (jfcache_t *)jheap_calloc(1, sizeof(jfcache_t)))) {
        JFCACHE_ERROR("calloc failed!");
        return NULL;
    }
    if (!(jfc->buf = (char *)jheap_malloc(bsize))) {
        jheap_free(jfc);
        JFCACHE_ERROR("malloc failed!");
        return NULL;
    }

    jfc->fd = jfs_open(fname, mode);
    if (!jfs_fd_valid(jfc->fd)) {
        jheap_free(jfc->buf);
        jheap_free(jfc);
        JFCACHE_ERROR("open(%s) failed!", fname);
        return NULL;
    }

    jfc->bsize = bsize;
    jfc->flag = -1;
    return (void*)jfc;
}

int jfcache_close(void* hd)
{
    jfcache_t *jfc = (jfcache_t *)hd;
    int ret = 0;

    if (!jfc)
        return 0;

    if (jfcache_sync(hd) < 0) {
        JFCACHE_ERROR("write failed!");
        ret = -1;
    }

    if (jfs_fd_valid(jfc->fd))
        jfs_close(jfc->fd);
    if (jfc->buf)
        jheap_free(jfc->buf);
    jheap_free(jfc);

    return ret;
}

int jfcache_sync(void* hd)
{
    jfcache_t *jfc = (jfcache_t *)hd;

    if (jfc->flag == 1 && jfc->size > 0) {
        if ((ssize_t)jfc->size != jfs_write(jfc->fd, jfc->buf, jfc->size)) {
            JFCACHE_ERROR("write failed!");
            return -1;
        }
        jfc->phy_off += jfc->size;
        jfc->logic_off = jfc->phy_off;
        jfc->size = 0;
    }

    return 0;
}

ssize_t jfcache_read(void* hd, char *buf, size_t size)
{
    jfcache_t *jfc = (jfcache_t *)hd;
    ssize_t rsize = 0;

    jfcache_sync(hd);
    if (jfc->flag == 0 && jfc->logic_off >= jfc->phy_off &&
            jfc->logic_off + size <= jfc->phy_off + jfc->size)
    {
        rsize = size;
        memcpy(buf, jfc->buf + jfc->logic_off - jfc->phy_off, rsize);
    } else {
        if (jfc->flag == -1) {
            jfc->logic_off = jfs_lseek(jfc->fd, 0, JFS_SEEK_CUR);
        }
        jfs_lseek(jfc->fd, jfc->logic_off, JFS_SEEK_SET);
        jfc->phy_off = jfc->logic_off;

        if (size > jfc->bsize) {
            if ((rsize = jfs_read(jfc->fd, buf, size)) < 0) {
                JFCACHE_ERROR("read failed!");
                return -1;
            }
            jfc->size = 0;
        } else {
            if ((rsize = jfs_read(jfc->fd, jfc->buf, jfc->bsize)) < 0) {
                JFCACHE_ERROR("read failed!");
                return -1;
            }
            jfc->size = rsize;

            rsize = (ssize_t)size <= rsize ? (ssize_t)size : rsize;
            memcpy(buf, jfc->buf, size);
        }
    }

    jfc->logic_off += rsize;
    jfc->flag = 0;

    return rsize;
}

ssize_t jfcache_write(void* hd, const char *buf, size_t size)
{
    jfcache_t *jfc = (jfcache_t *)hd;
    ssize_t wsize = -1;

    if (jfc->flag != 1) {
        if (jfc->flag == -1) {
            jfc->phy_off = jfs_lseek(jfc->fd, 0, JFS_SEEK_CUR);
        } else {
            jfc->phy_off = jfs_lseek(jfc->fd, jfc->logic_off, JFS_SEEK_SET);
        }
        jfc->logic_off = jfc->phy_off;
        jfc->size = 0;
        jfc->flag = 1;
    }

    if (jfc->size + size > jfc->bsize) {
        if (jfcache_sync(hd) < 0) {
            JFCACHE_ERROR("write failed!");
            return -1;
        }
    }

    if (jfc->size + size <= jfc->bsize) {
        memcpy(jfc->buf + jfc->size, buf, size);
        jfc->size += size;
        jfc->logic_off += size;
        return size;
    } else {
        if ((wsize = jfs_write(jfc->fd, buf, size)) != (ssize_t)size) {
            if (wsize < 0) {
                JFCACHE_ERROR("write failed!");
                return -1;
            }
        }
        jfc->phy_off += wsize;
        jfc->logic_off = jfc->phy_off;
        jfc->size = 0;
        return wsize;
    }

    return -1;
}

ssize_t jfcache_tell(void* hd)
{
    jfcache_t *jfc = (jfcache_t *)hd;

    if (jfc->flag == -1) {
        jfc->logic_off = jfs_lseek(jfc->fd, 0, JFS_SEEK_CUR);
        jfc->phy_off = jfc->logic_off;
        jfc->size = 0;
        jfc->flag = 0;
    }

    return jfc->logic_off;
}

ssize_t jfcache_seek(void* hd, ssize_t offset, int whence)
{
    jfcache_t *jfc = (jfcache_t *)hd;
    ssize_t roffset = 0;

    jfcache_tell(hd);
    jfcache_sync(hd);
    switch (whence) {
        case JFS_SEEK_SET:
            roffset = offset;
            break;
        case JFS_SEEK_CUR:
            roffset = jfc->logic_off + offset;
            break;
        case JFS_SEEK_END:
            roffset = jfs_lseek(jfc->fd, offset, JFS_SEEK_END);
            jfs_lseek(jfc->fd, jfc->phy_off, JFS_SEEK_SET);
            break;
        default:
            return -1;
    }

    if (roffset < 0) {
        jfc->logic_off = 0;
    } else {
        jfc->logic_off = roffset;
    }
    if (jfc->flag == 1) {
        jfc->phy_off = jfs_lseek(jfc->fd, jfc->logic_off, JFS_SEEK_SET);
    }

    return jfc->logic_off;
}
