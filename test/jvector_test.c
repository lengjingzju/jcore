/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdio.h>
#include <string.h>
#include "jvector.h"

#define TEST(expr) do { \
    if (!(expr)) { \
        printf("[FAIL] %s:%d %s\n", __FILE__, __LINE__, #expr); \
        return -1; \
    } else { \
        printf("[PASS] %s\n", #expr); \
    } \
} while(0)

int test_init_uninit(void)
{
    struct jvector vec;
    // 测试正常初始化
    TEST(0 == jvector_init(&vec, sizeof(int), 4, 2));
    TEST(vec.esize == sizeof(int));
    TEST(vec.asize == 4);
    TEST(vec.capacity == 2);

    // 测试反初始化
    jvector_uninit(&vec);
    TEST(vec.data == NULL);
    return 0;
}

int test_push_pop(void)
{
    struct jvector vec;
    jvector_init(&vec, sizeof(int), 2, 2);

    int values[] = {10, 20, 30};

    // 测试基本push/pop
    TEST(0 == jvector_push(&vec, &values[0]));
    TEST(vec.size == 1);

    int ret;
    TEST(0 == jvector_pop(&vec, &ret));
    TEST(ret == 10);
    TEST(vec.size == 0);

    // 测试自动扩容
    jvector_pushs(&vec, values, 3);
    TEST(vec.size == 3);
    TEST(vec.capacity >= 3);

    // 清理
    jvector_uninit(&vec);
    return 0;
}

int test_insert_erase(void)
{
    struct jvector vec;
    jvector_init(&vec, sizeof(int), 2, 4);

    int values[] = {1,2,3,4};
    jvector_pushs(&vec, values, 4);

    // 中间插入
    int new_val = 99;
    TEST(0 == jvector_insert(&vec, &new_val, 2));
    TEST(*(int*)jvector_at(&vec, 2) == 99);
    TEST(vec.size == 5);

    // 删除元素
    TEST(0 == jvector_erase(&vec, 2));
    TEST(vec.size == 4);

    // 清理
    jvector_uninit(&vec);
    return 0;
}

int test_resize(void)
{
    struct jvector vec;
    jvector_init(&vec, sizeof(int), 4, 4);

    // 扩容测试
    TEST(0 == jvector_resize(&vec, 8));
    TEST(vec.capacity == 8);

    // 缩容测试
    TEST(0 == jvector_resize(&vec, 2));
    TEST(vec.capacity == 2);

    jvector_uninit(&vec);
    return 0;
}

int main(void)
{
    test_init_uninit();
    test_push_pop();
    test_insert_erase();
    test_resize();

    printf("All tests completed\n");
    return 0;
}
