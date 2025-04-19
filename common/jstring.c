/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <string.h>
#include "jheap.h"
#include "jstring.h"

int jstring_init(struct jstring *str, size_t add_capacity, size_t init_capacity)
{
    str->size = 0;
    str->asize = add_capacity ? add_capacity : JSTRING_ASIZE_DEF;
    str->capacity = init_capacity ? init_capacity : str->asize + 1;
    str->data = (char *)jheap_malloc(str->capacity);
    if (!str->data) {
        str->capacity = 0;
        return -1;
    }
    str->data[0] = 0;
    return 0;
}

void jstring_uninit(struct jstring *str)
{
    if (str->data) {
        free(str->data);
        str->data = NULL;
    }
    str->size = 0;
    str->capacity = 0;
}

static inline int jstring_incsize(struct jstring *str, size_t new_capacity)
{
    char *new_data = (char *)jheap_realloc(str->data, new_capacity + 1);
    if (!new_data)
        return -1;
    str->capacity = new_capacity + 1;
    str->data = new_data;
    return 0;
}

int jstring_resize(struct jstring *str, size_t new_capacity)
{
    if (new_capacity < str->capacity) {
        str->data = (char *)jheap_realloc(str->data, new_capacity + 1);
        str->capacity = new_capacity + 1;
        if (str->size > new_capacity) {
            str->size = new_capacity;
            str->data[str->size] = 0;
        }
    } else if (new_capacity > str->capacity) {
        return jstring_incsize(str, new_capacity);
    }
    return 0;
}

int jstring_push(struct jstring *str, char element)
{
    if (str->size + 1 >= str->capacity) {
        if (jstring_incsize(str, str->size + 1 + str->asize) < 0)
            return -1;
    }
    str->data[str->size] = element;
    str->data[++str->size] = 0;

    return 0;
}

int jstring_pop(struct jstring *str, char *element)
{
    if (!str->size)
        return -1;
    if (element)
        *element = str->data[str->size - 1];
    str->data[--str->size] = 0;
    return 0;
}

int jstring_pushs(struct jstring *str, const char *elements, size_t num)
{
    if (str->size + num + 1 > str->capacity) {
        if (jstring_incsize(str, str->size + num + str->asize) < 0)
            return -1;
    }
    memcpy(str->data + str->size, elements, num);
    str->size += num;
    str->data[str->size] = 0;
    return 0;
}

int jstring_pops(struct jstring *str, char *elements, size_t *num)
{
    if (!str->size) {
        *num = 0;
        return -1;
    }

    if (*num >= str->size) {
        *num = str->size;
        str->size = 0;
        if (elements) {
            memcpy(elements, str->data, *num);
            elements[*num] = '\0';
        }
        str->data[str->size] = 0;
    } else {
        str->size -= *num;
        if (elements) {
            memcpy(elements, str->data + str->size, *num);
            elements[*num] = '\0';
        }
        str->data[str->size] = 0;
    }
    return 0;
}

int jstring_insert(struct jstring *str, char element, size_t index)
{
    if (index > str->size)
        return -1;
    if (str->size + 1 >= str->capacity) {
        if (jstring_incsize(str, str->size + 1 + str->asize) < 0)
            return -1;
    }

    if (index == str->size) {
        str->data[str->size] = element;
        str->data[++str->size] = 0;
    } else {
        size_t diff = str->size - index;
        char *src = str->data + index;
        char *dst = src + 1;
        memmove(dst, src, diff);
        *src = element;
        str->data[++str->size] = 0;
    }
    return 0;
}

int jstring_erase(struct jstring *str, size_t index)
{
    if (index >= str->size)
        return -1;

    if (index == str->size - 1) {
        str->data[--str->size] = 0;
    } else {
        size_t diff = str->size - index - 1;
        char *dst = str->data + index;
        char *src = dst + 1;
        memmove(dst, src, diff);
        str->data[--str->size] = 0;
    }
    return 0;
}

int jstring_inserts(struct jstring *str, const char *elements, size_t num, size_t index)
{
    if (index > str->size)
        return -1;
    if (str->size + num + 1 > str->capacity) {
        if (jstring_incsize(str, str->size + num + str->asize) < 0)
            return -1;
    }

    if (index == str->size) {
        memcpy(str->data + str->size, elements, num);
        str->size += num;
        str->data[str->size] = 0;
    } else  {
        size_t diff = str->size - index;
        char *src = str->data + index;
        char *dst = src + num;
        memmove(dst, src, diff);
        memcpy(src, elements, num);
        str->size += num;
        str->data[str->size] = 0;
    }
    return 0;
}

int jstring_erases(struct jstring *str, size_t *num, size_t index)
{
    if (index >= str->size) {
        *num = 0;
        return -1;
    }

    if (*num + index >= str->size) {
        *num = str->size - index;
        str->size = index;
        str->data[str->size] = 0;
    } else {
        size_t diff = str->size - index - *num;
        char *dst = str->data + index;
        char *src = dst + *num;
        memmove(dst, src, diff);
        str->size -= *num;
        str->data[str->size] = 0;
    }
    return 0;
}
