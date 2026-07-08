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
 * @brief   带索引的数据环形缓冲区管理器
 * @note    支持"单/多"生产者-"单/多"消费者模型，多消费者支持共享模式和独占模式，线程安全
 */
typedef struct jringdata jringdata_t;

/**
 * @brief   缓冲区空（不足）时的读取策略或缓冲区满（不足）时的写入策略
 * @note    1. BLOCK和RETRY不能同时选中，默认行为是部分读写，直接返回
 *          2. 完全读指要读取n个索引数据，循环buf中必须有n个索引数据读，不能只读部分
 *          3. 完全写指要写入n个索引数据，循环buf中必须有n字节剩余空间写，不能只写部分
 *          4. JRINGDATA_DROP共享读和独立读模式都有效，应该在BLOCK或RETRY（若有）后再执行
 *          5. 读/写索引数据的关联裸数据必须完整读写
 */
enum jringdata_strategy {
    JRINGDATA_COMPLETE = 1,     // 只能完全读写
    JRINGDATA_BLOCK    = 1 << 1,// 阻塞直到有数据读或阻塞直到全部写入（-1无限等待，0直接返回，>0 最多阻塞多少ms）
    JRINGDATA_RETRY    = 1 << 2,// 非阻塞尝试有限次（-1无限尝试，0直接返回，>0 最多尝试多少次）
    JRINGDATA_DROP     = 1 << 3 // 缓冲区满（不足）时写入是否允许丢弃缓冲区旧数据以腾出空间
};

/**
 * @brief   多消费者读指针管理模式
 */
enum jringdata_read_mode {
    JRINGDATA_READ_SHARED,      // 共享读指针：任一消费者读取都会移动全局读位置
    JRINGDATA_READ_EXCLUSIVE    // 独立读指针：所有消费者都读过某数据后空间才释放
};

/**
 * @brief   缓冲区初始化参数
 * @note    1. hold_num用于更新min_read_index保留一定size，以便可以新消费者可以消费历史数据
 *          2. 多生产者需要显式 jringdata_add_producer，多消费者需要显式 jringdata_add_consumer，单的无需
 */
typedef struct jringdata_cfg {
    uint32_t idx_num;           // 索引缓冲区的索引数量，内部会向上对齐到2的幂
    uint32_t idx_size;          // 索引结构体单元大小
    uint32_t capacity;          // 裸数据缓冲区容量（字节），内部会向上对齐到2的幂
    uint32_t max_producers;     // 最大生产者数量（须 ≥ 1；为 1 时不分配生产者数组）
    uint32_t max_consumers;     // 最大消费者数量（须 ≥ 1；为 1 时不分配消费者数组）
    uint32_t hold_num;          // 是否保留一定的历史数据以便可以新消费者可以消费历史数据（为 0 时不保留）
    uint32_t wake_num;          // 生产者写入后缓冲中的数据大小大于等于此项设置时才唤醒消费者
    enum jringdata_read_mode read_mode; // 多消费者时的读模式（max_consumers==1 时忽略，内部强制为 SHARED）
    uint32_t (*get_size)(const void *idx); // 通过idx获取裸数据大小，不填时直接将idx的前4字节当作uint32_t获取值
} jringdata_cfg_t;

/**
 * @brief   创建带索引的数据环形缓冲区
 * @param   cfg          [IN]   创建缓冲区的配置参数
 * @return  成功返回管理器指针；失败返回 NULL
 * @note    1. 结构体、缓冲区、消费者数组、生产者数组分配在连续的一块内存中
 *          2. 条件变量总使用单调时钟
 */
jringdata_t* jringdata_init(const jringdata_cfg_t *cfg);

/**
 * @brief   销毁带索引的数据环形缓冲区并释放所有资源
 * @param   rd          [IN]    管理器指针
 * @return  无返回值
 * @note    jringdata_uninit时已自动调用jringdata_stop停止并禁止读写
 */
void jringdata_uninit(jringdata_t *rd);

/**
 * @brief   允许读写
 * @param   rd          [IN]    管理器指针
 * @return  成功返回 0；失败返回 -1
 * @note    jringdata_init初始化时就允许读写，jringdata_stop禁止后可使用jringdata_start恢复允许读写
 */
int jringdata_start(jringdata_t *rd);

