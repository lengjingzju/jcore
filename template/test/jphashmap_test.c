/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jpheap.h"
#include "jphashmap.h"

/* 运行的命令：./jhashmap.sh jphashmap "struct jphashmap" "Name" y
               并将Name结构体加入到生成的jphashmap.h里
typedef struct {
    unsigned int hash;
    const char *name;
} Name;
*/

#define HASHMAP_SHIFT       7
#define HASHMAP_NUM         (1u << HASHMAP_SHIFT)
#define HASHMAP_MASK        (HASHMAP_NUM - 1)

#define HASHMAP_ADD_SHIFT   9
#define HASHMAP_ADD_NUM     (1u << HASHMAP_ADD_SHIFT)
#define HASHMAP_ADD_MASK    (HASHMAP_ADD_NUM - 1)

#define HASHMAP_SUB_SHIFT   5
#define HASHMAP_SUB_NUM     (1u << HASHMAP_SUB_SHIFT)
#define HASHMAP_SUB_MASK    (HASHMAP_SUB_NUM - 1)

static struct jphashmap_pool s_hashmap_pool;
static struct jphashmap_root s_hashmap_root;

static unsigned int jphashmap_key_hash(const void *key)
{
    const unsigned char *name = (const unsigned char *)key;
    unsigned int len = strlen((const char *)key);
    unsigned int hash = 0;
    JHASHMAP_CODE_CALC(hash, name, len);
    return hash;
}

static unsigned jphashmap_node_hash(Name *node)
{
    return node->hash;
}

static int jphashmap_key_cmp(Name *node, const void *key)
{
    return (int)(node->hash - jphashmap_key_hash(key));
}

static int jphashmap_node_cmp(Name *a, Name *b)
{
    return (int)(a->hash - b->hash);
}

static unsigned long _jphashmap_print_cb(Name *node, unsigned int mask)
{
    printf("bucket: %3u, hash: %10u, name: %s\n",
        node->hash & mask, node->hash, node->name);
    return JHASHMAP_NEXT;
}

static unsigned long jphashmap_print_cb(Name *node)
{
    return _jphashmap_print_cb(node, HASHMAP_MASK);
}

static unsigned long jphashmap_add_print_cb(Name *node)
{
    return _jphashmap_print_cb(node, HASHMAP_ADD_MASK);
}

static unsigned long jphashmap_sub_print_cb(Name *node)
{
    return _jphashmap_print_cb(node, HASHMAP_SUB_MASK);
}

static unsigned long jphashmap_free_cb(Name *node)
{
    static struct jphashmap_pool *pool = &s_hashmap_pool;
    pool->free(pool, node);
    return JHASHMAP_DEL;
}

