/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2026-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   环形缓冲区管理器（按元素操作）
 * @note    支持"单/多"生产者-"单/多"消费者模型，多消费者支持共享模式和独占模式，线程安全
 */
typedef struct jringbuf jringbuf_t;

/**
 * @brief   缓冲区空（不足）时的读取策略或缓冲区满（不足）时的写入策略
 * @note    1. BLOCK和RETRY不能同时选中，默认行为是部分读写，直接返回
 *          2. 完全读指要读取n个元素，循环buf中必须有n个元素可读，不能只读部分
 *          3. 完全写指要写入n个元素，循环buf中必须有n个元素剩余空间，不能只写部分
 *          4. JRINGBUF_DROP共享读和独立读模式都有效，应该在BLOCK或RETRY（若有）后再执行
 */
enum jringbuf_strategy {
    JRINGBUF_COMPLETE = 1,      // 只能完全读写
    JRINGBUF_BLOCK    = 1 << 1, // 阻塞直到有数据读或阻塞直到全部写入（-1无限等待，0直接返回，>0 最多阻塞多少ms）
    JRINGBUF_RETRY    = 1 << 2, // 非阻塞尝试有限次（-1无限尝试，0直接返回，>0 最多尝试多少次）
    JRINGBUF_DROP     = 1 << 3  // 缓冲区满（不足）时写入是否允许丢弃缓冲区旧数据以腾出空间
};

/**
 * @brief   多消费者读指针管理模式
 */
enum jringbuf_read_mode {
    JRINGBUF_READ_SHARED,       // 共享读指针：任一消费者读取都会移动全局读位置
    JRINGBUF_READ_EXCLUSIVE     // 独立读指针：所有消费者都读过某数据后空间才释放
};

/**
 * @brief   缓冲区初始化参数
 * @note    1. hold_num用于更新min_read_index保留一定数量的元素，以便新消费者可以消费历史数据
 *          2. 多生产者需要显式 jringbuf_add_producer，多消费者需要显式 jringbuf_add_consumer，单的无需
 */
typedef struct jringbuf_cfg {
    uint32_t capacity;          // 缓冲区元素个数，内部会向上对齐到2的幂，实际字节容量 = capacity * unit_size
    uint32_t unit_size;         // 单个元素的大小（字节），必须 > 0
    uint32_t max_producers;     // 最大生产者数量（须 ≥ 1；为 1 时不分配生产者数组）
    uint32_t max_consumers;     // 最大消费者数量（须 ≥ 1；为 1 时不分配消费者数组）
    uint32_t hold_num;          // 是否保留一定的历史数据以便新消费者可以消费历史数据（为 0 时不保留），单位：元素个数
    uint32_t wake_num;          // 生产者写入后缓冲区中的元素个数大于等于此项设置时才唤醒消费者，单位：元素个数
    enum jringbuf_read_mode read_mode; // 多消费者时的读模式（max_consumers==1 时忽略，内部强制为 SHARED）
} jringbuf_cfg_t;

/**
 * @brief   创建环形缓冲区
 * @param   cfg          [IN]   创建缓冲区的配置参数
 * @return  成功返回管理器指针；失败返回 NULL
 * @note    1. 结构体、缓冲区、消费者数组、生产者数组分配在连续的一块内存中
 *          2. 条件变量总使用单调时钟
 */
jringbuf_t* jringbuf_init(const jringbuf_cfg_t *cfg);

static inline jringbuf_t* jringbuf_init1(uint32_t capacity,
    uint32_t max_producers, uint32_t max_consumers,
    uint32_t hold_num, uint32_t wake_num, enum jringbuf_read_mode read_mode) {
    jringbuf_cfg_t cfg;
    cfg.capacity = capacity;
    cfg.unit_size = 1;
    cfg.max_producers = max_producers;
    cfg.max_consumers = max_consumers;
    cfg.hold_num = hold_num;
    cfg.wake_num = wake_num;
    cfg.read_mode = read_mode;
    return jringbuf_init(&cfg);
}

/**
 * @brief   销毁环形缓冲区并释放所有资源
 * @param   rb          [IN]    管理器指针
 * @return  无返回值
 * @note    jringbuf_uninit时已自动调用jringbuf_stop停止并禁止读写
 */
void jringbuf_uninit(jringbuf_t *rb);

/**
 * @brief   允许读写
 * @param   rb          [IN]    管理器指针
 * @return  成功返回 0；失败返回 -1
 * @note    jringbuf_init初始化时就允许读写，jringbuf_stop禁止后可使用jringbuf_start恢复允许读写
 */
int jringbuf_start(jringbuf_t *rb);

/**
 * @brief   停止并禁止读写
 * @param   rb          [IN]    管理器指针
 * @return  无返回值
 * @note    需要等待所有读写退出
 */
void jringbuf_stop(jringbuf_t *rb);

/**
 * @brief   获取当前缓冲区中的有效元素个数
 * @param   rb          [IN]    管理器指针
 * @param   consumer_id [IN]    消费者 ID（由 add_consumer 返回；单消费者时固定传 0）
 * @return  有效元素个数
 * @note    在多消费者独占读模式下，consumer_id为-1时获取全局缓冲区中的有效元素个数，
 *          >=0则获取指定消费者的未读元素个数
 */
