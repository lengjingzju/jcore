/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdio.h>
#include <string.h>
#include "jstring.h"

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
    struct jstring str;
    // 测试正常初始化
    TEST(0 == jstring_init(&str, 4, 2));
    TEST(str.asize == 4);
    TEST(str.capacity == 3);  // +1 for '\0'
    TEST(str.size == 0);
    TEST(str.data[0] == '\0');

    // 测试反初始化
    jstring_uninit(&str);
    TEST(str.data == NULL);
    return 0;
}

int test_push_pop(void)
{
    struct jstring str;
    jstring_init(&str, 2, 2);

    // 测试基本push/pop
    TEST(0 == jstring_push(&str, 'A'));
    TEST(str.size == 1);
    TEST(str.data[0] == 'A');
    TEST(str.data[1] == '\0');

    char ret;
    TEST(0 == jstring_pop(&str, &ret));
    TEST(ret == 'A');
    TEST(str.size == 0);
    TEST(str.data[0] == '\0');

    // 测试自动扩容
    const char *test_str = "Hello";
    TEST(0 == jstring_pushs(&str, test_str, 5));
    TEST(str.size == 5);
    TEST(strcmp(str.data, "Hello") == 0);

    jstring_uninit(&str);
    return 0;
}

int test_insert_erase(void)
{
    struct jstring str;
    jstring_init(&str, 4, 8);
    jstring_pushs(&str, "ABCDE", 5);

    // 中间插入
    TEST(0 == jstring_insert(&str, 'X', 2));
    TEST(str.size == 6);
    TEST(strcmp(str.data, "ABXCDE") == 0);

    // 删除元素
    TEST(0 == jstring_erase(&str, 3));
    TEST(str.size == 5);
    TEST(strcmp(str.data, "ABXDE") == 0);

    // 边界测试
    TEST(-1 == jstring_insert(&str, 'Y', 6));  // 越界插入
    TEST(0 == jstring_insert(&str, 'Y', 5));    // 末尾插入
    TEST(strcmp(str.data, "ABXDEY") == 0);

    jstring_uninit(&str);
    return 0;
}

int test_resize(void)
{
    struct jstring str;
    jstring_init(&str, 4, 8);
    jstring_pushs(&str, "Hello", 5);

    // 缩容测试
    TEST(0 == jstring_resize(&str, 3));
    TEST(str.size == 3);
    TEST(strcmp(str.data, "Hel") == 0);

    // 扩容测试
    TEST(0 == jstring_resize(&str, 10));
    TEST(str.capacity == 11);  // +1 for '\0'
    TEST(strcmp(str.data, "Hel") == 0);

    jstring_uninit(&str);
    return 0;
}

int test_batch_operations(void)
{
    struct jstring str;
    jstring_init(&str, 8, 0);

    // 批量压入/弹出
    const char *data = "Test batch ops";
    size_t len = strlen(data);

    TEST(0 == jstring_pushs(&str, data, len));
    TEST(str.size == len);
    TEST(strcmp(str.data, data) == 0);

    char buffer[16];
    size_t pop_num = 3;
    TEST(0 == jstring_pops(&str, buffer, &pop_num));
    TEST(pop_num == 3);
    TEST(memcmp(buffer, "ops", 3) == 0);
    TEST(strcmp(str.data, "Test batch ") == 0);

    jstring_uninit(&str);
    return 0;
}

int main(void)
{
    test_init_uninit();
    test_push_pop();
    test_insert_erase();
    test_resize();
    test_batch_operations();

    printf("All tests completed\n");
    return 0;
}
