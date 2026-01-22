/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jheap.h"
#include "jpheap.h"
#include "jphashmap.h"

typedef struct {
    unsigned int hash;
    const char *name;
} Name;

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

static unsigned jphashmap_node_hash(void **node)
{
    return (*(Name **)node)->hash;
}

static int jphashmap_key_cmp(void **node, const void *key)
{
    return (int)((*(Name **)node)->hash - jphashmap_key_hash(key));
}

static int jphashmap_node_cmp(void **a, void **b)
{
    return (int)((*(Name **)a)->hash - (*(Name **)b)->hash);
}

static uintptr_t _jphashmap_print_cb(void **node, unsigned int mask)
{
    Name *data = *(Name **)node;
    printf("bucket: %3u, hash: %10u, name: %s\n",
        data->hash & mask, data->hash, data->name);
    return JHASHMAP_NEXT;
}

static uintptr_t jphashmap_print_cb(void **node)
{
    return _jphashmap_print_cb(node, HASHMAP_MASK);
}

static uintptr_t jphashmap_add_print_cb(void **node)
{
    return _jphashmap_print_cb(node, HASHMAP_ADD_MASK);
}

static uintptr_t jphashmap_sub_print_cb(void **node)
{
    return _jphashmap_print_cb(node, HASHMAP_SUB_MASK);
}

static uintptr_t jphashmap_free_cb(void **node)
{
    static struct jphashmap_pool *pool = &s_hashmap_pool;
    jheap_free(*(Name **)node);
    pool->free(pool, node);
    return JHASHMAP_DEL;
}

static void jphashmap_test_indexs(void)
{
    static struct jphashmap_pool *pool = &s_hashmap_pool;
    static struct jphashmap_root *root = &s_hashmap_root;

#define STR_NUM2     128
    Name *val = NULL;
    void **data = NULL;
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
        val = (Name *)jheap_malloc(sizeof(Name));
        val->hash = jphashmap_key_hash(strs2[i]);
        val->name = strs2[i];
        data = pool->alloc(pool);
        *(Name **)data = val;
        jphashmap_fast_add(root, data);
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
    void **data = NULL;
    void **cur = NULL, **prev = NULL;
    int i = 0;
    const char *strs[STR_NUM] = {"name", "age", "gender", "culture", "address", "phone", "email", "hobby"};

    jphashmap_init_pool(pool, HASHMAP_NUM);

    root->key_hash = jphashmap_key_hash;
    root->node_hash = jphashmap_node_hash;
    root->key_cmp = jphashmap_key_cmp;
    root->node_cmp = jphashmap_node_cmp;
    jphashmap_init(root, HASHMAP_SHIFT, pool);

    for (i = 0; i < STR_NUM; ++i) {
        val = (Name *)jheap_malloc(sizeof(Name));
        val->hash = jphashmap_key_hash(strs[i]);
        val->name = strs[i];
        data = pool->alloc(pool);
        *(Name **)data = val;
        jphashmap_fast_add(root, data);
    }

    printf("Priority hashmap elements are:\n");
    jphashmap_loop(root, jphashmap_print_cb);

    val = (Name *)jheap_malloc(sizeof(Name));
    val->hash = jphashmap_key_hash("hobby");
    val->name = "hobby";
    data = pool->alloc(pool);
    *(Name **)data = val;
    cur = jphashmap_add(root, data);
    if (cur) {
        jheap_free(*(Name **)cur);
        pool->free(pool, cur);
    }
    printf("Add same node(%s), find=%s, hashmap elements are:\n", "hobby", cur ? "true" : "false");
    jphashmap_loop(root, jphashmap_print_cb);

    cur = jphashmap_find(root, "culture", &prev);
    printf("Find node(%s), find=%s, hashmap elements are:\n", "culture", cur ? "true" : "false");
    jphashmap_loop(root, jphashmap_print_cb);

    if (cur) {
        jphashmap_del(root, cur, prev);
        jheap_free(*(Name **)cur);
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
