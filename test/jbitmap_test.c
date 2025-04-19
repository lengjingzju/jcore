/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include "jbitmap.h"

#define TEST(expr) do { \
    if (!(expr)) { \
        printf("[FAIL] %s:%d %s\n", __FILE__, __LINE__, #expr); \
    } else { \
        printf("[PASS] %s\n", #expr); \
    } \
} while(0)

static void test_set_clear_bit(int nbits)
{
    jbitmap_t *bm = jbitmap_zalloc(nbits);

    // 测试单个位操作
    jbitmap_set_bit(bm, 0);
    TEST(jbitmap_test_bit(bm, 0));

    jbitmap_set_bit(bm, 63);
    TEST(jbitmap_test_bit(bm, 63));

    jbitmap_clear_bit(bm, 63);
    TEST(!jbitmap_test_bit(bm, 63));

    // 边界测试
    jbitmap_set_bit(bm, nbits - 1);
    TEST(jbitmap_test_bit(bm, nbits - 1));

    // test_and_系列
    TEST(!jbitmap_test_and_set_bit(bm, 32));
    TEST(jbitmap_test_and_set_bit(bm, 32));

    TEST(jbitmap_test_and_clear_bit(bm, 32));
    TEST(!jbitmap_test_bit(bm, 32));

    jbitmap_free(bm);
}

static void test_bulk_ops(int nbits)
{
    int i = 0;
    jbitmap_t *bm1 = jbitmap_zalloc(nbits);
    jbitmap_t *bm2 = jbitmap_zalloc(nbits);

    // fill/zero测试
    jbitmap_fill(bm1, nbits);
    TEST(jbitmap_full(bm1, nbits));

    jbitmap_zero(bm1, nbits);
    TEST(jbitmap_empty(bm1, nbits));

    // 部分置位
    jbitmap_set(bm1, 60, 10);
    TEST(!jbitmap_test_bit(bm1, 59));
    for (i = 60; i < 70; ++i) {
        TEST(jbitmap_test_bit(bm1, i));
    }
    TEST(!jbitmap_test_bit(bm1, 70));

    // 跨块清除
    jbitmap_clear(bm1, 61, 8);
    TEST(jbitmap_test_bit(bm1, 60));
    for (i = 61; i < 69; ++i) {
        TEST(!jbitmap_test_bit(bm1, i));
    }
    TEST(jbitmap_test_bit(bm1, 69));

    // 复制测试
    jbitmap_copy(bm2, bm1, nbits);
    TEST(jbitmap_equal(bm1, bm2, nbits));

    jbitmap_free(bm1);
    jbitmap_free(bm2);
}

static void test_bitwise_ops(int nbits)
{
    jbitmap_t *bm1 = jbitmap_zalloc(nbits);
    jbitmap_t *bm2 = jbitmap_zalloc(nbits);
    jbitmap_t *result = jbitmap_zalloc(nbits);

    jbitmap_set(bm1, 0, 64);
    TEST(jbitmap_find_first_zero_bit(bm1, nbits) == 64);
    jbitmap_set(bm2, 32, 64);
    TEST(jbitmap_find_first_bit(bm2, nbits) == 32);
    TEST(jbitmap_find_next_zero_bit(bm2, nbits, 32) == 96);

    // AND测试
    jbitmap_and(result, bm1, bm2, nbits);
    TEST(jbitmap_find_first_bit(result, nbits) == 32);
    TEST(jbitmap_find_next_zero_bit(result, nbits, 32) == 64);

    // OR测试
    jbitmap_or(result, bm1, bm2, nbits);
    TEST(jbitmap_find_first_zero_bit(result, nbits) == 96);

    // XOR测试
    jbitmap_xor(result, bm1, bm2, nbits);
    TEST(jbitmap_find_first_bit(result, nbits) == 0);
    TEST(jbitmap_find_next_zero_bit(result, nbits, 0) == 32);

    jbitmap_free(bm1);
    jbitmap_free(bm2);
    jbitmap_free(result);
}

static void test_find_functions(int nbits)
{
    jbitmap_t *bm = jbitmap_zalloc(nbits);

    // 查找测试
    TEST(jbitmap_find_first_bit(bm, nbits) == -1);

    jbitmap_set_bit(bm, 100);
    TEST(jbitmap_find_first_bit(bm, nbits) == 100);
    TEST(jbitmap_find_next_bit(bm, nbits, 101) == -1);

    // 零位查找
    jbitmap_fill(bm, nbits);
    jbitmap_clear_bit(bm, nbits - 1);
    TEST(jbitmap_find_first_zero_bit(bm, nbits) == nbits - 1);

    // 连续位测试
    jbitmap_zero(bm, nbits);
    jbitmap_set(bm, 200, 5); // 200-204置位，暂时未关心是否合法
    if (nbits >= 205)
        TEST(jbitmap_find_bits(bm, nbits, 5) == 200);
    else if (nbits > 200)
        TEST(jbitmap_find_bits(bm, nbits, nbits - 200) == 200);
    TEST(jbitmap_find_bits(bm, nbits, 6) == -1);

    // 跨块连续零位
    jbitmap_fill(bm, nbits);
    jbitmap_clear(bm, 60, 10); // 60-69清零
    TEST(jbitmap_find_zero_bits(bm, nbits, 10) == 60);

    jbitmap_free(bm);
}

static void test_edge_cases(int nbits)
{
    jbitmap_t *bm = jbitmap_zalloc(nbits);

    // 单bit位图测试
    jbitmap_set_bit(bm, 0);
    jbitmap_set_bit(bm, nbits - 1);
    TEST(jbitmap_test_bit(bm, 0));
    TEST(jbitmap_test_bit(bm, nbits - 1));

    // 全置位时的find_zero
    jbitmap_fill(bm, nbits);
    TEST(jbitmap_find_first_zero_bit(bm, nbits) == -1);

    // 最后一个bit特殊处理
    jbitmap_clear_bit(bm, nbits - 1);
    TEST(jbitmap_find_first_zero_bit(bm, nbits) == nbits - 1);

    jbitmap_free(bm);
}

int main(void)
{
    int test_bits = 0;

    for (test_bits = 200; test_bits < 400; ++test_bits) {
        printf("----test_bits = %d----\n", test_bits);
        test_set_clear_bit(test_bits);
        test_bulk_ops(test_bits);
        test_bitwise_ops(test_bits);
        test_find_functions(test_bits);
        test_edge_cases(test_bits);
    }

    printf("All tests completed\n");
    return 0;
}