uint32_t jringbuf_size(jringbuf_t *rb, int consumer_id);

/**
 * @brief   获取缓冲区总容量（元素个数）
 * @param   rb          [IN]    管理器指针
 * @return  容量（元素个数）
 */
uint32_t jringbuf_capacity(jringbuf_t *rb);

/**
 * @brief   获取成员数量
 * @param   rb          [IN]    管理器指针
 * @param   producers   [OUT]   生产者数量(可以为NULL)
 * @param   consumers   [OUT]   消费者数量(可以为NULL)
 * @return  成功返回 0；失败返回 -1
 * @note    如果时单生产者/单消费者，对应参数固定回写1
 */
int jringbuf_members(jringbuf_t *rb, uint32_t *producers, uint32_t *consumers);

/**
 * @brief   向缓冲区写入数据
 * @param   rb          [INOUT] 管理器指针
 * @param   producer_id [IN]    写数据的生产者 ID
 * @param   data        [IN]    待写入数据缓冲区（至少 len * unit_size 字节）
 * @param   len         [IN]    待写入元素个数（须 ≤ 总容量）
 * @param   strategy    [IN]    写满策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @param   pdropped    [INOUT] 传入最少丢弃元素个数，返回实际丢弃数（可以为NULL）
 * @return  成功返回实际写入的元素个数；失败返回 -1
 * @note    1. 多生产者环境下，所有生产者共享同一个写指针，由锁保护
 *          2. pdropped为NULL或值为0时丢弃到满足写入即可，有值时触发丢弃时最少丢弃元素个数不能
 *             小于此值，如果此值比需要的空间小时还是丢弃到满足写入
 */
int jringbuf_write(jringbuf_t *rb, int producer_id, const void *data, uint32_t len, uint32_t strategy, int arg, uint32_t *pdropped);

/**
 * @brief   从缓冲区读取数据
 * @param   rb          [INOUT] 管理器指针
 * @param   consumer_id [IN]    消费者 ID（由 add_consumer 返回；单消费者时固定传 0）
 * @param   buf         [OUT]   读取数据存放缓冲区（至少 len * unit_size 字节）
 * @param   len         [IN]    期望读取的元素个数
 * @param   size        [OUT]   缓冲区当前总有效元素个数(含本次读取的)，可以为NULL
 * @param   strategy    [IN]    缓冲区不足策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @return  成功返回实际读取的元素个数；失败返回 -1（ID 无效或无活跃消费者或数据不足）
 * @note    无
 */
int jringbuf_read(jringbuf_t *rb, int consumer_id, void *buf, uint32_t len, uint32_t* size, uint32_t strategy, int arg);

/**
 * @brief   添加一个生产者（多生产者有效）
 * @param   rb          [INOUT] 管理器指针
 * @return  成功返回生产者 ID（0 ~ max_producers-1）；无可分配槽位返回 -1
 * @note    最大生产者数为 1 时始终返回 ID=0，不会真正分配
 */
int jringbuf_add_producer(jringbuf_t *rb);

/**
 * @brief   移除一个生产者（多生产者有效）
 * @param   rb          [INOUT] 管理器指针
 * @param   producer_id [IN]    要移除的生产者 ID
 * @return  成功返回 0；ID 无效返回 -1
 * @note    producer_id为-1时移除所有生产者
 */
int jringbuf_del_producer(jringbuf_t *rb, int producer_id);

/**
 * @brief   添加一个消费者（多消费者有效）
 * @param   rb          [INOUT] 管理器指针
 * @param   use_ridx    [IN]    初始index是使用min_read_index(1)还是write_index(0)
 * @return  成功返回分配的消费者 ID（0 ~ max_consumers-1）；无可分配槽位返回 -1
 * @note    JRINGBUF_READ_EXCLUSIVE模式时use_ridx才有效
 */
int jringbuf_add_consumer(jringbuf_t *rb, int use_ridx);

/**
 * @brief   移除一个消费者（多消费者有效）
 * @param   rb          [INOUT] 管理器指针
 * @param   consumer_id [IN]    要移除的消费者 ID
 * @return  成功返回 0；ID 无效返回 -1
 * @note    1. 移除后立即更新最小读索引，可能释放空间给写者
 *          2. consumer_id为-1时移除所有消费者
 */
int jringbuf_del_consumer(jringbuf_t *rb, int consumer_id);

/**
 * @brief   丢弃老的消费者数据
 * @param   rb          [INOUT] 管理器指针
 * @param   consumer_id [IN]    要清空的消费者 ID
 * @param   dropped     [IN]    丢弃元素个数（0为全部丢弃）
 * @return  成功返回 0；失败返回 -1
 * @note    consumer_id为-1时清空所有消费者数据
 */
int jringbuf_drop_data(jringbuf_t *rb, int consumer_id, uint32_t dropped);

#ifdef __cplusplus
}
#endif

