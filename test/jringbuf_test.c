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
#include "jringbuf.h"
#include "jthread.h"

/*----------------------------------------------------------------------------
  测试配置
----------------------------------------------------------------------------*/

#define TEST_CAPACITY       64
#define TEST_MAX_PRODUCERS  4
#define TEST_MAX_CONSUMERS  4
#define TEST_HOLD_SIZE      16

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

/*----------------------------------------------------------------------------
  测试 1：单生产者单消费者基本读写
----------------------------------------------------------------------------*/
static void test_siso_basic(void)
{
    printf("Test 1: Single Producer Single Consumer basic read/write\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 1, 0, 0, JRINGBUF_READ_SHARED);
    test_assert(rb != NULL, "init failed");

    /* 写入 10 字节 */
    uint8_t wdata[10] = {0,1,2,3,4,5,6,7,8,9};
    int ret = jringbuf_write(rb, 0, wdata, 10, 0, 0, 0);
    test_assert(ret == 10, "write failed");
    test_assert(jringbuf_size(rb, -1) == 10, "size mismatch");

    /* 读取 5 字节 */
    uint8_t rbuf[10] = {0};
    ret = jringbuf_read(rb, 0, rbuf, 5, 0, 0);
    test_assert(ret == 5, "read failed");
    test_assert(memcmp(rbuf, wdata, 5) == 0, "data mismatch");
    test_assert(jringbuf_size(rb, -1) == 5, "size after partial read");

    /* 读取剩余 5 字节 */
    ret = jringbuf_read(rb, 0, rbuf + 5, 5, 0, 0);
    test_assert(ret == 5, "read rest failed");
    test_assert(memcmp(rbuf + 5, wdata + 5, 5) == 0, "rest data mismatch");
    test_assert(jringbuf_size(rb, -1) == 0, "should be empty");

    jringbuf_uninit(rb);
    printf("Test 1 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 2：完全读写策略 COMPLETE
----------------------------------------------------------------------------*/
static void test_complete_strategy(void)
{
    printf("Test 2: COMPLETE read/write strategy\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 1, 0, 0, JRINGBUF_READ_SHARED);

    /* 先写入 30 字节，使缓冲区有足够数据 */
    uint8_t wdata[30];
    memset(wdata, 0xAB, sizeof(wdata));
    int ret = jringbuf_write(rb, 0, wdata, 30, 0, 0, 0);
    test_assert(ret == 30, "write failed");

    /* 完全读 40 字节，数据不足，应返回 -1 */
    uint8_t rbuf[40];
    ret = jringbuf_read(rb, 0, rbuf, 40, JRINGBUF_COMPLETE, 0);
    test_assert(ret == -1, "complete read should fail");
    test_assert(jringbuf_size(rb, -1) == 30, "data should remain");

    /* 完全读 30 字节，应该成功 */
    ret = jringbuf_read(rb, 0, rbuf, 30, JRINGBUF_COMPLETE, 0);
    test_assert(ret == 30, "complete read should succeed");
    test_assert(jringbuf_size(rb, -1) == 0, "should be empty");

    /* 测试完全写：缓冲区满时完全写失败 */
    uint8_t fill[TEST_CAPACITY];
    memset(fill, 0xCD, sizeof(fill));
    ret = jringbuf_write(rb, 0, fill, TEST_CAPACITY, 0, 0, 0);
    test_assert(ret == TEST_CAPACITY, "fill write failed");
    test_assert(jringbuf_size(rb, -1) == TEST_CAPACITY, "buffer should be full");

    /* 完全写 1 字节，应失败 */
    uint8_t one = 0xFF;
    ret = jringbuf_write(rb, 0, &one, 1, JRINGBUF_COMPLETE, 0, 0);
    test_assert(ret == -1, "complete write should fail when full");

    jringbuf_uninit(rb);
    printf("Test 2 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 3：BLOCK 模式读写
----------------------------------------------------------------------------*/
typedef struct {
    jringbuf_t *rb;
    int id;
    int use_ridx;
} thread_arg_t;

static jthread_ret_t producer_block(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    /* 延迟 20ms 再写入，让读者先进去等待 */
    jthread_msleep(20);
    uint8_t data[4] = {1,2,3,4};
    int ret = jringbuf_write(targ->rb, targ->id, data, 4, 0, 0, 0);
    test_assert(ret == 4, "producer write failed");
    /* 延迟 30ms 再写入，buf中数据达到指定值才唤醒消费者 */
    jthread_msleep(30);
    ret = jringbuf_write(targ->rb, targ->id, data, 4, 0, 0, 0);
    test_assert(ret == 4, "producer write failed");
    return NULL;
}

static jthread_ret_t consumer_block(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint8_t rbuf[4];
    uint64_t start = now_ms();
    int ret = jringbuf_read(targ->rb, targ->id, rbuf, 4,
                            JRINGBUF_BLOCK, -1); /* 无限等待 */
    uint64_t elapsed = now_ms() - start;
    test_assert(ret == 4, "consumer read failed");
    test_assert(elapsed >= 50, "should have waited at about 50ms");
    return NULL;
}

static void test_block_strategy(void)
{
    printf("Test 3: BLOCK strategy\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 2, 2, 0, 8, // 生产者最少8B数据才唤醒消费者
                                   JRINGBUF_READ_SHARED);

    /* 添加一个消费者 */
    int pid1 = jringbuf_add_producer(rb);
    int cid1 = jringbuf_add_consumer(rb, 0);
    test_assert(cid1 >= 0, "add consumer failed");

    jthread_t prod, cons;
    thread_arg_t parg = {rb, pid1, 1};
    thread_arg_t carg = {rb, cid1, 1};

    /* 消费者先启动，进入等待 */
    jthread_create(&cons, NULL, consumer_block, &carg);
    /* 生产者在 10ms 后写入 */
    jthread_create(&prod, NULL, producer_block, &parg);

    jthread_join(cons);
    jthread_join(prod);

    jringbuf_uninit(rb);
    printf("Test 3 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 4：RETRY 策略
----------------------------------------------------------------------------*/
static void test_retry_strategy(void)
{
    printf("Test 4: RETRY strategy\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 1, 0, 0, JRINGBUF_READ_SHARED);

    uint8_t rbuf[4];
    uint64_t start = now_ms();
    /* 尝试重试 1000000 次，缓冲区空，应失败 */
    int ret = jringbuf_read(rb, 0, rbuf, 4,
                            JRINGBUF_RETRY, 1000000);
    uint64_t elapsed = now_ms() - start;
    test_assert(ret == -1, "retry read should fail");
    test_assert(elapsed >= 2, "should have waited some time");

    jringbuf_uninit(rb);
    printf("Test 4 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 5：DROP 策略（写入时丢弃旧数据）
----------------------------------------------------------------------------*/
static void test_drop_strategy(void)
{
    printf("Test 5: DROP strategy (discard old data)\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 1, 0, 0, JRINGBUF_READ_SHARED);

    /* 写满缓冲区 */
    uint8_t fill[TEST_CAPACITY];
    memset(fill, 0x11, TEST_CAPACITY);
    int ret = jringbuf_write(rb, 0, fill, TEST_CAPACITY, 0, 0, 0);
    test_assert(ret == TEST_CAPACITY, "fill failed");
    test_assert(jringbuf_size(rb, -1) == TEST_CAPACITY, "size wrong");

    /* 再写入 10 字节，使用 DROP 策略丢弃旧数据 */
    uint8_t newdata[10];
    memset(newdata, 0x22, 10);
    ret = jringbuf_write(rb, 0, newdata, 10, JRINGBUF_DROP, 0, 0); /* dropped=0 自动计算 */
    test_assert(ret == 10, "drop write failed");
    /* 由于丢弃了 10 字节旧数据，数据长度仍为 capacity */
    test_assert(jringbuf_size(rb, -1) == TEST_CAPACITY, "size after drop");

    /* 验证最前面的 10 字节被丢弃，现在最旧的是 0x11 从 offset 10 开始 */
    uint8_t rbuf[TEST_CAPACITY];
    ret = jringbuf_read(rb, 0, rbuf, TEST_CAPACITY, 0, 0);
    test_assert(ret == TEST_CAPACITY, "read all");
    /* 前 TEST_CAPACITY - 10 字节应为 0x11 */
    test_assert(memcmp(rbuf, fill + 10, TEST_CAPACITY - 10) == 0, "old data mismatch");
    /* 最后 10 字节应为 0x22 */
    test_assert(memcmp(rbuf + TEST_CAPACITY - 10, newdata, 10) == 0, "new data mismatch");

    jringbuf_uninit(rb);
    printf("Test 5 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 6：多生产者写入
----------------------------------------------------------------------------*/
static jthread_ret_t multi_producer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint8_t data[4];
    memset(data, (uint8_t)targ->id, 4);
    int ret = jringbuf_write(targ->rb, targ->id, data, 4, 0, 0, 0);
    test_assert(ret == 4, "multi producer write failed");
    return NULL;
}

static void test_multi_producer(void)
{
    printf("Test 6: Multi-producer write\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, TEST_MAX_PRODUCERS, 1, 0, 0, JRINGBUF_READ_SHARED);

    /* 添加 4 个生产者（包括默认的 0 号，还需添加 3 个） */
    int pids[TEST_MAX_PRODUCERS] = {0};
    pids[0] = 0; /* 默认存在 */
    for (int i = 1; i < TEST_MAX_PRODUCERS; i++) {
        pids[i] = jringbuf_add_producer(rb);
        test_assert(pids[i] >= 0, "add producer failed");
    }

    jthread_t threads[TEST_MAX_PRODUCERS];
    thread_arg_t args[TEST_MAX_PRODUCERS];
    for (int i = 0; i < TEST_MAX_PRODUCERS; i++) {
        args[i].rb = rb;
        args[i].id = pids[i];
        args[i].use_ridx = 0;
        jthread_create(&threads[i], NULL, multi_producer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_PRODUCERS; i++) {
        jthread_join(threads[i]);
    }

    /* 总共写入了 4 * 4 = 16 字节 */
    test_assert(jringbuf_size(rb, -1) == 16, "total data wrong");

    /* 读取并简单验证 */
    uint8_t rbuf[16];
    int ret = jringbuf_read(rb, 0, rbuf, 16, JRINGBUF_COMPLETE, 0);
    test_assert(ret == 16, "read all failed");

    jringbuf_uninit(rb);
    printf("Test 6 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 7：多消费者共享读模式
----------------------------------------------------------------------------*/
static jthread_ret_t shared_consumer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint8_t rbuf[8];
    int ret = jringbuf_read(targ->rb, targ->id, rbuf, 8, 0, 0);
    test_assert(ret == 8, "consumer read failed");
    return NULL;
}

static void test_multi_consumer_shared(void)
{
    printf("Test 7: Multi-consumer shared read\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, TEST_MAX_CONSUMERS,
                                   0, 0, JRINGBUF_READ_SHARED);
    /* 添加 4 个消费者 */
    int cids[TEST_MAX_CONSUMERS] = {0};
    cids[0] = 0;
    for (int i = 1; i < TEST_MAX_CONSUMERS; i++) {
        cids[i] = jringbuf_add_consumer(rb, 0); /* 从最新开始 */
        test_assert(cids[i] >= 0, "add consumer failed");
    }

    /* 写入 40 字节 */
    uint8_t wdata[40];
    memset(wdata, 0x5A, 40);
    int ret = jringbuf_write(rb, 0, wdata, 40, 0, 0, 0);
    test_assert(ret == 40, "write failed");

    /* 所有消费者同时读取 8 字节，共享读指针，总读出 32 字节 */
    jthread_t threads[TEST_MAX_CONSUMERS];
    thread_arg_t args[TEST_MAX_CONSUMERS];
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        args[i].rb = rb;
        args[i].id = cids[i];
        args[i].use_ridx = 0;
        jthread_create(&threads[i], NULL, shared_consumer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        jthread_join(threads[i]);
    }

    /* 由于是共享读，移动了 8x4=32 字节全局读指针，数据应还剩 8 字节 */
    test_assert(jringbuf_size(rb, -1) == 8, "shared read size wrong");

    jringbuf_uninit(rb);
    printf("Test 7 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 8：多消费者独占读模式
----------------------------------------------------------------------------*/
static jthread_ret_t exclusive_consumer(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    uint8_t rbuf[4];
    int ret = jringbuf_read(targ->rb, targ->id, rbuf, 4, 0, 0);
    test_assert(ret == 4, "exclusive read failed");
    /* 验证数据内容（每个消费者应该读到相同的部分） */
    test_assert(rbuf[0] == 0, "data corruption");
    return NULL;
}

static void test_multi_consumer_exclusive(void)
{
    printf("Test 8: Multi-consumer exclusive read\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, TEST_MAX_CONSUMERS,
                                   0, 0, JRINGBUF_READ_EXCLUSIVE);
    int cids[TEST_MAX_CONSUMERS] = {0};
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        cids[i] = jringbuf_add_consumer(rb, 1); /* 从最旧数据开始 */
        test_assert(cids[i] >= 0, "add consumer failed");
    }

    /* 写入 16 字节，每个字节值为其索引 */
    uint8_t wdata[16];
    for (int i = 0; i < 16; i++) wdata[i] = (uint8_t)i;
    int ret = jringbuf_write(rb, 0, wdata, 16, 0, 0, 0);
    test_assert(ret == 16, "write failed");

    /* 4 个消费者每人读 4 字节，独占读指针，空间在所有消费后才释放 */
    jthread_t threads[TEST_MAX_CONSUMERS];
    thread_arg_t args[TEST_MAX_CONSUMERS];
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        args[i].rb = rb;
        args[i].id = cids[i];
        args[i].use_ridx = 1;
        jthread_create(&threads[i], NULL, exclusive_consumer, &args[i]);
    }
    for (int i = 0; i < TEST_MAX_CONSUMERS; i++) {
        jthread_join(threads[i]);
    }

    /* 所有消费者都读完后，缓冲区应还剩12字节 */
    test_assert(jringbuf_size(rb, -1) == 12, "buffer should be 12 bytes after all read");

    jringbuf_uninit(rb);
    printf("Test 8 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 9：hold_size 历史数据保留
----------------------------------------------------------------------------*/
static void test_hold_size(void)
{
    printf("Test 9: hold_size history retention\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 2,
                                   TEST_HOLD_SIZE, 0, JRINGBUF_READ_EXCLUSIVE);
    int c1 = jringbuf_add_consumer(rb, 1); /* 从最旧开始 */
    test_assert(c1 >= 0, "add consumer 1 failed");

    /* 写入 60 字节 (capacity 64)，快写满 */
    uint8_t wdata[60];
    memset(wdata, 0xAA, 60);
    int ret = jringbuf_write(rb, 0, wdata, 60, 0, 0, 0);
    test_assert(ret == 60, "write 60 failed");

    /* 消费者 c1 读取 30 字节 */
    uint8_t rbuf[30];
    ret = jringbuf_read(rb, c1, rbuf, 30, 0, 0);
    test_assert(ret == 30, "read 30 failed");

    /* 此时 hold_size=16，min_read_index 被约束在 write_index - hold_size 之内，
       但消费者 c1 已经读到 30 的位置，应该还剩 30 字节 */
    test_assert(jringbuf_size(rb, c1) == 30, "data len should be 30");

    int c2 = jringbuf_add_consumer(rb, 1); /* 从最旧开始 */
    test_assert(c1 >= 0, "add consumer 1 failed");

    /* 由于是惰性更新，且消费者 c1 取的是 min_read_index，应该还剩 60 字节 */
    test_assert(jringbuf_size(rb, c2) == 60, "data len should be 60");

   /* 再写入 30 字节，空间不够，但由于 hold_size 和消费者进度，无法写入 */
    uint8_t more[30];
    memset(more, 0xBB, 30);
    ret = jringbuf_write(rb, 0, more, 30, 0, 0, 0);
    test_assert(ret < 30, "write should fail due to hold_size + consumer lag");

    jringbuf_uninit(rb);
    printf("Test 9 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 10：动态删除生产者和消费者
----------------------------------------------------------------------------*/
static void test_dynamic_del(void)
{
    printf("Test 10: Dynamic add/del producers and consumers\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 3, 3, 0, 0, JRINGBUF_READ_EXCLUSIVE);

    int p1 = jringbuf_add_producer(rb);
    int p2 = jringbuf_add_producer(rb);
    test_assert(p1 >= 0 && p2 >= 0, "add producers failed");

    int c1 = jringbuf_add_consumer(rb, 1);
    int c2 = jringbuf_add_consumer(rb, 1);
    test_assert(c1 >= 0 && c2 >= 0, "add consumers failed");

    /* 删除一个生产者 */
    test_assert(jringbuf_del_producer(rb, p1) == 0, "del producer failed");

    /* 删除所有消费者 */
    test_assert(jringbuf_del_consumer(rb, -1) == 0, "del all consumers failed");
    /* 此时缓冲区应清空 */
    test_assert(jringbuf_size(rb, -1) == 0, "should be empty after del all consumers");

    jringbuf_uninit(rb);
    printf("Test 10 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 11：drop_data 接口
----------------------------------------------------------------------------*/
static void test_drop_data(void)
{
    printf("Test 11: drop_data interface\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 2, 0, 0, JRINGBUF_READ_EXCLUSIVE);
    int c1 = jringbuf_add_consumer(rb, 1);
    int c2 = jringbuf_add_consumer(rb, 0); /* 从 write_index 开始，无历史 */
    test_assert(c1 >= 0 && c2 >= 0, "add consumers failed");

    /* 写入 32 字节 */
    uint8_t wdata[32];
    memset(wdata, 0xCC, 32);
    int ret = jringbuf_write(rb, 0, wdata, 32, 0, 0, 0);
    test_assert(ret == 32, "write failed");

    /* c2 丢弃所有数据 */
    test_assert(jringbuf_drop_data(rb, c2, 0) == 0, "drop_data failed");
    /* c2 的未读数据量应为 0 */
    test_assert(jringbuf_size(rb, c2) == 0, "c2 size should be 0");

    /* c1 丢弃 10 字节 */
    test_assert(jringbuf_drop_data(rb, c1, 10) == 0, "partial drop failed");
    test_assert(jringbuf_size(rb, c1) == 22, "c1 size after partial drop");

    jringbuf_uninit(rb);
    printf("Test 11 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  测试 12：销毁时的安全退出
----------------------------------------------------------------------------*/
static jthread_ret_t slow_reader(void *arg) {
    jringbuf_t *rb = (jringbuf_t*)arg;
    uint8_t buf[1];
    /* 使用无限阻塞读，等待 uninit 唤醒 */
    int ret = jringbuf_read(rb, 0, buf, 1, JRINGBUF_BLOCK, -1);
    test_assert(ret == -1, "should fail due to uninit");
    return NULL;
}

static void test_uninit_safety(void)
{
    printf("Test 12: Safe uninit with waiting threads\n");

    jringbuf_t *rb = jringbuf_init(TEST_CAPACITY, 1, 1, 0, 0, JRINGBUF_READ_SHARED);
    jthread_t reader;
    jthread_create(&reader, NULL, slow_reader, rb);

    /* 保证读者已进入等待 */
    jthread_msleep(10);

    /* 销毁应等待读者退出 */
    jringbuf_uninit(rb);
    jthread_join(reader);

    printf("Test 12 PASSED\n\n");
}

/*----------------------------------------------------------------------------
  主函数
----------------------------------------------------------------------------*/
int main(void)
{
    printf("=== jringbuf Test Suite ===\n\n");

    test_siso_basic();
    test_complete_strategy();
    test_block_strategy();
    test_retry_strategy();
    test_drop_strategy();
    test_multi_producer();
    test_multi_consumer_shared();
    test_multi_consumer_exclusive();
    test_hold_size();
    test_dynamic_del();
    test_drop_data();
    test_uninit_safety();

    printf("All tests PASSED.\n");
    return 0;
}
