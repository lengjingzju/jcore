/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 红黑树的特性:
 * 1. 节点非黑即红
 * 2. 根节点是黑色
 * 3. 叶节点是黑色(叶子节点指最底端的节点NIL，指空节点)
 * 4. 红节点的子节点必须是黑色(路径中不能有两个连续的红节点)
 * 5. 从一个节点到该节点的子孙节点的所有路径上包含相同数目的黑节点(确保没有一条路径会比其他路径长出俩倍)
 */

/**
 * @brief   红黑树的节点结构
 */
struct jrbtree {
    struct jrbtree  *right_son;
    struct jrbtree  *left_son;
    uintptr_t        parent_color;
};

/**
 * @brief   红黑树的根
 * @note    首次使用红黑树前一定要设置num和head为空，且设置回调函数
 *          销毁红黑数全部节点再次使用红黑树一定要设置num和head为空
 */
struct jrbtree_root {
    int num;                // 节点的总数
    struct jrbtree *head;   // 红黑树的根节点，使用红黑树的入口
    int (*key_cmp)(struct jrbtree *node, const void *key);  // 节点和key的比较函数
    int (*node_cmp)(struct jrbtree *a, struct jrbtree *b);  // 节点之间的比较函数
};

/**
 * @brief   从节点的红黑树成员指针获取节点的指针
 * @param   ptr [IN] 节点的红黑树成员指针
 * @param   type [IN] 节点结构体的类型名
 * @param   member [IN] 节点的红黑树成员名
 * @return  返回节点的指针
 * @note    无
 */
#define jrbtree_entry(ptr, type, member) ((type *)((char *)(ptr)-(uintptr_t)(&((type *)0)->member)))

/**
 * @brief   加入节点到红黑树
 * @param   root [IN] 红黑树的根
 * @param   node [INOUT] 要加入的节点
 * @return  成功返回0; 失败表示节点有冲突返回-1
 * @note    一定要设置node_cmp回调函数
 */
int jrbtree_add(struct jrbtree_root *root, struct jrbtree *node);

/**
 * @brief   从红黑树删除节点
 * @param   root [IN] 红黑树的根
 * @param   node [INOUT] 要删除的节点
 * @return  无返回值
 * @note    无
 */
void jrbtree_del(struct jrbtree_root *root, struct jrbtree *node);

/**
 * @brief   替换红黑树的旧节点为新节点
 * @param   root [IN] 红黑树的根
 * @param   old [IN] 被替换的节点
 * @param   node [INOUT] 新的节点
 * @return  无返回值
 * @note    old和node的key值必须相等
 */
void jrbtree_replace(struct jrbtree_root *root, struct jrbtree *old, struct jrbtree *node);

/**
 * @brief   查询红黑树指定key值的节点
 * @param   root [IN] 红黑树的根
 * @param   key [IN] 查询的key
 * @return  返回找到的节点
 * @note    一定要设置key_cmp回调函数
 */
struct jrbtree *jrbtree_search(struct jrbtree_root *root, const void *key);

/**
 * @brief   查询红黑树key值最小的节点
 * @param   root [IN] 红黑树的根
 * @return  返回找到的节点
 * @note    无
 */
struct jrbtree *jrbtree_first(const struct jrbtree_root *root);

/**
 * @brief   查询红黑树key值最大的节点
 * @param   root [IN] 红黑树的根
 * @return  返回找到的节点
 * @note    无
 */
struct jrbtree *jrbtree_last(const struct jrbtree_root *root);

/**
 * @brief   查询红黑树node节点的下一节点
 * @param   node [IN] 参考的节点
 * @return  返回找到的节点
 * @note    无
 */
struct jrbtree *jrbtree_next(const struct jrbtree *node);

/**
 * @brief   查询红黑树node节点的上一节点
 * @param   node [IN] 参考的节点
 * @return  返回找到的节点
 * @note    无
 */
struct jrbtree *jrbtree_prev(const struct jrbtree *node);

#ifdef __cplusplus
}
#endif
