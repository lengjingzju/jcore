/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <string.h>

extern void* __libc_malloc(size_t size);
extern void* __libc_realloc(void *ptr, size_t size);
extern void  __libc_free(void *ptr);

extern void jhook_addptr(void *ptr, size_t size, void *addr);
extern void jhook_delptr(void *ptr);
extern int jhook_tailnum(void);

void free(void *ptr)
{
    if (ptr) {
        jhook_delptr(ptr);
        __libc_free(ptr);
    }
}

void *malloc(size_t size)
{
    void *ptr = __libc_malloc(size + jhook_tailnum());

    if (!ptr)
        return NULL;
    jhook_addptr(ptr, size, __builtin_return_address(0));
    return ptr;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t tsize = nmemb * size;
    void *ptr = __libc_malloc(tsize + jhook_tailnum());

    if (!ptr)
        return NULL;
    memset(ptr, 0, tsize);
    jhook_addptr(ptr, tsize, __builtin_return_address(0));
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    jhook_delptr(ptr);
    ptr = __libc_realloc(ptr, size + jhook_tailnum());
    if (!ptr)
        return NULL;
    jhook_addptr(ptr, size, __builtin_return_address(0));
    return ptr;
}

char *strdup(const char *s)
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

char *strndup(const char *s, size_t n)
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