static void jphashmap_test_indexs(void)
{
    static struct jphashmap_pool *pool = &s_hashmap_pool;
    static struct jphashmap_root *root = &s_hashmap_root;

#define STR_NUM2     128
    Name *val = NULL;
    int i = 0;
    const char *strs2[STR_NUM2] = {
        // 水果类
        "apple", "banana", "orange", "grape", "strawberry",         // 基础水果
        "pineapple", "watermelon", "mango", "peach", "pear",        // 常见水果
        "cherry", "lemon", "lime", "kiwi", "pomegranate",           // 核果类
        "blueberry", "raspberry", "blackberry", "coconut", "fig",   // 浆果类
        "grapefruit", "guava", "lychee", "papaya", "plum",          // 热带水果
        "apricot", "quince", "dragonfruit", "jackfruit",            // 异域水果
        "kumquat", "mandarin", "nectarine", "plantain",             // 柑橘类变种
        "pomelo", "rambutan", "starfruit", "tangerine",             // 东南亚水果
        "persimmon", "mulberry", "passionfruit", "longan",          // 中国传统水果
        "loquat", "waxapple", "sugarcane", "durian",                // 特殊口感水果
        "custardapple", "breadfruit", "pitaya", "salak",            // 稀有品种
        "soursop", "mangosteen", "carambola", "santol",             // 东南亚特色
        "jujube", "hawthorn", "gooseberry", "elderberry",           // 药用水果
        "cloudberry", "boysenberry", "loganberry", "currant",       // 杂交品种
        "uglifruit", "tamarind", "feijoa", "ackee",                 // 特殊品类
        "miraclefruit", "cupuacu", "cempedak", "langsat",           // 热带珍稀

        // 蔬菜类
        "carrot", "potato", "tomato", "cucumber", "pumpkin",        // 根茎类
        "eggplant", "spinach", "lettuce", "celery", "cabbage",      // 叶菜类
        "broccoli", "cauliflower", "asparagus", "artichoke",        // 花菜类
        "zucchini", "radish", "beet", "turnip", "sweetpotato",      // 块茎类
        "ginger", "garlic", "onion", "leek", "shallot",             // 调味类
        "pepper", "chili", "okra", "bean", "pea",                   // 豆荚类
        "corn", "bambooshoot", "lotusroot", "taro", "yam",          // 水生/淀粉类
        "mushroom", "shiitake", "enoki", "oyster_mushroom",         // 菌菇类
        "kale", "chard", "arugula", "watercress", "endive",         // 绿叶菜
        "fennel", "parsley", "basil", "coriander", "dill",          // 香草类
        "bittergourd", "luffa", "chayote", "wintermelon",           // 瓜类
        "daikon", "kohlrabi", "rutabaga", "jicama",                 // 根茎变种
        "bok_choy", "napa_cabbage", "radicchio",                    // 十字花科
    };

    for (i = 0; i < STR_NUM2; ++i) {
        val = pool->alloc(pool);
        val->hash = jphashmap_key_hash(strs2[i]);
        val->name = strs2[i];
        jphashmap_fast_add(root, val);
    }

    printf("\nNew hashmap elements are:\n");
    jphashmap_loop(root, jphashmap_print_cb);

    printf("\nIncrease size, hashmap elements are:\n");
    jphashmap_resize(root, HASHMAP_ADD_SHIFT);
    jphashmap_loop(root, jphashmap_add_print_cb);

    printf("\nDecrease size, hashmap elements are:\n");
    jphashmap_resize(root, HASHMAP_SUB_SHIFT);
    jphashmap_loop(root, jphashmap_sub_print_cb);

    jphashmap_loop(root, jphashmap_free_cb);
}

int main(void)
{
#define STR_NUM     8
    static struct jphashmap_pool *pool = &s_hashmap_pool;
    static struct jphashmap_root *root = &s_hashmap_root;

    Name *val = NULL;
    Name *cur = NULL, *prev = NULL;
    int i = 0;
    const char *strs[STR_NUM] = {"name", "age", "gender", "culture", "address", "phone", "email", "hobby"};

    jphashmap_init_pool(pool, HASHMAP_NUM);

    root->key_hash = jphashmap_key_hash;
    root->node_hash = jphashmap_node_hash;
    root->key_cmp = jphashmap_key_cmp;
    root->node_cmp = jphashmap_node_cmp;
    jphashmap_init(root, HASHMAP_SHIFT, pool);

    for (i = 0; i < STR_NUM; ++i) {
        val = pool->alloc(pool);
        val->hash = jphashmap_key_hash(strs[i]);
        val->name = strs[i];
        jphashmap_fast_add(root, val);
    }

    printf("Priority hashmap elements are:\n");
    jphashmap_loop(root, jphashmap_print_cb);

    val = pool->alloc(pool);
    val->hash = jphashmap_key_hash("hobby");
    val->name = "hobby";
    cur = jphashmap_add(root, val);
    if (cur) {
        pool->free(pool, cur);
    }
    printf("Add same node(%s), find=%s, hashmap elements are:\n", "hobby", cur ? "true" : "false");
    jphashmap_loop(root, jphashmap_print_cb);

    cur = jphashmap_find(root, "culture", &prev);
    printf("Find node(%s), find=%s, hashmap elements are:\n", "culture", cur ? "true" : "false");

    if (cur) {
        jphashmap_del(root, cur, prev);
        pool->free(pool, cur);
    }
    printf("Del node(%s), hashmap elements are:\n", "culture");
    jphashmap_loop(root, jphashmap_print_cb);

    jphashmap_loop(root, jphashmap_free_cb);
    printf("Free all nodes, current number is %d:\n", pool->sel);

    jphashmap_test_indexs();

    jphashmap_uninit(root);
    jphashmap_uninit_pool(pool);

    return 0;
}