/**
 * @brief   停止并禁止读写
 * @param   rd          [IN]    管理器指针
 * @return  无返回值
 * @note    需要等待所有读写退出
 */
void jringdata_stop(jringdata_t *rd);

/**
 * @brief   获取当前缓冲区中的有效索引个数和裸数据字节数
 * @param   rd          [IN]    管理器指针
 * @param   consumer_id [IN]    消费者 ID（由 add_consumer 返回；单消费者时固定传 0）
 * @param   data_size   [OUT]   获取到的裸数据缓冲区的有效字节数，可以为NULL不获取
 * @return  有效索引个数
 * @note    在多消费者独占读模式下，consumer_id为-1时获取全局缓冲区中的
 *          有效数据字节数（jringdata_init），>=0则获取指定消费者的
 */
uint32_t jringdata_size(jringdata_t *rd, int consumer_id, uint32_t *data_size);

/**
 * @brief   获取缓冲区总容量
 * @param   rd          [IN]    管理器指针
 * @param   data_size   [OUT]   获取到的裸数据缓冲区的容量字节数，可以为NULL不获取
 * @return  索引容量个数（字节数）
 */
uint32_t jringdata_capacity(jringdata_t *rd, uint32_t *data_size);

/**
 * @brief   获取成员数量
 * @param   rd          [IN]    管理器指针
 * @param   producers   [OUT]   生产者数量(可以为NULL)
 * @param   consumers   [OUT]   消费者数量(可以为NULL)
 * @return  成功返回 0；失败返回 -1
 * @note    如果时单生产者/单消费者，对应参数固定回写1
 */
int jringdata_members(jringdata_t *rd, uint32_t *producers, uint32_t *consumers);

/**
 * @brief   向缓冲区写入数据
 * @param   rd          [INOUT] 管理器指针
 * @param   producer_id [IN]    写数据的生产者 ID
 * @param   idx         [IN]    待写入索引缓冲区
 * @param   num         [IN]    待写入索引个数
 * @param   data        [IN]    待写入数据缓冲区
 * @param   strategy    [IN]    写满策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @param   pdropped    [INOUT] 传入最少丢弃索引个数，返回实际丢弃数（可以为NULL）
 * @return  成功返回写入索引数；失败返回 -1
 * @note    1. data的总长度通过idx的长度获取回调的值相加得到
 *          2. 多生产者环境下，所有生产者共享同一个写指针，由锁保护
 *          3. pdropped为NULL或值为0时丢弃到满足写入即可，有值时触发丢弃时最少丢弃索引个数不能
 *             小于此值，如果此值比需要的空间小时还是丢弃到满足写入
 */
int jringdata_write(jringdata_t *rd, int producer_id, const void *idx, uint32_t num,
    const void *data, uint32_t strategy, int arg, uint32_t *pdropped);

/**
 * @brief   向缓冲区写入数据（分散写入）
 * @param   rd          [INOUT] 管理器指针
 * @param   producer_id [IN]    写数据的生产者 ID
 * @param   idx         [IN]    待写入索引指针缓冲区的数组
 * @param   num         [IN]    待写入索引个数
 * @param   data        [IN]    待写入数据指针缓冲区的数组
 * @param   strategy    [IN]    写满策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @param   pdropped    [INOUT] 传入最少丢弃索引个数，返回实际丢弃数（可以为NULL）
 * @return  成功返回写入索引数；失败返回 -1
 * @note    1. data中每个数据指针指向对象的长度通过idx的长度获取回调的值得到
 *          2. idx和data分别存储指针数组，两个数组的指针分别指向索引和裸数据
 *          3. pdropped为NULL或值为0时丢弃到满足写入即可，有值时触发丢弃时最少丢弃索引个数不能
 *             小于此值，如果此值比需要的空间小时还是丢弃到满足写入
 */
int jringdata_writev(jringdata_t *rd, int producer_id, const void **idx, uint32_t num,
    const void **data, uint32_t strategy, int arg, uint32_t *pdropped);

