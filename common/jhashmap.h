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
 * @brief   记录活动节点的序号省内存标志
 * @note    如果是1，jhashmap_bucket记录序号使用short，最大支持的桶偏移为15
 *          32位系统打开此标志省内存才有效，改变此标志需要重新编译
 */
#ifndef JHASHMAP_SHORT_FLAG
#define JHASHMAP_SHORT_FLAG     0
#endif

/**
 * @brief   计算哈希值
 */
#ifndef JHASHMAP_CODE_CALC
#define JHASHMAP_CODE_CALC(hash, array, size) do { unsigned int i; hash = 0; for (i = 0; i < (size); ++i) hash = (hash << 5) - hash + (array)[i]; } while (0)
#endif

/**
 * @brief   从节点的单向链表成员指针获取节点的指针
 * @param   ptr [IN] 节点的链表成员指针
 * @param   type [IN] 节点结构体的类型名
 * @param   member [IN] 节点的链表成员名
 * @return  返回节点的指针
 * @note    无
 */
#define jhashmap_entry(ptr, type, member) ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * @brief   哈希表的节点结构
 */
struct jhashmap {
    struct jhashmap *next;                                  // 指向单链表下一个节点
};

/**
 * @brief   哈希表的桶结构
 */
struct jhashmap_bucket {
#ifdef JHASHMAP_SHORT_FLAG
    unsigned short next;                                    // 指向前一个桶序号
    unsigned short prev;                                    // 指向后一个桶序号
#else
    unsigned int next;                                      // 指向前一个桶序号
    unsigned int prev;                                      // 指向后一个桶序号
#endif
    struct jhashmap head;                                   // 挂载节点数据的单向链表
};

/**
 * @brief   哈希表的管理结构
 * @note    首次使用哈希表前一定要设置回调函数；
 *          哈希表的节点为单链表节点 `struct jhashmap` ；
 *          buckets多分配一个节点，用于存储有值桶的入口
 */
struct jhashmap_hd {
    unsigned int num;                                       // 节点的总数
    unsigned int bucket_shift;                              // 桶总数 1<<bucket_shift
    unsigned int bucket_num;                                // 桶总数
    unsigned int bucket_mask;                               // 桶总数-1，获取桶序号的hash值
    struct jhashmap_bucket *buckets;                        // 桶数组
    unsigned int (*key_hash)(const void *key);              // 通过key获取hash值
    unsigned int (*node_hash)(struct jhashmap *node);       // 通过节点获取hash值
    int (*key_cmp)(struct jhashmap *node, const void *key); // 节点和key的比较函数，键值相等返回0
    int (*node_cmp)(struct jhashmap *a, struct jhashmap *b);// 节点之间的比较函数，键值相等返回0
};

/**
 * @brief   哈希表初始化
 * @param   hd [INOUT] 哈希表句柄
 * @param   bucket_shift [IN] 桶的总数 1<<bucket_shift
 * @return  成功返回0; 失败返回-1
 * @note    首次使用哈希表前一定要设置回调函数
 */
int jhashmap_init(struct jhashmap_hd *hd, unsigned int bucket_shift);

/**
 * @brief   哈希表容量调整
 * @param   hd [INOUT] 哈希表句柄
 * @param   bucket_shift [IN] 新的桶的总数 1<<bucket_shift
 * @return  成功返回0; 失败返回-1不改变
 * @note    用于扩容缩容
 */
int jhashmap_resize(struct jhashmap_hd *hd, unsigned int bucket_shift);

/**
 * @brief   哈希表反初始化
 * @param   hd [INOUT] 哈希表句柄
 * @return  无返回值
 * @note    无
 */
void jhashmap_uninit(struct jhashmap_hd *hd);

/**
 * @brief   哈希表获取指定key的节点
 * @param   hd [IN] 哈希表句柄
 * @param   key [IN] 要查找的key
 * @param   prev [OUT] 找到节点的前一个节点，可以为空
 * @return  成功返回有效指针，失败返回空指针
 * @note    无
 */
struct jhashmap *jhashmap_find(struct jhashmap_hd *hd, const void *key, struct jhashmap **prev);

/**
 * @brief   哈希表获取和指定node相同key的节点
 * @param   hd [IN] 哈希表句柄
 * @param   key [IN] 要查找的node
 * @param   prev [OUT] 找到节点的前一个节点，可以为空
 * @return  成功返回有效指针，失败返回空指针
 * @note    无
 */
struct jhashmap *jhashmap_find_node(struct jhashmap_hd *hd, struct jhashmap *node, struct jhashmap **prev);

/**
 * @brief   哈希表增加节点
 * @param   hd [IN] 哈希表句柄
 * @param   node [IN] 增加的节点
 * @return  无返回值
 * @note    不检查key重复
 */
void jhashmap_fast_add(struct jhashmap_hd *hd, struct jhashmap *node);

/**
 * @brief   哈希表增加节点
 * @param   hd [IN] 哈希表句柄
 * @param   node [IN] 增加的节点
 * @return  有同key值的节点替换原节点并返回原节点；否则直接增加节点返回NULL
 * @note    检查key重复
 */
struct jhashmap *jhashmap_add(struct jhashmap_hd *hd, struct jhashmap *node);

/**
 * @brief   哈希表删除节点
 * @param   hd [IN] 哈希表句柄
 * @param   node [IN] 删除的节点
 * @param   prev [IN] 删除节点的前一个节点，可以为空
 * @return  成功返回0；找不到节点时失败返回-1
 * @note    无
 */
int jhashmap_del(struct jhashmap_hd *hd, struct jhashmap *node, struct jhashmap *prev);

/**
 * @brief   哈希表遍历回调函数"unsigned long (*cb)(struct jhashmap *node)"的返回值

 * @note    没有定义JHASHMAP_ADD，JHASHMAP_ADD返回新节点struct jhashmap的指针"(unsigned long)(ptr)"；
 *          JHASHMAP_ADD时需要保证桶序号相同；此时外部也不需要调用${name}_add增加
 */
#ifndef JHASHMAP_RET
#define JHASHMAP_RET    1
#define JHASHMAP_NEXT   0   // 表示继续循环
#define JHASHMAP_STOP   1   // 表示退出循环
#define JHASHMAP_DEL    2   // 表示删除cur节点，此时外部不用调用jhashmap_del删除
#endif

/**
 * @brief   哈希表遍历节点
 * @param   hd [IN] 哈希表句柄
 * @param   cb [IN] 回调函数
 * @return  无返回值
 * @note    注意prev节点可能不是有效的节点，而是head
 */
void jhashmap_loop(struct jhashmap_hd *hd, unsigned long (*cb)(struct jhashmap *node));

#ifdef __cplusplus
}
#endif
