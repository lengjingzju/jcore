/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2026-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "jringdata.h"
#include "jthread.h"

/*----------------------------------------------------------------------------
  测试配置
----------------------------------------------------------------------------*/

#define TEST_IDX_NUM        64      // 索引个数（向上对齐到2的幂）
#define TEST_IDX_SIZE       sizeof(uint32_t)   // 索引结构：存储数据长度
#define TEST_DATA_CAP       256     // 数据缓冲区容量（字节）
#define TEST_MAX_PRODUCERS  4
#define TEST_MAX_CONSUMERS  4
#define TEST_HOLD_NUM       8       // 保留历史索引数

/*----------------------------------------------------------------------------
  辅助宏与函数
----------------------------------------------------------------------------*/

/** @brief 测试断言，失败时打印行号并退出 */
#define test_assert(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "TEST FAILED at line %d: %s\n", __LINE__, msg); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

/** @brief 获取单调时间（毫秒） */
static inline uint64_t now_ms(void) {
    return jtime_monomsec_get();
}

/** @brief 默认 get_size 回调：将索引视为 uint32_t，返回其值 */
static uint32_t get_size_def(const void *idx) {
    return *(const uint32_t *)idx;
}

/** @brief 自定义 get_size：返回索引值+1（用于测试） */
static uint32_t get_size_custom(const void *idx) {
    return *(const uint32_t *)idx + 1;
}

