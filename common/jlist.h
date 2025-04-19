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
 * @brief   双向循环链表的头和成员
 * @note    双向循环链表添加删除更简单，但是成员也是两个指针，更浪费空间
 */
struct jdlist_head {
    struct jdlist_head *next, *prev;
};

/**
 * @brief   判断双向循环链表是否为空
 * @param   head [IN] 链表头
 * @return  链表为空时返回真，否则返回假
 * @note    无
 */
#define jdlist_empty(head)              ((head)->next == (head))

/**
 * @brief   从节点的双向链表成员指针获取节点的指针
 * @param   ptr [IN] 节点的链表成员指针
 * @param   type [IN] 节点结构体的类型名
 * @param   member [IN] 节点的链表成员名
 * @return  返回节点的指针
 * @note    无
 */
#define jdlist_entry(ptr, type, member) ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * @brief   遍历双向循环链表的循环头
 * @param   pos [OUT] 被赋值当前节点的指针
 * @param   head [IN] 链表头
 * @param   member [IN] 节点的链表成员名
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，不能在循环体内部进行添加删除节点操作
 */
#define jdlist_for_each_entry(pos, head, member)                \
for (pos = jdlist_entry((head)->next, typeof(*pos), member);    \
    &pos->member != (head);                                     \
    pos = jdlist_entry(pos->member.next, typeof(*pos), member))

/**
 * @brief   安全遍历双向循环链表的循环头
 * @param   pos [OUT] 给他赋值当前节点的指针
 * @param   n [OUT] 给他赋值后一节点的指针
 * @param   head [IN] 链表头
 * @param   member [IN] 节点的链表成员名
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
#define jdlist_for_each_entry_safe(pos, n, head, member)        \
for (pos = jdlist_entry((head)->next, typeof(*pos), member),    \
    n = jdlist_entry(pos->member.next, typeof(*pos), member);   \
    &pos->member != (head);                                     \
    pos = n, n = jdlist_entry(n->member.next, typeof(*n), member))

/**
 * @brief   初始化双向循环链表头
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    无
 */
static inline void jdlist_init_head(struct jdlist_head *head)
{
    head->next = head;
    head->prev = head;
}

static inline void __jdlist_add(struct jdlist_head *_new,
   struct jdlist_head *prev, struct jdlist_head *next)
{
    next->prev = _new;
    _new->next = next;
    _new->prev = prev;
    prev->next = _new;
}

/**
 * @brief   加入新节点到指定节点后面
 * @param   _new [INOUT] 要加入的节点
 * @param   list [INOUT] 参考位置的节点
 * @return  无返回值
 * @note    无
 */
static inline void jdlist_add(struct jdlist_head *_new, struct jdlist_head *list)
{
    __jdlist_add(_new, list, list->next);
}

/**
 * @brief   加入新节点到指定节点前面
 * @param   _new [INOUT] 要加入的节点
 * @param   list [INOUT] 参考位置的节点
 * @return  无返回值
 * @note    无
 */
static inline void jdlist_add_tail(struct jdlist_head *_new, struct jdlist_head *list)
{
    __jdlist_add(_new, list->prev, list);
}

/**
 * @brief   从链表中删除指定节点
 * @param   _del [INOUT] 要删除的节点
 * @return  无返回值
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
static inline void jdlist_del(struct jdlist_head *_del)
{
    struct jdlist_head *prev = _del->prev;
    struct jdlist_head *next = _del->next;
    next->prev = prev;
    prev->next = next;
    jdlist_init_head(_del);
}

/**
 * @brief   单向循环链表的成员
 * @note    单向循环链表添加删除稍显复杂，但是只有一个指针，更省空间
 */
struct jslist {
    struct jslist *next;
};

/**
 * @brief   单向循环链表的头
 * @note    单向循环链表多记录prev是为了添加到链表尾部更快速
 */
struct jslist_head {
    struct jslist *next, *prev;
};

/**
 * @brief   判断单向循环链表是否为空
 * @param   head [IN] 链表头
 * @return  链表为空时返回真，否则返回假
 * @note    无
 */
