/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JFS_SP                          '/'
#define JFS_RSP                         '\\'

/**
 * @brief   检查并修改适应当前系统的路径分隔符
 * @buf     buf [INOUT] 要修改的路径
 * @return  无返回值
 * @note    JFS_SP是本系统的路径分隔符，JFS_RSP是windows的路径分隔符
 */
static inline void jfs_sp_adapted(char *buf)
{
    while (*buf) {
        if (*buf == JFS_RSP)
            *buf = JFS_SP;
        ++buf;
    }
}

/**
 * @brief   写入到标准输出
 */
#define JFS_WROUT(buf, count)           write(STDOUT_FILENO, buf, count);

/**
 * @brief   写入到标准错误
 */
#define JFS_WRERR(buf, count)           write(STDERR_FILENO, buf, count);

/**
 * @brief   修改偏移的相对位置
 */
#define JFS_SEEK_SET                    SEEK_SET    // 文件开头
#define JFS_SEEK_CUR                    SEEK_CUR    // 当前位置
#define JFS_SEEK_END                    SEEK_END    // 文件结尾

typedef int jfs_fd_t;
#define JFS_INVALID_FD                  (-1)
#define jfs_fd_valid(fd)                ((fd) >= 0)

/**
 * @brief   关闭文件
 */
#define jfs_close(fd)                   do { if ((fd) >= 0) close(fd); (fd) = -1; } while (0)

/**
 * @brief   打开文件
 * @param   fname [IN] 要打开的文件名
 * @param   mode [IN] 打开的模式，只有"r" "w" "a" "r+" "w+" "a+"六种类型
 * @return  成功返回文件描述符; 失败返回JFS_INVALID_FD
 * @note    无
 */
static inline jfs_fd_t jfs_open(const char *fname, const char *mode)
{
    int oflag = 0;
    mode_t omode = 0;

    switch (mode[0]) {
    case 'r':
        oflag = mode[1] == '+' ? O_RDWR : O_RDONLY;
        break;
    case 'w':
        oflag = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        omode = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
        break;
    case 'a':
        oflag = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        omode = S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH;
        break;
    default:
        oflag = O_RDONLY;
        break;
    }

    return omode ? open(fname, oflag, omode) : open(fname, oflag);
}

/**
 * @brief   读文件
 */
#define jfs_read(fd, buffer, count)     read(fd, buffer, count)

/**
 * @brief   写文件
 */
#define jfs_write(fd, buffer, count)    write(fd, buffer, count)

/**
 * @brief   修改文件偏移
 */
#define jfs_lseek(fd, offset, whence)   lseek(fd, offset, whence)

/**
 * @brief   同步文件到磁盘
 */
#define jfs_fsync(fd)                   fsync(fd)

/**
 * @brief   截断文件
 */
#define jfs_ftruncate(fd, length)       fftruncate(fd, length)

/**
 * @brief   删除文件
 */
#define jfs_rmfile(fname)               unlink(fname)

/**
 * @brief   读取文件所有内容到buffer
 * @param   fname [IN] 要读取的文件名
 * @param   buf [OUT] 存储文件buf的指针的指针
 * @param   count [OUT] 文件的长度
 * @return  成功返回0; 失败返回-1
 * @note    会在函数内部给buf分配内存，buf的大小比文件长度大1，且末尾置为了'\0'
 */
int jfs_readall(const char *fname, char **buf, size_t *count);

/**
 * @brief   释放读取的buffer
 * @param   buf [INOUT] 存储文件buf的指针的指针
 * @param   count [INOUT] 文件的长度
 * @return  无返回值
 * @note    释放jfs_readall分配的内存
 */
void jfs_readfree(char **buf, size_t *count);

/**
 * @brief   将buffer的内容写入到文件
 * @param   fname [IN] 要写入的文件名
 * @param   buf [IN] 要写入的buffer
 * @param   count [IN] 要写入的buffer长度
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jfs_writeall(const char *fname, const char *buf, size_t count);

#define JFS_NOTEXISTED                  0   // 文件不存在
#define JFS_ISFILE                      1   // 是普通文件
#define JFS_ISDIR                       2   // 是目录
#define JFS_ISLINK                      4   // 是符号链接
#define JFS_ISOTHER                     8   // 其它类型

/**
 * @brief   获取文件类型
 * @param   path [IN] 要检查的文件名
 * @return  返回文件类型
 * @note    无
 */
