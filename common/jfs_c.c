/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <string.h>
#include "jfs.h"
#include "jlog.h"
#include "jheap.h"

#define JFS_PATH_MAX        4096
#define JFS_ERRNO(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)

int jfs_readall(const char *fname, char **buf, size_t *count)
{
    ssize_t size = 0;
    jfs_fd_t fd;

    fd = jfs_open(fname, "r");
    if (!jfs_fd_valid(fd)) {
        JFS_ERRNO("open(%s) failed!", fname);
        return -1;
    }

    size = jfs_lseek(fd, 0, JFS_SEEK_END);
    jfs_lseek(fd, 0, JFS_SEEK_SET);

    *buf = (char *)jheap_malloc(size + 1);
    if (!(*buf)) {
        JFS_ERRNO("malloc failed!");
        jfs_close(fd);
        return -1;
    }

    if (size != jfs_read(fd, *buf, size)) {
        JFS_ERRNO("read() failed!");
        jfs_close(fd);
        jheap_free(*buf);
        *buf = NULL;
        return -1;
    }
    *(*buf + size) = '\0';
    *count = size;

    jfs_close(fd);
    return 0;
}

void jfs_readfree(char **buf, size_t *count)
{
    if (*buf) {
        jheap_free(*buf);
        *buf = NULL;
    }
    *count = 0;
}

int jfs_writeall(const char *fname, const char *buf, size_t count)
{
    jfs_fd_t fd;

    fd = jfs_open(fname, "w");
    if (!jfs_fd_valid(fd)) {
        JFS_ERRNO("open(%s) failed!", fname);
        return -1;
    }

    if ((ssize_t)count != jfs_write(fd, buf, count)) {
        JFS_ERRNO("write() failed!");
        jfs_close(fd);
        return -1;
    }

    jfs_close(fd);
    return 0;
}

#define CMP_TYPE_DIR(ra, rb) do {   \
    if (a->type == JFS_ISDIR) {     \
        if (b->type != JFS_ISDIR)   \
            return ra;              \
    } else {                        \
        if (b->type == JFS_ISDIR)   \
            return rb;              \
    }                               \
} while (0)

static long cmp00(const jfs_dirent_t *a, const jfs_dirent_t *b) { return strcmp(a->name, b->name); }
static long cmp01(const jfs_dirent_t *a, const jfs_dirent_t *b) { return strcmp(b->name, a->name); }
static long cmp02(const jfs_dirent_t *a, const jfs_dirent_t *b) { return a->size  - b->size; }
static long cmp03(const jfs_dirent_t *a, const jfs_dirent_t *b) { return b->size  - a->size; }
static long cmp04(const jfs_dirent_t *a, const jfs_dirent_t *b) { return a->mtime - b->mtime; }
static long cmp05(const jfs_dirent_t *a, const jfs_dirent_t *b) { return b->mtime - a->mtime; }

static long cmp10(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return strcmp(a->name, b->name); }
static long cmp11(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return strcmp(b->name, a->name); }
static long cmp12(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return a->size  - b->size; }
static long cmp13(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return b->size  - a->size; }
static long cmp14(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return a->mtime - b->mtime; }
static long cmp15(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(-1, 1); return b->mtime - a->mtime; }

static long cmp20(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return strcmp(a->name, b->name); }
static long cmp21(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return strcmp(b->name, a->name); }
static long cmp22(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return a->size  - b->size; }
static long cmp23(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return b->size  - a->size; }
static long cmp24(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return a->mtime - b->mtime; }
static long cmp25(const jfs_dirent_t *a, const jfs_dirent_t *b) { CMP_TYPE_DIR(1, -1); return b->mtime - a->mtime; }

typedef long (*dir_cmp_t)(const jfs_dirent_t *, const jfs_dirent_t *);

static int quick_sortdir_step(jfs_dirent_t *dirs, int left, int right, dir_cmp_t cmp)
{
    int i = left;
    int j = right;
    jfs_dirent_t key, tmp;

    memcpy(&key, &dirs[left], sizeof(jfs_dirent_t));
    while (i != j) {
        while (i < j && cmp(&dirs[j], &key) >= 0)
            --j;
        while (i < j && cmp(&dirs[i], &key) <= 0)
            ++i;
        if (i < j) {
            memcpy(&tmp, &dirs[i], sizeof(jfs_dirent_t));
            memcpy(&dirs[i], &dirs[j], sizeof(jfs_dirent_t));
            memcpy(&dirs[j], &tmp, sizeof(jfs_dirent_t));
        }
    }
    memcpy(&dirs[left], &dirs[i], sizeof(jfs_dirent_t));
    memcpy(&dirs[i], &key, sizeof(jfs_dirent_t));

    return i;
}

static void quick_sortdir(jfs_dirent_t *dirs, int left, int right, dir_cmp_t cmp)
{
    int *stack = NULL;
    int count = 0, i = 0, r = 0, l = 0;

    stack = (int *)jheap_malloc(right * sizeof(int));
    if (!stack)
        return;
    if (left < right) {
        stack[count++] = left;
        stack[count++] = right;
    }

    while (count) {
        r = stack[--count];
        l = stack[--count];
        i = quick_sortdir_step(dirs, l, r, cmp);
        if (l < i - 1) {
            stack[count++] = l;
            stack[count++] = i - 1;
        }
        if (r > i + 1) {
            stack[count++] = i + 1;
            stack[count++] = r;
        }
    }
    jheap_free(stack);
}

void jfs_sortdir(jfs_dirent_t *dirs, int num, unsigned int opt)
{
    dir_cmp_t cmp;

    if (num <= 1)
        return;

    switch (opt) {
    case JFS_SORT_BY_NAME:                      cmp = cmp00; break;
    case JFS_SORT_BY_RNAME:                     cmp = cmp01; break;
    case JFS_SORT_BY_SIZE:                      cmp = cmp02; break;
    case JFS_SORT_BY_RSIZE:                     cmp = cmp03; break;
    case JFS_SORT_BY_TIME:                      cmp = cmp04; break;
    case JFS_SORT_BY_RTIME:                     cmp = cmp05; break;
    case JFS_SORT_BY_NAME  | JFS_SORT_BY_DIR:   cmp = cmp10; break;
    case JFS_SORT_BY_RNAME | JFS_SORT_BY_DIR:   cmp = cmp11; break;
    case JFS_SORT_BY_SIZE  | JFS_SORT_BY_DIR:   cmp = cmp12; break;
    case JFS_SORT_BY_RSIZE | JFS_SORT_BY_DIR:   cmp = cmp13; break;
    case JFS_SORT_BY_TIME  | JFS_SORT_BY_DIR:   cmp = cmp14; break;
    case JFS_SORT_BY_RTIME | JFS_SORT_BY_DIR:   cmp = cmp15; break;
    case JFS_SORT_BY_NAME  | JFS_SORT_BY_RDIR:  cmp = cmp20; break;
    case JFS_SORT_BY_RNAME | JFS_SORT_BY_RDIR:  cmp = cmp21; break;
    case JFS_SORT_BY_SIZE  | JFS_SORT_BY_RDIR:  cmp = cmp22; break;
    case JFS_SORT_BY_RSIZE | JFS_SORT_BY_RDIR:  cmp = cmp23; break;
    case JFS_SORT_BY_TIME  | JFS_SORT_BY_RDIR:  cmp = cmp24; break;
    case JFS_SORT_BY_RTIME | JFS_SORT_BY_RDIR:  cmp = cmp25; break;
    default: return;
    }

    quick_sortdir(dirs, 0, num - 1, cmp);
}