/*----------------------------------------------------------------------------
  测试 1：单生产者单消费者基本读写（连续模式）
----------------------------------------------------------------------------*/
static void test_siso_basic(void)
{
    printf("Test 1: Single Producer Single Consumer basic read/write (continuous)\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);
    test_assert(rd != NULL, "init failed");

    /* 准备索引和数据：写入 3 个索引，长度分别为 5, 10, 15 */
    uint32_t idx[3] = {5, 10, 15};
    uint8_t data[30];
    for (int i = 0; i < 30; i++) data[i] = (uint8_t)i; // 0~29

    int ret = jringdata_write(rd, 0, idx, 3, data, 0, 0, NULL);
    test_assert(ret == 3, "write failed");
    uint32_t total_idx, total_data;
    total_idx = jringdata_size(rd, -1, &total_data);
    test_assert(total_idx == 3, "idx count mismatch");
    test_assert(total_data == 30, "data size mismatch");

    /* 读取 2 个索引（只读 idx[0], idx[1]） */
    uint32_t ridx[3] = {0};
    uint8_t rdata[30] = {0};
    ret = jringdata_read(rd, 0, ridx, 2, rdata, 15, NULL, NULL, 0, 0);
    test_assert(ret == 2, "read failed");
    test_assert(ridx[0] == 5 && ridx[1] == 10, "idx mismatch");
    test_assert(memcmp(rdata, data, 15) == 0, "data mismatch");

    /* 读取剩余 1 个索引 */
    ret = jringdata_read(rd, 0, ridx + 2, 1, rdata + 15, 15, NULL, NULL, 0, 0);
    test_assert(ret == 1, "read rest failed");
    test_assert(ridx[2] == 15, "idx mismatch");
    test_assert(memcmp(rdata + 15, data + 15, 15) == 0, "data mismatch");

    /* 缓冲区应空 */
    total_idx = jringdata_size(rd, -1, &total_data);
    test_assert(total_idx == 0 && total_data == 0, "should be empty");

    jringdata_uninit(rd);
    printf("Test 1 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 2：完全读写策略 COMPLETE
----------------------------------------------------------------------------*/
static void test_complete_strategy(void)
{
    printf("Test 2: COMPLETE read/write strategy\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    /* 写入 5 个索引，总数据 100 字节 */
    uint32_t idx[5] = {20,20,20,20,20};
    uint8_t data[100];
    memset(data, 0xAB, 100);
    int ret = jringdata_write(rd, 0, idx, 5, data, 0, 0, NULL);
    test_assert(ret == 5, "write failed");

    /* 完全读 6 个索引，数据不足，应失败 */
    uint32_t ridx[6];
    uint8_t rdata[120];
    ret = jringdata_read(rd, 0, ridx, 6, rdata, 120, NULL, NULL,
                         JRINGDATA_COMPLETE, 0);
    test_assert(ret == -1, "complete read should fail");
    test_assert(jringdata_size(rd, -1, NULL) == 5, "data should remain");

    /* 完全读 5 个索引，成功 */
    ret = jringdata_read(rd, 0, ridx, 5, rdata, 100, NULL, NULL,
                         JRINGDATA_COMPLETE, 0);
    test_assert(ret == 5, "complete read success");
    test_assert(jringdata_size(rd, -1, NULL) == 0, "should be empty");

    /* 完全写：缓冲区已满时，完全写应失败（索引和数据容量均满） */
    /* 先写满索引（TEST_IDX_NUM 个）和数据（TEST_DATA_CAP）*/
    uint32_t fill_idx[TEST_IDX_NUM];
    uint8_t fill_data[TEST_DATA_CAP];
    for (int i = 0; i < TEST_IDX_NUM; i++) {
        fill_idx[i] = 1;  // 每个索引对应1字节数据
    }
    memset(fill_data, 0xCD, TEST_DATA_CAP);
    ret = jringdata_write(rd, 0, fill_idx, TEST_IDX_NUM, fill_data, 0, 0, NULL);
    test_assert(ret == TEST_IDX_NUM, "fill write failed");
    test_assert(jringdata_size(rd, -1, NULL) == TEST_IDX_NUM, "idx full");

    /* 尝试完全写 1 个索引（需要数据1字节），应失败 */
    uint32_t one_idx = 1;
    uint8_t one_data = 0xFF;
    ret = jringdata_write(rd, 0, &one_idx, 1, &one_data, JRINGDATA_COMPLETE, 0, NULL);
    test_assert(ret == -1, "complete write should fail when full");

    jringdata_uninit(rd);
    printf("Test 2 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 3：BLOCK 模式读写
----------------------------------------------------------------------------*/
typedef struct {
    jringdata_t *rd;
    int id;
    int use_ridx;
} thread_arg_t;

static jthread_ret_t producer_block(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    /* 延迟 20ms 再写入，让读者先进入等待 */
    jthread_msleep(20);
    uint32_t idx[2] = {4, 4};      // 两个索引，各4字节
    uint8_t data[8] = {1,2,3,4,5,6,7,8};
    int ret = jringdata_write(targ->rd, targ->id, idx, 2, data, 0, 0, NULL);
    test_assert(ret == 2, "producer write failed");
    /* 再延迟 30ms，写入更多数据唤醒消费者（wake_num 为 3 个索引） */
    jthread_msleep(30);
    uint32_t idx2[1] = {4};
    uint8_t data2[4] = {9,10,11,12};
    ret = jringdata_write(targ->rd, targ->id, idx2, 1, data2, 0, 0, NULL);
    test_assert(ret == 1, "producer write failed");
    return NULL;
}

static jthread_ret_t consumer_block(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint32_t ridx[3];
    uint8_t rdata[12];
    uint64_t start = now_ms();
    int ret = jringdata_read(targ->rd, targ->id, ridx, 3, rdata, 12,
                             NULL, NULL, JRINGDATA_BLOCK, -1);
    uint64_t elapsed = now_ms() - start;
    test_assert(ret == 3, "consumer read failed");
    test_assert(elapsed >= 50, "should have waited about 50ms");
    /* 验证数据 */
    test_assert(ridx[0] == 4 && ridx[1] == 4 && ridx[2] == 4, "idx mismatch");
    uint8_t expected[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    test_assert(memcmp(rdata, expected, 12) == 0, "data mismatch");
    return NULL;
}

static void test_block_strategy(void)
{
    printf("Test 3: BLOCK strategy\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 2,
        .max_consumers = 2,
        .hold_num = 0,
        .wake_num = 3,    // 至少3个索引才唤醒
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int pid1 = jringdata_add_producer(rd);
    int cid1 = jringdata_add_consumer(rd, 0);
    test_assert(pid1 >= 0 && cid1 >= 0, "add producer/consumer failed");

    jthread_t prod, cons;
    thread_arg_t parg = {rd, pid1, 1};
    thread_arg_t carg = {rd, cid1, 1};

    jthread_create(&cons, NULL, consumer_block, &carg);
    jthread_create(&prod, NULL, producer_block, &parg);
    jthread_join(cons);
    jthread_join(prod);

    jringdata_uninit(rd);
    printf("Test 3 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 4：RETRY 策略
----------------------------------------------------------------------------*/
static void test_retry_strategy(void)
{
    printf("Test 4: RETRY strategy\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    uint32_t ridx;
    uint8_t rdata[1];
    uint64_t start = now_ms();
    int ret = jringdata_read(rd, 0, &ridx, 1, rdata, 1, NULL, NULL,
                             JRINGDATA_RETRY, 1000000);
    uint64_t elapsed = now_ms() - start;
    test_assert(ret == -1, "retry read should fail");
    test_assert(elapsed >= 2, "should have waited some time");

    jringdata_uninit(rd);
    printf("Test 4 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 5：DROP 策略（写入时丢弃旧索引/数据）
----------------------------------------------------------------------------*/
static void test_drop_strategy(void)
{
    printf("Test 5: DROP strategy (discard old data)\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    /* 填满缓冲区：每个索引占1字节数据，写满索引个数 */
    uint32_t fill_idx[TEST_IDX_NUM];
    uint8_t fill_data[TEST_IDX_NUM];
    for (int i = 0; i < TEST_IDX_NUM; i++) {
        fill_idx[i] = 1;
        fill_data[i] = (uint8_t)(0x11 + i);
    }
    int ret = jringdata_write(rd, 0, fill_idx, TEST_IDX_NUM, fill_data, 0, 0, NULL);
    test_assert(ret == TEST_IDX_NUM, "fill write failed");

    /* 再写入 10 个索引，每个1字节，使用 DROP 丢弃旧数据 */
    uint32_t new_idx[10];
    uint8_t new_data[10];
    for (int i = 0; i < 10; i++) {
        new_idx[i] = 1;
        new_data[i] = (uint8_t)(0x22 + i);
    }
    ret = jringdata_write(rd, 0, new_idx, 10, new_data, JRINGDATA_DROP, 0, NULL);
    test_assert(ret == 10, "drop write failed");
    /* 索引数仍为 TEST_IDX_NUM（因为丢10个旧索引，写入10个新索引） */
    test_assert(jringdata_size(rd, -1, NULL) == TEST_IDX_NUM, "idx count unchanged");

    /* 读取全部，验证旧数据被丢弃，新数据在末尾 */
    uint32_t ridx[TEST_IDX_NUM];
    uint8_t rdata[TEST_IDX_NUM];
    ret = jringdata_read(rd, 0, ridx, TEST_IDX_NUM, rdata, TEST_IDX_NUM,
                         NULL, NULL, 0, 0);
    test_assert(ret == TEST_IDX_NUM, "read all");
    /* 前 TEST_IDX_NUM - 10 个应为旧数据的后部分（从 fill_data[10] 开始） */
    test_assert(memcmp(rdata, fill_data + 10, TEST_IDX_NUM - 10) == 0, "old data mismatch");
    /* 最后 10 个应为新数据 */
    test_assert(memcmp(rdata + TEST_IDX_NUM - 10, new_data, 10) == 0, "new data mismatch");

    /* 再次全部写入 */
    ret = jringdata_write(rd, 0, fill_idx, TEST_IDX_NUM, fill_data, 0, 0, NULL);
    test_assert(ret == TEST_IDX_NUM, "fill write failed");

    /* 测试最少丢一半索引 */
    uint32_t dropped = TEST_IDX_NUM / 2;
    ret = jringdata_write(rd, 0, new_idx, 10, new_data, JRINGDATA_DROP, 0, &dropped);
    test_assert(ret == 10, "drop write failed");
    test_assert(dropped == TEST_IDX_NUM / 2, "drop number wrong");
    /* 索引数为 TEST_IDX_NUM / 2 + 10，写入10个新索引，最少丢一半索引 */
    test_assert(jringdata_size(rd, -1, NULL) == TEST_IDX_NUM / 2 + 10, "idx count wrong");

    jringdata_uninit(rd);
    printf("Test 5 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 6：多生产者写入（连续模式）
----------------------------------------------------------------------------*/
static jthread_ret_t multi_producer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint32_t idx[2] = {2, 3};          // 每个生产者写2个索引，数据长度2和3
    uint8_t data[5];
    memset(data, (uint8_t)targ->id, 5);
    int ret = jringdata_write(targ->rd, targ->id, idx, 2, data, 0, 0, NULL);
    test_assert(ret == 2, "multi producer write failed");
    return NULL;
}

static void test_multi_producer(void)
{
    printf("Test 6: Multi-producer write\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = TEST_MAX_PRODUCERS,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int pids[TEST_MAX_PRODUCERS] = {0};
    pids[0] = 0;
    for (int i = 1; i < TEST_MAX_PRODUCERS; i++) {
        pids[i] = jringdata_add_producer(rd);
        test_assert(pids[i] >= 0, "add producer failed");
    }

    jthread_t threads[TEST_MAX_PRODUCERS];
    thread_arg_t args[TEST_MAX_PRODUCERS];
    for (int i = 0; i < TEST_MAX_PRODUCERS; i++) {
        args[i].rd = rd;
        args[i].id = pids[i];
        jthread_create(&threads[i], NULL, multi_producer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_PRODUCERS; i++) {
        jthread_join(threads[i]);
    }

    /* 总共写入 4*2=8 个索引，总数据 (2+3)*4=20 字节 */
    uint32_t idx_sz, data_sz;
    idx_sz = jringdata_size(rd, -1, &data_sz);
    test_assert(idx_sz == 8, "total idx wrong");
    test_assert(data_sz == 20, "total data wrong");

    jringdata_uninit(rd);
    printf("Test 6 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 7：多消费者共享读模式
----------------------------------------------------------------------------*/
static jthread_ret_t shared_consumer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint32_t ridx[4];
    uint8_t rdata[16];
    int ret = jringdata_read(targ->rd, targ->id, ridx, 4, rdata, 16,
                             NULL, NULL, 0, 0);
    test_assert(ret == 4, "shared consumer read failed");
    return NULL;
}

static void test_multi_consumer_shared(void)
{
    printf("Test 7: Multi-consumer shared read\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = TEST_MAX_CONSUMERS,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int cids[TEST_MAX_CONSUMERS] = {0};
    cids[0] = 0;
    for (int i = 1; i < TEST_MAX_CONSUMERS; i++) {
        cids[i] = jringdata_add_consumer(rd, 0);
        test_assert(cids[i] >= 0, "add consumer failed");
    }

    /* 写入 20 个索引，每个索引数据4字节（共80字节） */
    uint32_t idx[20];
    uint8_t data[80];
    for (int i = 0; i < 20; i++) {
        idx[i] = 4;
        data[i*4] = (uint8_t)i;
        data[i*4+1] = (uint8_t)(i+1);
        data[i*4+2] = (uint8_t)(i+2);
        data[i*4+3] = (uint8_t)(i+3);
    }
    int ret = jringdata_write(rd, 0, idx, 20, data, 0, 0, NULL);
    test_assert(ret == 20, "write failed");

    /* 4 个消费者每人读 4 个索引（16字节），共享读指针，总读出16个索引 */
    jthread_t threads[TEST_MAX_CONSUMERS];
    thread_arg_t args[TEST_MAX_CONSUMERS];
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        args[i].rd = rd;
        args[i].id = cids[i];
        jthread_create(&threads[i], NULL, shared_consumer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        jthread_join(threads[i]);
    }

    /* 剩余 4 个索引（16字节） */
    uint32_t remain_idx, remain_data;
    remain_idx = jringdata_size(rd, -1, &remain_data);
    test_assert(remain_idx == 4, "should have 4 idx left");
    test_assert(remain_data == 16, "should have 16 bytes left");

    jringdata_uninit(rd);
    printf("Test 7 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 8：多消费者独占读模式
----------------------------------------------------------------------------*/
static jthread_ret_t exclusive_consumer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint32_t ridx[4];
    uint8_t rdata[16];
    int ret = jringdata_read(targ->rd, targ->id, ridx, 4, rdata, 16,
                             NULL, NULL, 0, 0);
    test_assert(ret == 4, "exclusive read failed");
    /* 验证数据（每个消费者读到相同部分，因为独占读会保留数据直到全部消费） */
    for (int i = 0; i < 4; i++) {
        test_assert(ridx[i] == 4, "idx wrong");
    }
    return NULL;
}

static void test_multi_consumer_exclusive(void)
{
    printf("Test 8: Multi-consumer exclusive read\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = TEST_MAX_CONSUMERS,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_EXCLUSIVE,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int cids[TEST_MAX_CONSUMERS] = {0};
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        cids[i] = jringdata_add_consumer(rd, 1);   // 从最旧开始
        test_assert(cids[i] >= 0, "add consumer failed");
    }

    /* 写入 16 个索引，每个4字节，共64字节 */
    uint32_t idx[16];
    uint8_t data[64];
    for (int i = 0; i < 16; i++) {
        idx[i] = 4;
        memset(data + i*4, (uint8_t)i, 4);
    }
    int ret = jringdata_write(rd, 0, idx, 16, data, 0, 0, NULL);
    test_assert(ret == 16, "write failed");

    /* 4 个消费者每人读 4 个索引（16字节），独占读，空间在所有消费者读完后才释放 */
    jthread_t threads[TEST_MAX_CONSUMERS];
    thread_arg_t args[TEST_MAX_CONSUMERS];
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        args[i].rd = rd;
        args[i].id = cids[i];
        jthread_create(&threads[i], NULL, exclusive_consumer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        jthread_join(threads[i]);
    }

    /* 所有消费者都读完，应释放所有索引数据（为0） */
    uint32_t remain_idx, remain_data;
    remain_idx = jringdata_size(rd, -1, &remain_data);
    test_assert(remain_idx == 12, "should have 12 idx left (because each consumer read 4, total 16, min_read_index moved to 4? Actually exclusive: min_read_index = min of all consumers, all read 4, so min=4, total=16-4=12)");
    test_assert(remain_data == 48, "should have 48 bytes left");

    jringdata_uninit(rd);
    printf("Test 8 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 9：hold_num 历史保留
----------------------------------------------------------------------------*/
static void test_hold_num(void)
{
    printf("Test 9: hold_num history retention\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 2,
        .hold_num = TEST_HOLD_NUM,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_EXCLUSIVE,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int c1 = jringdata_add_consumer(rd, 1);
    test_assert(c1 >= 0, "add consumer 1 failed");

    /* 写入 50 个索引，每个1字节 */
    uint32_t idx[50];
    uint8_t data[50];
    for (int i = 0; i < 50; i++) {
        idx[i] = 1;
        data[i] = (uint8_t)i;
    }
    int ret = jringdata_write(rd, 0, idx, 50, data, 0, 0, NULL);
    test_assert(ret == 50, "write 50 failed");

    /* c1 读完所有 50 个索引（先读42，再读8） */
    uint32_t ridx[50];
    uint8_t rdata[50];
    ret = jringdata_read(rd, c1, ridx, 50, rdata, 50, NULL, NULL, 0, 0);
    test_assert(ret == 50, "read all failed");

    /* c1 再写入 1 个索引，才会触发更新 min_read_index */
    ret = jringdata_write(rd, 0, idx, 1, data, 0, 0, NULL);
    test_assert(ret == 1, "write 50 failed");

    /* 添加新消费者 c2，从最旧开始（此时 hold_num 应生效） */
    int c2 = jringdata_add_consumer(rd, 1);
    test_assert(c2 >= 0, "add consumer 2 failed");

    uint32_t c2_size, c2_data;
    c2_size = jringdata_size(rd, c2, &c2_data);
    test_assert(c2_size == TEST_HOLD_NUM + 1, "c2 should see hold_num idx");
    test_assert(c2_data == TEST_HOLD_NUM + 1, "c2 data size should match");

    jringdata_uninit(rd);
    printf("Test 9 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 10：动态删除生产者和消费者
----------------------------------------------------------------------------*/
static void test_dynamic_del(void)
{
    printf("Test 10: Dynamic add/del producers and consumers\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 3,
        .max_consumers = 3,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_EXCLUSIVE,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int p1 = jringdata_add_producer(rd);
    int p2 = jringdata_add_producer(rd);
    test_assert(p1 >= 0 && p2 >= 0, "add producers failed");

    int c1 = jringdata_add_consumer(rd, 1);
    int c2 = jringdata_add_consumer(rd, 1);
    test_assert(c1 >= 0 && c2 >= 0, "add consumers failed");

    test_assert(jringdata_del_producer(rd, p1) == 0, "del producer failed");
    test_assert(jringdata_del_consumer(rd, -1) == 0, "del all consumers failed");
    test_assert(jringdata_size(rd, -1, NULL) == 0, "should be empty");

    jringdata_uninit(rd);
    printf("Test 10 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 11：drop_data 接口
----------------------------------------------------------------------------*/
static void test_drop_data(void)
{
    printf("Test 11: drop_data interface\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 2,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_EXCLUSIVE,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    int c1 = jringdata_add_consumer(rd, 1);
    int c2 = jringdata_add_consumer(rd, 0);  // 从最新开始，无历史
    test_assert(c1 >= 0 && c2 >= 0, "add consumers failed");

    /* 写入 20 个索引，每个2字节，共40字节 */
    uint32_t idx[20];
    uint8_t data[40];
    for (int i = 0; i < 20; i++) {
        idx[i] = 2;
        data[i*2] = (uint8_t)(i*2);
        data[i*2+1] = (uint8_t)(i*2+1);
    }
    int ret = jringdata_write(rd, 0, idx, 20, data, 0, 0, NULL);
    test_assert(ret == 20, "write failed");

    /* c2 丢弃所有数据 */
    test_assert(jringdata_drop_data(rd, c2, 0) == 0, "drop_data all for c2");
    test_assert(jringdata_size(rd, c2, NULL) == 0, "c2 should have 0");

    /* c1 丢弃前 5 个索引（10字节） */
    test_assert(jringdata_drop_data(rd, c1, 5) == 0, "drop_data partial");
    uint32_t c1_sz, c1_data;
    c1_sz = jringdata_size(rd, c1, &c1_data);
    test_assert(c1_sz == 15, "c1 should have 15 idx left");
    test_assert(c1_data == 30, "c1 data size");

    jringdata_uninit(rd);
    printf("Test 11 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 12：销毁时的安全退出
----------------------------------------------------------------------------*/
static jthread_ret_t slow_reader(void *arg) {
    jringdata_t *rd = (jringdata_t*)arg;
    uint32_t ridx;
    uint8_t rdata[1];
    int ret = jringdata_read(rd, 0, &ridx, 1, rdata, 1, NULL, NULL,
                             JRINGDATA_BLOCK, -1);
    test_assert(ret == -1, "should fail due to uninit");
    return NULL;
}

static void test_uninit_safety(void)
{
    printf("Test 12: Safe uninit with waiting threads\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);
    jthread_t reader;
    jthread_create(&reader, NULL, slow_reader, rd);

    jthread_msleep(10);
    jringdata_uninit(rd);
    jthread_join(reader);

    printf("Test 12 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 13：writev / readv 分散操作
----------------------------------------------------------------------------*/
static void test_writev_readv(void)
{
    printf("Test 13: writev and readv (scatter/gather)\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_def
    };
    jringdata_t *rd = jringdata_init(&cfg);

    /* 准备 3 个独立的索引和数据块 */
    uint32_t idx1 = 5, idx2 = 10, idx3 = 15;
    uint8_t data1[5] = {0,1,2,3,4};
    uint8_t data2[10] = {5,6,7,8,9,10,11,12,13,14};
    uint8_t data3[15] = {15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
    const void *idx_arr[3] = {&idx1, &idx2, &idx3};
    const void *data_arr[3] = {data1, data2, data3};

    int ret = jringdata_writev(rd, 0, idx_arr, 3, data_arr, 0, 0, NULL);
    test_assert(ret == 3, "writev failed");

    /* 读取 3 个索引和对应的数据 */
    uint32_t ridx[3];
    uint8_t rdata[30];
    void *ridx_arr[3] = {&ridx[0], &ridx[1], &ridx[2]};
    void *rdata_arr[3] = {rdata, rdata+5, rdata+15};  // 注意各数据区大小
    uint32_t lens[3] = {5, 10, 15};
    ret = jringdata_readv(rd, 0, ridx_arr, 3, rdata_arr, lens,
                          NULL, NULL, 0, 0);
    test_assert(ret == 3, "readv failed");
    test_assert(ridx[0] == 5 && ridx[1] == 10 && ridx[2] == 15, "idx mismatch");
    test_assert(memcmp(rdata, data1, 5) == 0, "data1 mismatch");
    test_assert(memcmp(rdata+5, data2, 10) == 0, "data2 mismatch");
    test_assert(memcmp(rdata+15, data3, 15) == 0, "data3 mismatch");

    jringdata_uninit(rd);
    printf("Test 13 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 14：自定义 get_size 回调
----------------------------------------------------------------------------*/
static void test_custom_get_size(void)
{
    printf("Test 14: Custom get_size callback\n");

    jringdata_cfg_t cfg = {
        .idx_num = TEST_IDX_NUM,
        .idx_size = TEST_IDX_SIZE,
        .capacity = TEST_DATA_CAP,
        .max_producers = 1,
        .max_consumers = 1,
        .hold_num = 0,
        .wake_num = 0,
        .read_mode = JRINGDATA_READ_SHARED,
        .get_size = get_size_custom   // 返回 idx+1
    };
    jringdata_t *rd = jringdata_init(&cfg);

    /* 写入 3 个索引，值为 5,10,15，实际数据长度应为 6,11,16 */
    uint32_t idx[3] = {5, 10, 15};
    uint8_t data[33];   // 6+11+16=33
    for (int i = 0; i < 33; i++) data[i] = (uint8_t)(i+100);

    int ret = jringdata_write(rd, 0, idx, 3, data, 0, 0, NULL);
    test_assert(ret == 3, "write failed");

    uint32_t total_data;
    jringdata_size(rd, -1, &total_data);
    test_assert(total_data == 33, "total data size wrong (custom)");

    /* 读取验证 */
    uint32_t ridx[3];
    uint8_t rdata[33];
    ret = jringdata_read(rd, 0, ridx, 3, rdata, 33, NULL, NULL, 0, 0);
    test_assert(ret == 3, "read failed");
    test_assert(memcmp(rdata, data, 33) == 0, "data mismatch");

    jringdata_uninit(rd);
    printf("Test 14 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  主函数
----------------------------------------------------------------------------*/
int main(void)
{
    printf("=== jringdata Test Suite ===\n\n");

    test_siso_basic();
    test_complete_strategy();
    test_block_strategy();
    test_retry_strategy();
    test_drop_strategy();
    test_multi_producer();
    test_multi_consumer_shared();
    test_multi_consumer_exclusive();
    test_hold_num();
    test_dynamic_del();
    test_drop_data();
    test_uninit_safety();
    test_writev_readv();
    test_custom_get_size();

    printf("All tests PASSED.\n");
    return 0;
}