#define jslist_empty(head)              ((head)->next == (struct jslist *)(head))

/**
 * @brief   从节点的单向链表成员指针获取节点的指针
 * @param   ptr [IN] 节点的链表成员指针
 * @param   type [IN] 节点结构体的类型名
 * @param   member [IN] 节点的链表成员名
 * @return  返回节点的指针
 * @note    无
 */
#define jslist_entry(ptr, type, member) ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * @brief   遍历单向循环链表的循环头
 * @param   pos [OUT] 被赋值当前节点的指针
 * @param   head [IN] 链表头
 * @param   member [IN] 节点的链表成员名
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，不能在循环体内部进行添加删除节点操作
 */
#define jslist_for_each_entry(pos, head, member)                \
for (pos = jslist_entry((head)->next, typeof(*pos), member);    \
    &pos->member != (struct jslist *)(head);                    \
    pos = jslist_entry(pos->member.next, typeof(*pos), member))

/**
 * @brief   安全遍历单向循环链表的循环头
 * @param   p [OUT] 给他赋值前一节点的指针
 * @param   pos [OUT] 给他赋值当前节点的指针
 * @param   n [OUT] 给他赋值后一节点的指针
 * @param   head [IN] 链表头
 * @param   member [IN] 节点的链表成员名
 * @return  不是函数，无返回概念
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 *          注意修改时注意重新赋值，例如删除pos节点需要再循环体内部设置"pos = p;"
 */
#define jslist_for_each_entry_safe(p, pos, n, head, member)     \
for (p = jslist_entry((head), typeof(*pos), member),            \
    pos = jslist_entry((head)->next, typeof(*pos), member),     \
    n = jslist_entry(pos->member.next, typeof(*pos), member);   \
    &pos->member != (struct jslist *)(head);                    \
    p = pos, pos = n, n = jslist_entry(n->member.next, typeof(*n), member))

/**
 * @brief   初始化单向循环链表头
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    无
 */
static inline void jslist_init_head(struct jslist_head *head)
{
    head->next = (struct jslist *)head;
    head->prev = (struct jslist *)head;
}

/**
 * @brief   在链表首部加入新节点
 * @param   _new [INOUT] 要加入的节点
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    无
 */
static inline void jslist_add_head(struct jslist *_new, struct jslist_head *head)
{
    if (head->next == (struct jslist *)head) {
        head->prev = _new;
    }
    _new->next = head->next;
    head->next = _new;
}

/**
 * @brief   在链表尾部加入新节点
 * @param   _new [INOUT] 要加入的节点
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    无
 */
static inline void jslist_add_head_tail(struct jslist *_new, struct jslist_head *head)
{
    _new->next = head->prev->next;
    head->prev->next = _new;
    head->prev = _new;
}

/**
 * @brief   在链表指定节点的后边加入新节点
 * @param   _new [INOUT] 要加入的节点
 * @param   prev [INOUT] 参考位置的节点
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    无
 */
static inline void jslist_add(struct jslist *_new, struct jslist *prev, struct jslist_head *head)
{
    if (prev->next == (struct jslist *)head) {
        head->prev = _new;
    }
    _new->next = prev->next;
    prev->next = _new;
}

/**
 * @brief   从链表中删除第一个节点
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
static inline void jslist_del_head(struct jslist_head *head)
{
    struct jslist *_del = head->next;
    if (_del->next == (struct jslist *)head) {
        head->prev = (struct jslist *)head;
    }
    head->next = _del->next;
    _del->next = NULL;
}

/**
 * @brief   从链表中删除指定节点
 * @param   _del [INOUT] 要删除的节点
 * @param   prev [INOUT] 要删除的节点的前一节点
 * @param   head [INOUT] 链表头
 * @return  无返回值
 * @note    一般是操作pos，可以在循环体内部进行添加删除节点操作
 */
static inline void jslist_del(struct jslist *_del, struct jslist *prev, struct jslist_head *head)
{
    if (_del->next == (struct jslist *)head) {
        head->prev = prev;
    }
    prev->next = _del->next;
    _del->next = NULL;
}

#ifdef __cplusplus
}
#endif
