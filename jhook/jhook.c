/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <string.h>
#include <stdbool.h>

#ifndef JHOOK_WRAP
#define JHOOK_WRAP      0
#endif

#if JHOOK_WRAP
#define jhook_free      __wrap_free
#define jhook_malloc    __wrap_malloc
#define jhook_calloc    __wrap_calloc
#define jhook_realloc   __wrap_realloc
#define jhook_strdup    __wrap_strdup
#define jhook_strndup   __wrap_strndup
#else
#define jhook_free      free
#define jhook_malloc    malloc
#define jhook_calloc    calloc
#define jhook_realloc   realloc
#define jhook_strdup    strdup
#define jhook_strndup   strndup
#endif

extern void* __libc_malloc(size_t size);
extern void* __libc_realloc(void *ptr, size_t size);
extern void  __libc_free(void *ptr);

extern void jhook_addptr(void *ptr, size_t size, void *addr);
extern void jhook_delptr(void *ptr, bool del_node);
extern int jhook_tailnum(void);

void jhook_free(void *ptr)
{
    if (ptr) {
        jhook_delptr(ptr, false);
        __libc_free(ptr);
    }
}

void *jhook_malloc(size_t size)
{
    void *ptr = __libc_malloc(size + jhook_tailnum());

    if (!ptr)
        return NULL;
    jhook_addptr(ptr, size, __builtin_return_address(0));
    return ptr;
}

void *jhook_calloc(size_t nmemb, size_t size)
{
    size_t tsize = nmemb * size;
    void *ptr = __libc_malloc(tsize + jhook_tailnum());

    if (!ptr)
        return NULL;
    memset(ptr, 0, tsize);
    jhook_addptr(ptr, tsize, __builtin_return_address(0));
    return ptr;
}

void *jhook_realloc(void *ptr, size_t size)
{
    jhook_delptr(ptr, true);
    ptr = __libc_realloc(ptr, size + jhook_tailnum());
    if (!ptr)
        return NULL;
    jhook_addptr(ptr, size, __builtin_return_address(0));
    return ptr;
}

char *jhook_strdup(const char *s)
{
    size_t size = strlen(s) + 1;
    char *ptr = (char *)__libc_malloc(size + jhook_tailnum());
    if (!ptr)
        return NULL;
    memcpy(ptr, s, size - 1);
    ptr[size - 1] = '\0';
    jhook_addptr((void *)ptr, size, __builtin_return_address(0));
    return ptr;
}

char *jhook_strndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    size_t size = (n < len ? n : len) + 1;
    char *ptr = (char *)__libc_malloc(size + jhook_tailnum());

    if (!ptr)
        return NULL;
    memcpy(ptr, s, size - 1);
    ptr[size - 1] = '\0';
    jhook_addptr((void *)ptr, size, __builtin_return_address(0));
    return ptr;
}
