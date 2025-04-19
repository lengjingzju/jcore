/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   动态字符串管理结构
 */
struct jstring {
    size_t size;                        // 当前元素数量
    size_t asize;                       // 容量每次增加的大小
    size_t capacity;                    // 当前容量
    char *data;                         // 数据存储指针
#define JSTRING_ASIZE_DEF   8u
};

/**
 * @brief   初始化动态字符串管理结构
 * @param   str [IN] 要初始化的动态字符串管理结构
 * @param   add_capacity [IN] 数组扩容默认增加大小
 * @param   init_capacity [IN] 数组容量初始大小
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jstring_init(struct jstring *str, size_t add_capacity, size_t init_capacity);

/**
 * @brief   反初始化动态字符串管理结构
 * @param   str [IN] 动态字符串管理结构
 * @return  无返回值
 * @note    无
 */
void jstring_uninit(struct jstring *str);

/**
 * @brief   调整动态字符串的容量
 * @param   str [IN] 动态字符串管理结构
 * @param   new_capacity [IN] 动态字符串新的容量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jstring_resize(struct jstring *str, size_t new_capacity);

/**
 * @brief   访问动态字符串的元素
 * @param   str [IN] 动态字符串管理结构
 * @param   index [IN] 要访问的序号
 * @return  返回字符值
 * @note    不检查index的合法性，index的合法性由用户保证，速度较快
 */
static inline char jstring_at_unsafe(struct jstring *str, size_t index)
{
    return str->data[index];
}

/**
 * @brief   访问动态字符串的元素
 * @param   str [IN] 动态字符串管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回字符值，失败返回'\0'
 * @note    检查index的合法性
 */
static inline char jstring_at(struct jstring *str, size_t index)
{
    if (index >= str->size)
        return '\0';
    return str->data[index];
}

/**
 * @brief   动态字符串末尾增加一个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   element [IN] 要增加的元素
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jstring_push(struct jstring *str, char element);

/**
 * @brief   动态字符串末尾删除一个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   element [OUT] 被删除的元素，可以为空
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jstring_pop(struct jstring *str, char *element);

/**
 * @brief   动态字符串末尾增加多个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jstring_pushs(struct jstring *str, const char *elements, size_t num);

/**
 * @brief   动态字符串末尾删除多个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   elements [OUT] 被删除的元素数组，可以为空
 * @param   num [INOUT] 被删除的元素数量
 * @return  成功返回0; 失败返回-1
 * @note    elements的大小要比num大1，字符串末尾会补'\0'
 */
int jstring_pops(struct jstring *str, char *elements, size_t *num);

/**
 * @brief   动态字符串任意位置增加一个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   element [IN] 要增加的元素
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于str->size
 */
int jstring_insert(struct jstring *str, char element, size_t index);

/**
 * @brief   动态字符串任意位置删除一个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于str->size
 */
int jstring_erase(struct jstring *str, size_t index);

/**
 * @brief   动态字符串任意位置增加多个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   elements [IN] 要增加的元素数组
 * @param   num [IN] 要增加的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于str->size
 */
int jstring_inserts(struct jstring *str, const char *elements, size_t num, size_t index);

/**
 * @brief   动态字符串任意位置删除多个元素
 * @param   str [IN] 动态字符串管理结构
 * @param   num [INOUT] 被删除的元素数量
 * @param   index [IN] 要访问的序号
 * @return  成功返回0; 失败返回-1
 * @note    index不能大于str->size
 */
int jstring_erases(struct jstring *str, size_t *num, size_t index);

#ifdef __cplusplus
}
#endif
