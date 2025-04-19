/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "jlog.h"
#include "jfs.h"
#include "jheap.h"

#define JFS_PATH_MAX        4096

#define JFS_ERRNO(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)

static int jfs_def_filter(const char *fname, unsigned int flen, unsigned int ftype)
{
    if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
        return 0;
    return 1;
}

int jfs_countdir(const char *dname, jfs_filter_cb filter)
{
    DIR *dir = NULL;
    struct dirent *d = NULL;
    struct stat st;
    int dlen, tlen;
    int ftype = 0;
    int num = 0;

    dir = opendir(dname);
    if (!dir) {
        JFS_ERRNO("opendir(%s) failed!", dname);
        return -1;
    }

    if (filter) {
        dlen = strlen(dname);
        if (dlen >= JFS_PATH_MAX - 2) {
            num = -1;
        } else {
            char path[JFS_PATH_MAX];

            memcpy(path, dname, dlen);
            if (path[dlen] != '/')
                path[dlen++] = '/';

            while ((d = readdir(dir))) {
                tlen = strlen(d->d_name);
                if (dlen + tlen >= JFS_PATH_MAX) {
                    num = -1;
                    break;
                }
                memcpy(path + dlen, d->d_name, tlen);
                path[dlen + tlen] = '\0';

                lstat(path, &st);
                switch (st.st_mode & S_IFMT) {
                case S_IFREG:
                    ftype = JFS_ISFILE;
                    break;
                case S_IFDIR:
                    ftype = JFS_ISDIR;
                    break;
                case S_IFLNK:
                    ftype = JFS_ISLINK;
                    break;
                default:
                    ftype = JFS_ISOTHER;
                    break;
                }

                if (filter(d->d_name, tlen, ftype) == 0)
                    continue;
                ++num;
            }
        }
    } else {
        while ((d = readdir(dir))) {
            if (jfs_def_filter(d->d_name, 0, 0) == 0)
                continue;
            ++num;
        }
    }

    closedir(dir);
    return num;
}

int jfs_listdir_(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter, int print_flag)
{
    DIR *dir = NULL;
    struct dirent *d = NULL;
    struct stat st;
    int dlen, tlen, total = 0;
    int ftype = 0;
    int ret = 0;

    dir = opendir(dname);
    if (!dir) {
        if (print_flag)
            JFS_ERRNO("opendir(%s) failed!", dname);
        return -1;
    }

    while ((d = readdir(dir))) {
        if (jfs_def_filter(d->d_name, 0, 0) == 0)
            continue;
        ++total;
    }

    if (total == 0) {
        *dirs = NULL;
        *num = 0;
        goto end;
    }

    *num = 0;
    *dirs = (jfs_dirent_t *)jheap_malloc(total * sizeof(jfs_dirent_t));
    if (!(*dirs)) {
        if (print_flag)
            JFS_ERRNO("malloc failed!");
        ret = -1;
        goto end;
    }

    dlen = strlen(dname);
    if (dlen >= JFS_PATH_MAX - 2) {
        ret = -1;
    } else {
        char path[JFS_PATH_MAX];

        memcpy(path, dname, dlen);
        if (path[dlen] != '/')
            path[dlen++] = '/';

        if (!filter)
            filter = jfs_def_filter;

        rewinddir(dir);
        while ((d = readdir(dir))) {
            tlen = strlen(d->d_name);
            if (dlen + tlen >= JFS_PATH_MAX) {
                ret = -1;
                break;
            }
            memcpy(path + dlen, d->d_name, tlen);
            path[dlen + tlen] = '\0';

            lstat(path, &st);
            switch (st.st_mode & S_IFMT) {
            case S_IFREG:
                ftype = JFS_ISFILE;
                break;
            case S_IFDIR:
                ftype = JFS_ISDIR;
                break;
            case S_IFLNK:
                ftype = JFS_ISLINK;
                break;
            default:
                ftype = JFS_ISOTHER;
                break;
            }

            if (filter(d->d_name, tlen, ftype) == 0)
                continue;

            if (!((*dirs)[*num].name = (char *)jheap_malloc(tlen + 1))) {
                if (print_flag)
                    JFS_ERRNO("malloc failed!");
                ret = -1;
                break;
            }
            memcpy((*dirs)[*num].name, d->d_name, tlen);
            (*dirs)[*num].name[tlen] = '\0';
            (*dirs)[*num].len = tlen;
            (*dirs)[*num].size = st.st_size;
            (*dirs)[*num].mtime = st.st_mtime;
            (*dirs)[*num].type = ftype;
            ++(*num);
            if (*num == total)
                break;
        }
    }

    if (ret < 0 || *num == 0) {
        jfs_freedir(dirs, num);
    }

end:
    closedir(dir);
    return ret;
}

