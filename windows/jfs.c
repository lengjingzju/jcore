/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "jlog.h"
#include "jfs.h"
#include "jheap.h"

#define JFS_PATH_MAX        MAX_PATH

#define JFS_ERRNO(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d (%d:%s) " fmt, __func__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)

static int jfs_def_filter(const char *fname, unsigned int flen, unsigned int ftype)
{
    if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
        return 0;
    return 1;
}

static int jfs_get_ftype(const WIN32_FIND_DATAA *fdata)
{
    if (fdata->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return JFS_ISLINK;
    if (fdata->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return JFS_ISDIR;
    return JFS_ISFILE;
}

int jfs_countdir(const char *dname, jfs_filter_cb filter)
{
    char pattern[JFS_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", dname);

    WIN32_FIND_DATAA fdata;
    HANDLE hFind = FindFirstFileA(pattern, &fdata);
    if (hFind == INVALID_HANDLE_VALUE) {
        JFS_ERRNO("FindFirstFile(%s) failed!", dname);
        return -1;
    }

    int num = 0;
    do {
        if (strcmp(fdata.cFileName, ".") == 0 || strcmp(fdata.cFileName, "..") == 0)
            continue;
        int ftype = jfs_get_ftype(&fdata);
        unsigned int tlen = (unsigned int)strlen(fdata.cFileName);
        if (filter && !filter(fdata.cFileName, tlen, ftype))
            continue;
        ++num;
    } while (FindNextFileA(hFind, &fdata));
    FindClose(hFind);

    return num;
}

int jfs_listdir_(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter, int print_flag)
{
    char pattern[JFS_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", dname);

    WIN32_FIND_DATAA fdata;
    HANDLE hFind = FindFirstFileA(pattern, &fdata);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (print_flag)
            JFS_ERRNO("FindFirstFile(%s) failed!", dname);
        return -1;
    }

    int total = 0;
    do {
        if (strcmp(fdata.cFileName, ".") == 0 || strcmp(fdata.cFileName, "..") == 0)
            continue;
        ++total;
    } while (FindNextFileA(hFind, &fdata));
    FindClose(hFind);

    if (total == 0) {
        *dirs = NULL;
        *num = 0;
        return 0;
    }

    *dirs = (jfs_dirent_t *)jheap_malloc(total * sizeof(jfs_dirent_t));
    if (!(*dirs)) {
        if (print_flag)
            JFS_ERRNO("malloc failed!");
        return -1;
    }

    hFind = FindFirstFileA(pattern, &fdata);
    if (hFind == INVALID_HANDLE_VALUE) {
        jheap_free(*dirs);
        *dirs = NULL;
        return -1;
    }

    *num = 0;
    if (!filter) filter = jfs_def_filter;

    do {
        if (strcmp(fdata.cFileName, ".") == 0 || strcmp(fdata.cFileName, "..") == 0)
            continue;

        int ftype = jfs_get_ftype(&fdata);
        unsigned int tlen = (unsigned int)strlen(fdata.cFileName);
        if (!filter(fdata.cFileName, tlen, ftype))
            continue;

        (*dirs)[*num].name = (char *)jheap_malloc(tlen + 1);
        if (!(*dirs)[*num].name) {
            if (print_flag)
                JFS_ERRNO("malloc failed!");
            jfs_freedir(dirs, num);
            FindClose(hFind);
            return -1;
        }
        memcpy((*dirs)[*num].name, fdata.cFileName, tlen + 1);
        (*dirs)[*num].len = tlen;
        (*dirs)[*num].type = ftype;
        (*dirs)[*num].size = ((unsigned long long)fdata.nFileSizeHigh << 32) | fdata.nFileSizeLow;

        // 转换文件时间
        FILETIME ft = fdata.ftLastWriteTime;
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        (*dirs)[*num].mtime = (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);

        ++(*num);
    } while (FindNextFileA(hFind, &fdata));
    FindClose(hFind);

    return 0;
}

int jfs_listdir(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter)
{
    return jfs_listdir_(dname, dirs, num, filter, 1);
}

void jfs_freedir(jfs_dirent_t **dirs, int *num)
{
    if (*dirs) {
        for (int i = 0; i < *num; ++i) {
            jheap_free((*dirs)[i].name);
        }
        jheap_free(*dirs);
        *dirs = NULL;
        *num = 0;
    }
}

int jfs_mkdir_(const char *dname, int print_flag)
{
    char path[JFS_PATH_MAX];
    strncpy(path, dname, sizeof(path));
    path[sizeof(path)-1] = '\0';

    for (char *p = path + 1; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            CreateDirectoryA(path, NULL);
            *p = '\\';
        }
    }
    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            if (print_flag)
                JFS_ERRNO("CreateDirectory(%s) failed!", path);
            return -1;
        }
    }
    return 0;
}

int jfs_mkdir(const char *dname)
{
    return jfs_mkdir_(dname, 0);
}

static int jfs_rmdir_exec(const char *path)
{
    char pattern[JFS_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    WIN32_FIND_DATAA fdata;
    HANDLE hFind = FindFirstFileA(pattern, &fdata);
    if (hFind == INVALID_HANDLE_VALUE) {
        return RemoveDirectoryA(path) ? 0 : -1;
    }

    do {
        if (strcmp(fdata.cFileName, ".") == 0 || strcmp(fdata.cFileName, "..") == 0)
            continue;

        char child[JFS_PATH_MAX];
        snprintf(child, sizeof(child), "%s\\%s", path, fdata.cFileName);

        if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            jfs_rmdir_exec(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(hFind, &fdata));
    FindClose(hFind);

    return RemoveDirectoryA(path) ? 0 : -1;
}

int jfs_rmdir(const char *dname)
{
    return jfs_rmdir_exec(dname);
}

