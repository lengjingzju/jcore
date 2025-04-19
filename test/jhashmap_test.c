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
#include "jhashmap.h"

#define HASHMAP_SHIFT       7
#define HASHMAP_NUM         (1u << HASHMAP_SHIFT)
#define HASHMAP_MASK        (HASHMAP_NUM - 1)

#define HASHMAP_ADD_SHIFT   9
#define HASHMAP_ADD_NUM     (1u << HASHMAP_ADD_SHIFT)
#define HASHMAP_ADD_MASK    (HASHMAP_ADD_NUM - 1)

#define HASHMAP_SUB_SHIFT   5
#define HASHMAP_SUB_NUM     (1u << HASHMAP_SUB_SHIFT)
#define HASHMAP_SUB_MASK    (HASHMAP_SUB_NUM - 1)

static jpheap_mgr_t s_pheap;

typedef struct {
    struct jhashmap list;
    unsigned int hash;
    const char *name;
} Name;

static unsigned int jhashmap_key_hash(const void *key)
{
    const unsigned char *name = (const unsigned char *)key;
    unsigned int len = strlen((const char *)key);
    unsigned int hash = 0;
    JHASHMAP_CODE_CALC(hash, name, len);
    return hash;
}

static unsigned jhashmap_node_hash(struct jhashmap *node)
{
    Name *tnode = jhashmap_entry(node, Name, list);
    return tnode->hash;
}

static int jhashmap_key_cmp(struct jhashmap *node, const void *key)
{
    Name *tnode = jhashmap_entry(node, Name, list);
    return (int)(tnode->hash - jhashmap_key_hash(key));
}

static int jhashmap_node_cmp(struct jhashmap *a, struct jhashmap *b)
{
    Name *ta = jhashmap_entry(a, Name, list);
    Name *tb = jhashmap_entry(b, Name, list);
    return (int)(ta->hash - tb->hash);
}


static unsigned long _jhashmap_print_cb(struct jhashmap *node, unsigned int mask)
{
    Name *tnode = jhashmap_entry(node, Name, list);
    printf("bucket: %3u, hash: %10u, name: %s\n",
        tnode->hash & mask, tnode->hash, tnode->name);
    return JHASHMAP_NEXT;
}

static unsigned long jhashmap_print_cb(struct jhashmap *node)
{
    return _jhashmap_print_cb(node, HASHMAP_MASK);
}

static unsigned long jhashmap_add_print_cb(struct jhashmap *node)
{
    return _jhashmap_print_cb(node, HASHMAP_ADD_MASK);
}

static unsigned long jhashmap_sub_print_cb(struct jhashmap *node)
{
    return _jhashmap_print_cb(node, HASHMAP_SUB_MASK);
}

static unsigned long jhashmap_free_cb(struct jhashmap *node)
{
    Name *tnode = jhashmap_entry(node, Name, list);
    jpheap_free(&s_pheap, tnode);
    return JHASHMAP_DEL;
}

static void jhashmap_test_indexs(struct jhashmap_hd *hd)
{
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
        val = (Name *)jpheap_alloc(&s_pheap);
        val->hash = jhashmap_key_hash(strs2[i]);
        val->name = strs2[i];
        jhashmap_fast_add(hd, &val->list);
    }

    printf("\nNew hashmap elements are:\n");
    jhashmap_loop(hd, jhashmap_print_cb);

    printf("\nIncrease size, hashmap elements are:\n");
    jhashmap_resize(hd, HASHMAP_ADD_SHIFT);
    jhashmap_loop(hd, jhashmap_add_print_cb);

    printf("\nDecrease size, hashmap elements are:\n");
    jhashmap_resize(hd, HASHMAP_SUB_SHIFT);
    jhashmap_loop(hd, jhashmap_sub_print_cb);

    jhashmap_loop(hd, jhashmap_free_cb);
}

int main(void)
{
#define STR_NUM     8
    struct jhashmap_hd hd;
    Name *val = NULL;
    struct jhashmap *cur = NULL, *prev = NULL;
    int i = 0;
    const char *strs[STR_NUM] = {"name", "age", "gender", "culture", "address", "phone", "email", "hobby"};

    hd.key_hash = jhashmap_key_hash;
    hd.node_hash = jhashmap_node_hash;
    hd.key_cmp = jhashmap_key_cmp;
    hd.node_cmp = jhashmap_node_cmp;
    jhashmap_init(&hd, HASHMAP_SHIFT);

    s_pheap.size = sizeof(Name);
    s_pheap.num = HASHMAP_NUM;
    jpheap_init(&s_pheap);

    for (i = 0; i < STR_NUM; ++i) {
        val = (Name *)jpheap_alloc(&s_pheap);
        val->hash = jhashmap_key_hash(strs[i]);
        val->name = strs[i];
        jhashmap_fast_add(&hd, &val->list);
    }

    printf("Priority hashmap elements are:\n");
    jhashmap_loop(&hd, jhashmap_print_cb);

    val = (Name *)jpheap_alloc(&s_pheap);
    val->hash = jhashmap_key_hash("hobby");
    val->name = "hobby";
    cur = jhashmap_add(&hd, &val->list);
    if (cur) {
        val = jhashmap_entry(cur, Name, list);
        jpheap_free(&s_pheap, val);
    }
    printf("Add same node(%s), find=%s, hashmap elements are:\n", "hobby", cur ? "true" : "false");
    jhashmap_loop(&hd, jhashmap_print_cb);

    cur = jhashmap_find(&hd, "culture", &prev);
    printf("Find node(%s), find=%s, hashmap elements are:\n", "culture", cur ? "true" : "false");

    if (cur) {
        jhashmap_del(&hd, cur, prev);
        val = jhashmap_entry(cur, Name, list);
        jpheap_free(&s_pheap, val);
    }
    printf("Del node(%s), hashmap elements are:\n", "culture");
    jhashmap_loop(&hd, jhashmap_print_cb);

    jhashmap_loop(&hd, jhashmap_free_cb);
    printf("Free all nodes, current number is %d:\n", s_pheap.sel);

    jhashmap_test_indexs(&hd);

    jhashmap_uninit(&hd);
    jpheap_uninit(&s_pheap);

    return 0;
}