int jfs_listdir(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter)
{
    return jfs_listdir_(dname, dirs, num, filter, 1);
}

void jfs_freedir(jfs_dirent_t **dirs, int *num)
{
    if (*dirs) {
        int i;
        for (i = 0; i < *num; ++i) {
            jheap_free((*dirs)[i].name);
        }
        jheap_free(*dirs);
        *dirs = NULL;
        *num = 0;
    }
}

int jfs_mkdir_(const char *dname, int print_flag)
{
    int dlen = 0;
    int ret = 0;

    if (jfs_existed(dname))
        return 0;

    dlen = strlen(dname);
    if (dlen >= JFS_PATH_MAX - 2) {
        ret = -1;
    } else {
        char path[JFS_PATH_MAX];
        char *p = path, *q = path + 1;

        memcpy(path, dname, dlen + 1);
        while ((q = strchr(q, '/'))) {
            *q = '\0';
            if (!jfs_existed(p)) {
                if (mkdir(p, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
                    if (print_flag)
                        JFS_ERRNO("mkdir(%s) failed!", p);
                    ret = -1;
                    goto end;
                }
            }
            *q++ = '/';
        }

        if (!jfs_existed(p) && mkdir(p, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
            if (print_flag)
                JFS_ERRNO("mkdir(%s) failed!", p);
            ret = -1;
            goto end;
        }
    }

end:
    return ret;
}

int jfs_mkdir(const char *dname)
{
    return jfs_mkdir_(dname, 0);
}

static int jfs_rmdir_exec(char *path, int dlen)
{
    DIR *dir = NULL;
    struct dirent *d = NULL;
    struct stat st;
    int tlen, alen;
    int ret = 0;

    dir = opendir(path);
    if (!dir) {
        JFS_ERRNO("opendir(%s) failed!", path);
        return -1;
    }

    while ((d = readdir(dir))) {
        if (jfs_def_filter(d->d_name, 0, 0) == 0)
            continue;

        tlen = strlen(d->d_name);
        alen = dlen + tlen;
        if (alen >= JFS_PATH_MAX - 1) {
            ret = -1;
            break;
        }
        memcpy(path + dlen, d->d_name, tlen);
        path[alen] = '\0';

        lstat(path, &st);
        switch (st.st_mode & S_IFMT) {
        case S_IFDIR:
            path[alen++] = '/';
            path[alen] = '\0';
            if (jfs_rmdir_exec(path, alen) < 0) {
                ret = -1;
            }
            break;
        default:
            if (unlink(path) < 0) {
                JFS_ERRNO("unlink(%s) failed!", path);
                ret = -1;
            }
            break;
        }

        if (ret < 0)
            break;
    }
    closedir(dir);

    path[dlen] = '\0';
    if (ret == 0) {
        if (rmdir(path) < 0) {
            JFS_ERRNO("rmdir(%s) failed!", path);
            ret = -1;
        }
    }

    return ret;
}

int jfs_rmdir(const char *dname)
{
    int dlen = 0;
    int ret = 0;

    if (!jfs_existed(dname))
        return 0;

    dlen = strlen(dname);
    if (dlen >= JFS_PATH_MAX - 2) {
        ret = -1;
    } else {
        char path[JFS_PATH_MAX];

        memcpy(path, dname, dlen);
        if (path[dlen] != '/')
            path[dlen++] = '/';
        path[dlen] = '\0';
        ret = jfs_rmdir_exec(path, dlen);
    }

    return ret;
}