/**
 * @brief   从缓冲区读取数据
 * @param   rd          [INOUT] 管理器指针
 * @param   consumer_id [IN]    消费者 ID（由 add_consumer 返回；单消费者时固定传 0）
 * @param   idx         [OUT]   读取索引存放缓冲区
 * @param   num         [IN]    期望读取的索引数
 * @param   data        [OUT]   读取数据存放缓冲区
 * @param   len         [IN]    数据存放缓冲区的分配空间字节数
 * @param   total_num   [OUT]   索引缓冲区总索引数(含本次读取的)，可以为NULL
 * @param   total_size  [OUT]   数据缓冲区总字节数(含本次读取的)，可以为NULL
 * @param   strategy    [IN]    缓冲区不足策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @return  成功返回读取索引数；失败返回 -1（ID 无效或无活跃消费者或数据不足）
 * @note    无
 */
int jringdata_read(jringdata_t *rd, int consumer_id, void *idx, uint32_t num,
    void *data, uint32_t len, uint32_t* total_num, uint32_t* total_size, uint32_t strategy, int arg);

/**
 * @brief   从缓冲区读取数据（分散读取）
 * @param   rd          [INOUT] 管理器指针
 * @param   consumer_id [IN]    消费者 ID（由 add_consumer 返回；单消费者时固定传 0）
 * @param   idx         [OUT]   读取索引指针存放缓冲区的数组
 * @param   num         [IN]    期望读取的索引数
 * @param   data        [OUT]   读取数据指针存放缓冲区的数组
 * @param   len         [IN]    data数组的每个指针成员指向的对象的分配空间字节数的数组
 * @param   total_num   [OUT]   索引缓冲区总索引数(含本次读取的)，可以为NULL
 * @param   total_size  [OUT]   数据缓冲区总字节数(含本次读取的)，可以为NULL
 * @param   strategy    [IN]    缓冲区不足策略
 * @param   arg         [IN]    策略的参数，含义依赖策略（超时时间或尝试次数）
 * @return  成功返回读取索引数；失败返回 -1（ID 无效或无活跃消费者或数据不足）
 * @note    idx和data分别存储指针数组，两个数组的指针分别指向索引和裸数据，内存空间需要预分配
 */
int jringdata_readv(jringdata_t *rd, int consumer_id, void **idx, uint32_t num,
    void **data, uint32_t *len, uint32_t* total_num, uint32_t* total_size, uint32_t strategy, int arg);

/**
 * @brief   添加一个生产者（多生产者有效）
 * @param   rd          [INOUT] 管理器指针
 * @return  成功返回生产者 ID（0 ~ max_producers-1）；无可分配槽位返回 -1
 * @note    最大生产者数为 1 时始终返回 ID=0，不会真正分配
 */
int jringdata_add_producer(jringdata_t *rd);

/**
 * @brief   移除一个生产者（多生产者有效）
 * @param   rd          [INOUT] 管理器指针
 * @param   producer_id [IN]    要移除的生产者 ID
 * @return  成功返回 0；ID 无效返回 -1
 * @note    producer_id为-1时移除所有生产者
 */
int jringdata_del_producer(jringdata_t *rd, int producer_id);

/**
 * @brief   添加一个消费者（多消费者有效）
 * @param   rd          [INOUT] 管理器指针
 * @param   use_ridx    [IN]    初始index是使用使用min_read_index(1)还是write_index(0)
 * @return  成功返回分配的消费者 ID（0 ~ max_consumers-1）；无可分配槽位返回 -1
 * @note    JRINGDATA_READ_EXCLUSIVE模式时use_ridx才有效
 */
int jringdata_add_consumer(jringdata_t *rd, int use_ridx);

/**
 * @brief   移除一个消费者（多消费者有效）
 * @param   rd          [INOUT] 管理器指针
 * @param   consumer_id [IN]    要移除的消费者 ID
 * @return  成功返回 0；ID 无效返回 -1
 * @note    1. 移除后立即更新最小读索引，可能释放空间给写者
 *          2. consumer_id为-1时移除所有消费者
 */
int jringdata_del_consumer(jringdata_t *rd, int consumer_id);

/**
 * @brief   丢弃老的消费者数据
 * @param   rd          [INOUT] 管理器指针
 * @param   consumer_id [IN]    要清空的消费者 ID
 * @param   dropped     [IN]    丢弃索引数（0为全部丢弃）
 * @return  成功返回 0；失败返回 -1
 * @note    consumer_id为-1时清空所有消费者数据
 */
int jringdata_drop_data(jringdata_t *rd, int consumer_id, uint32_t dropped);

#ifdef __cplusplus
}
#endif