static inline unsigned int jfs_gettype(const char *path)
{
    struct stat st;
    unsigned int type = JFS_NOTEXISTED;

    if (lstat(path, &st) < 0)
        return type;

    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        type = JFS_ISFILE;
        break;
    case S_IFDIR:
        type = JFS_ISDIR;
        break;
    case S_IFLNK:
        type = JFS_ISLINK;
        break;
    default:
        type = JFS_ISOTHER;
        break;
    }

    return type;
}

/**
 * @brief   判断文件是否存在
 */
#define jfs_existed(path)               (access(path, F_OK) == 0)

/**
 * @brief   重命名文件
 */
#define jfs_rename(oldpath, newpath)    rename(oldpath, newpath)

/**
 * @brief   目录中文件条目的结构体
 */
typedef struct {
    char *name;         // 文件或文件夹名
    unsigned int len;   // 文件名长度
    unsigned int type;  // 文件类型
    off_t size;         // 文件大小
    time_t mtime;       // 文件修改时间
} jfs_dirent_t;

/**
 * @brief   文件条目排序方法，R表示反序
 */
#define JFS_SORT_BY_DIR                 (1)
#define JFS_SORT_BY_RDIR                (2)
#define JFS_SORT_BY_NAME                (0)
#define JFS_SORT_BY_RNAME               (1 << 4)
#define JFS_SORT_BY_SIZE                (2 << 4)
#define JFS_SORT_BY_RSIZE               (3 << 4)
#define JFS_SORT_BY_TIME                (4 << 4)
#define JFS_SORT_BY_RTIME               (5 << 4)

/**
 * @brief   过滤的回调函数
 * @param   fname [IN] 文件名
 * @param   tlen [IN] 文件名长度，不含'\0'
 * @param   ftype [IN] 文件类型
 * @return  保留返回1; 过滤返回0
 * @note    无
 */
typedef int (*jfs_filter_cb)(const char *fname, unsigned int tlen, unsigned int ftype);

/**
 * @brief   计算文件夹中文件条目数
 * @param   dname [IN] 文件夹路径名
 * @param   filter [IN] 过滤回调函数
 * @return  成功返回文件条目数; 失败返回-1
 * @note    不含"."和".."文件
 */
int jfs_countdir(const char *dname, jfs_filter_cb filter);

/**
 * @brief   获取文件夹中所有的文件条目
 * @param   dname [IN] 文件夹路径名
 * @param   dirs [OUT] 保存的文件条目
 * @param   num [OUT] 文件条目的数量
 * @param   filter [IN] 过滤回调函数
 * @return  成功返回0; 失败返回-1
 * @note    不含"."和".."文件
 */
int jfs_listdir(const char *dname, jfs_dirent_t **dirs, int *num, jfs_filter_cb filter);

/**
 * @brief   释放文件条目资源
 * @param   dirs [INOUT] 文件条目的指针的指针
 * @param   num [INOUT] 文件条目的数量
 * @return  无返回值
 * @note    释放jfs_listdir分配的内存
 */
void jfs_freedir(jfs_dirent_t **dirs, int *num);

/**
 * @brief   对文件条目进行排序
 * @param   dirs [INOUT] 文件条目的数组
 * @param   num [IN] 文件条目的数量
 * @param   opt [IN] 排序方法
 * @return  无返回值
 * @note    JFS_SORT_BY_DIR和JFS_SORT_BY_RDIR可以和后面的排序方法组合，例如 JFS_SORT_BY_DIR | JFS_SORT_BY_NAME
 */
void jfs_sortdir(jfs_dirent_t *dirs, int num, unsigned int opt);

/**
 * @brief   创建文件夹
 * @param   dname [IN] 文件夹路径名
 * @return  成功返回0; 失败返回-1
 * @note    上层文件不存在会创建上层文件夹
 */
int jfs_mkdir(const char *dname);

/**
 * @brief   删除文件夹
 * @param   dname [IN] 文件夹路径名
 * @return  成功返回0; 失败返回-1
 * @note    文件夹中有内容也会递归删除
 */
int jfs_rmdir(const char *dname);

#ifdef __cplusplus
}
#endif
