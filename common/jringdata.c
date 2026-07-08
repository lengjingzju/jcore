/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2026-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "jringdata.h"
#include "jthread.h"
#include "jheap.h"

/*----------------------------------------------------------------------------
  带索引的数据环形缓冲区主结构（柔性数组布局）
----------------------------------------------------------------------------*/

/**
 * @brief   环形缓冲区上下文（索引或数据）
 */
struct jringdata_ctx {
    uint32_t        total_len;          // 总的数据大小（索引个数或字节数，对齐到2的幂）
    uint32_t        unit_size;          // 索引结构体单元大小；数据缓冲区恒为1
    uint32_t        data_len;           // 有效数据量（索引个数或字节数）
    uint32_t        write_index;        // 绝对写位置，单调递增，利用自然溢出
    uint32_t        min_read_index;     // 活跃消费者中最小的读位置
    uint32_t        buf_offset;         // 缓冲区起始偏移
    uint32_t        read_index_offset;  // 消费者读索引数组偏移（仅独立读有效）
};

/**
 * @brief   带索引的数据环形缓冲区管理器
 *
 * 尾部连续内存布局（由偏移量索引）：
 *   - index[idx_num * idx_size]        索引缓冲区
 *   - data[capacity]                   数据缓冲区
 *   - read_idx[max_consumers]          消费者索引读位置数组 (uint32_t)，仅 max_consumers>1且JRINGDATA_READ_EXCLUSIVE
 *   - read_data[max_consumers]         消费者数据读位置数组 (uint32_t)，仅 max_consumers>1且JRINGDATA_READ_EXCLUSIVE
 *   - producer[max_producers]          生产者有效性数组 (uint8_t)，仅 max_producers>1
 *   - consumer[max_consumers]          消费者有效性数组 (uint8_t)，仅 max_consumers>1
 */
struct jringdata {
    uint32_t        max_producers;      // 最大生产者数量
    uint32_t        cur_producers;      // 当前生产者数量
    uint32_t        max_consumers;      // 最大消费者数量
    uint32_t        cur_consumers;      // 当前消费者数量
    uint32_t        hold_num;           // 历史窗口大小（索引个数）
    uint32_t        wake_num;           // 唤醒窗口大小（索引个数）
    enum jringdata_read_mode read_mode; // 读指针管理模式
    uint8_t         disable_rw;         // 是否禁止读写
    uint8_t         min_read_stale;     // 1 表示 min_read_index 需要重新计算（惰性）
    uint8_t         min_read_lock;      // 1 表示正在读取数据，不能改变 min_read_index (单消费者)
    uint32_t        rw_count;           // 正在读写的生产者或消费者数目
    uint32_t        total_size;         // 数据区总大小
    uint32_t        producer_offset;    // 生产者有效性数组偏移
    uint32_t        consumer_offset;    // 消费者有效性数组偏移
    uint32_t (*get_size)(const void *idx); // 通过idx获取裸数据大小，不填时直接将idx的前4字节当作uint32_t获取值

    struct jringdata_ctx idx_ctx;       // 索引环形缓冲区上下文
    struct jringdata_ctx data_ctx;      // 数据环形缓冲区上下文

    jthread_mutex_t mutex;              // 全局互斥锁
    jthread_cond_t  not_empty;          // 数据可用条件变量（单调时钟）
    jthread_cond_t  not_full;           // 空间可用条件变量（单调时钟）

    uint8_t         data[];             // 柔性数组起始
};

/*----------------------------------------------------------------------------
  内部宏：通过偏移访问各数组
----------------------------------------------------------------------------*/

#define JRD_IDX_BUF(rd)     ((uint8_t*)((rd)->data))                                    // 索引缓冲区起始
#define JRD_DATA_BUF(rd)    ((uint8_t*)((rd)->data + (rd)->data_ctx.buf_offset))        // 数据缓冲区起始
#define JRD_RIDX_ARR(rd)    ((uint32_t*)((rd)->data + (rd)->idx_ctx.read_index_offset)) // 消费者索引读位置数组
#define JRD_RDATA_ARR(rd)   ((uint32_t*)((rd)->data + (rd)->data_ctx.read_index_offset))// 消费者数据读位置数组
#define JRD_PROD_ACT(rd)    ((uint8_t*)((rd)->data + (rd)->producer_offset))            // 生产者活跃数组
#define JRD_CONS_ACT(rd)    ((uint8_t*)((rd)->data + (rd)->consumer_offset))            // 消费者活跃数组

/*----------------------------------------------------------------------------
  内部辅助函数
----------------------------------------------------------------------------*/

/**
 * @brief   将整数向上对齐到下一个 2 的幂
 */
static inline uint32_t next_pow2(uint32_t n)
{
    if (n == 0)
        return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

/**
 * @brief   获取索引项的数据长度默认函数
 */
static inline uint32_t get_size_def(const void *idx)
{
    return *(const uint32_t *)idx;
}

/**
 * @brief   计算输入索引的数据总长度
 * @param   rd          [IN] 管理器指针
 * @param   idx         [IN] 输入的索引数组
 * @param   num         [IN] 索引个数
 * @return  对应的数据总字节数
 */
static uint32_t calc_data_len_for_input_idx(jringdata_t *rd, const void *idx, uint32_t num)
{
    const uint8_t *idx_entry = (const uint8_t *)idx;
    uint32_t idx_size = rd->idx_ctx.unit_size;
    uint32_t total = 0;
    for (uint32_t i = 0; i < num; ++i) {
        total += rd->get_size((const void *)idx_entry);
        idx_entry += idx_size;
    }
    return total;
}

/**
 * @brief   计算输入索引的数据总长度（分散索引）
 * @param   rd          [IN] 管理器指针
 * @param   idx         [IN] 输入的索引数组
 * @param   num         [IN] 索引个数
 * @return  对应的数据总字节数
 */
static uint32_t calc_data_len_for_input_idxv(jringdata_t *rd, const void **idx, uint32_t num)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < num; ++i) {
        total += rd->get_size(idx[i]);
    }
    return total;
}

/**
 * @brief   计算从指定索引位置开始的若干索引的数据总长度
 * @param   rd          [IN] 管理器指针
 * @param   start_idx   [IN] 起始索引绝对位置（逻辑序号）
 * @param   num         [IN] 索引个数
 * @return  对应的数据总字节数
 * @note    假定 start_idx 和 num 在有效范围内，且索引数据未被覆盖
 */
static uint32_t calc_data_len_for_idx_range(jringdata_t *rd, uint32_t start_idx, uint32_t num)
{
    uint8_t *idx_buf = JRD_IDX_BUF(rd);
    uint32_t idx_size = rd->idx_ctx.unit_size;
    uint32_t idx_mask = rd->idx_ctx.total_len - 1;
    uint32_t pos = start_idx & idx_mask;
    uint32_t total = 0;

    for (uint32_t i = 0; i < num; ++i) {
        const void *idx_entry = (const void *)(idx_buf + (pos * idx_size));
        total += rd->get_size(idx_entry);
        pos = (pos + 1) & idx_mask;
    }
    return total;
}

/**
 * @brief   遍历活跃消费者并重新计算 min_read_index 及 data_len（同时更新 idx 和 data）
 */
static void update_min_read_index(jringdata_t *rd)
{
    uint32_t min_idx = rd->idx_ctx.write_index;   /* 初始设为最远位置 */
    uint32_t min_data = rd->data_ctx.write_index;

    /* 1. 基于活跃消费者计算“最慢读指针” */
    if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
        uint32_t *ridx = JRD_RIDX_ARR(rd);
        uint32_t *rdata = JRD_RDATA_ARR(rd);
        uint8_t  *act = JRD_CONS_ACT(rd);

        for (uint32_t i = 0, j = 0; i < rd->max_consumers && j < rd->cur_consumers; ++i) {
            if (act[i]) {
                /* 利用无符号减法自然溢出：距离大的更旧 */
                ++j;
                if ((rd->idx_ctx.write_index - ridx[i]) > (rd->idx_ctx.write_index - min_idx)) {
                    min_idx = ridx[i];
                    /* 上面判断成立同时((rd->data_ctx.write_index - rdata[i]) > (rd->data_ctx.write_index - min_data))也应该成立 */
                    min_data = rdata[i];
                }
            }
        }
    }

    /* 2. 如果没有活跃消费者，min_idx 和 min_data 保持为各自的 write_index */
    /* 3. 处理 hold_num：保留至少 hold_num 个索引的历史数据 */
    if (rd->hold_num > 0) {
        uint32_t old_min_idx = rd->idx_ctx.min_read_index;
        uint32_t old_min_data = rd->data_ctx.min_read_index;
        uint32_t hold_min_idx = rd->idx_ctx.write_index - rd->hold_num;

        // 目标下限是 hold_min_idx，但绝不能回退到 old_min_idx 之前
        uint32_t target_min_idx = hold_min_idx;
        if ((rd->idx_ctx.write_index - old_min_idx) < rd->hold_num) {
            // 之前已经比 hold_min 更靠后了，就从 old_min 往前推进
            target_min_idx = old_min_idx;
        }
        if ((rd->idx_ctx.write_index - target_min_idx) > (rd->idx_ctx.write_index - min_idx)) {
            min_idx = target_min_idx;
            // 对于 data，需要根据 min_idx 计算对应的 data 偏移
            if (rd->idx_ctx.write_index - min_idx < min_idx - old_min_idx) {
                min_data = rd->data_ctx.write_index - calc_data_len_for_idx_range(
                    rd, min_idx, rd->idx_ctx.write_index - min_idx);
            } else {
                min_data = old_min_data + calc_data_len_for_idx_range(
                    rd, old_min_idx, min_idx - old_min_idx);
            }
        }
    }

    /* 4. 更新全局状态 */
    rd->idx_ctx.min_read_index = min_idx;
    rd->idx_ctx.data_len = rd->idx_ctx.write_index - min_idx;
    rd->data_ctx.min_read_index = min_data;
    rd->data_ctx.data_len = rd->data_ctx.write_index - min_data;
    rd->min_read_stale = 0;
}

/**
 * @brief   尝试丢弃旧数据以腾出空间（内部函数，需在持锁且无人读时调用）
 * @param   rd          [IN] 管理器指针
 * @param   pdropped    [INOUT] 传入最少丢弃索引个数，返回实际丢弃数（可以为NULL）
 * @param   need_idx    [IN] 需要的索引空间
 * @param   need_data   [IN] 需要的数据空间
 * @return  0成功，-1失败（无法释放足够空间）
 */
static int drop_old_data(jringdata_t *rd, uint32_t *pdropped, uint32_t need_idx, uint32_t need_data)
{
    uint32_t min_drop = pdropped ? *pdropped : 0;
    uint32_t dropped = 0;
    uint32_t idx_space, data_space;

    // 如果 min_drop 大于现有数据，直接清空全部数据
    if (min_drop >= rd->idx_ctx.data_len) {
        // 检查总容量是否足够（清空后空间 = 总容量）
        if (rd->idx_ctx.total_len < need_idx || rd->data_ctx.total_len < need_data) {
            return -1; // 总容量不足，无法满足
        }
        if (pdropped)
            *pdropped = rd->idx_ctx.data_len;
        // 清空所有数据
        rd->idx_ctx.min_read_index = rd->idx_ctx.write_index;
        rd->idx_ctx.data_len = 0;
        rd->data_ctx.min_read_index = rd->data_ctx.write_index;
        rd->data_ctx.data_len = 0;
        if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
            uint32_t *ridx = JRD_RIDX_ARR(rd);
            uint32_t *rdata = JRD_RDATA_ARR(rd);
            uint8_t *act = JRD_CONS_ACT(rd);
            for (uint32_t i = 0, j = 0; i < rd->max_consumers && j < rd->cur_consumers; ++i) {
                if (act[i]) {
                    ++j;
                    ridx[i] = rd->idx_ctx.write_index;
                    rdata[i] = rd->data_ctx.write_index;
                }
            }
        }
        return 0;
    }

    // 逐步丢弃，直到空间满足且丢弃数达到 min_drop
    while (1) {
        idx_space = rd->idx_ctx.total_len - rd->idx_ctx.data_len;
        data_space = rd->data_ctx.total_len - rd->data_ctx.data_len;

        // 退出条件：空间满足，且（丢弃数达标 或 数据已空）
        if (idx_space >= need_idx && data_space >= need_data &&
            (dropped >= min_drop || rd->idx_ctx.data_len == 0)) {
            break; // 成功
        }

        // 数据已空但空间仍未满足，无法继续
        if (rd->idx_ctx.data_len == 0) {
            return -1; // 空间仍不足
        }

        // 获取最旧索引的数据长度（可能为0）
        uint32_t oldest_data_len = calc_data_len_for_idx_range(rd, rd->idx_ctx.min_read_index, 1);

        // 移动所有消费者的读位置（独占模式）
        if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
            uint32_t *ridx = JRD_RIDX_ARR(rd);
            uint32_t *rdata = JRD_RDATA_ARR(rd);
            uint8_t *act = JRD_CONS_ACT(rd);
            for (uint32_t i = 0, j = 0; i < rd->max_consumers && j < rd->cur_consumers; ++i) {
                if (act[i]) {
                    ++j;
                    // 如果该消费者当前正好指着最旧索引，则推进
                    if (ridx[i] == rd->idx_ctx.min_read_index) {
                        ridx[i] += 1;
                        rdata[i] += oldest_data_len;
                    }
                }
            }
        }

        // 更新全局 min
        rd->idx_ctx.min_read_index += 1;
        rd->idx_ctx.data_len -= 1;
        rd->data_ctx.min_read_index += oldest_data_len;
        rd->data_ctx.data_len -= oldest_data_len;
        ++dropped;
    }

    if (pdropped)
        *pdropped = dropped;
    return 0;
}

/*----------------------------------------------------------------------------
  核心接口实现
----------------------------------------------------------------------------*/

/**
 * @brief   创建带索引的数据环形缓冲区
 */
jringdata_t* jringdata_init(const jringdata_cfg_t *cfg)
{
    if (!cfg || cfg->idx_num == 0 || cfg->idx_size == 0 || cfg->capacity == 0 ||
        cfg->max_producers < 1 || cfg->max_consumers < 1) {
        return NULL;
    }

    uint32_t idx_num = next_pow2(cfg->idx_num);
    uint32_t capacity = next_pow2(cfg->capacity);
    uint32_t idx_buf_size = idx_num * cfg->idx_size;
    uint32_t data_buf_size = capacity;
    uint32_t ridx_size = (cfg->max_consumers > 1 && cfg->read_mode == JRINGDATA_READ_EXCLUSIVE) ?  cfg->max_consumers * sizeof(uint32_t) : 0;
    uint32_t rdata_size = (cfg->max_consumers > 1 && cfg->read_mode == JRINGDATA_READ_EXCLUSIVE) ?  cfg->max_consumers * sizeof(uint32_t) : 0;
    uint32_t prod_act_size = (cfg->max_producers > 1) ? cfg->max_producers * sizeof(uint8_t) : 0;
    uint32_t cons_act_size = (cfg->max_consumers > 1) ? cfg->max_consumers * sizeof(uint8_t) : 0;

    uint32_t total = (uint32_t)sizeof(jringdata_t) + idx_buf_size + data_buf_size +
                   ridx_size + rdata_size + prod_act_size + cons_act_size;

    jringdata_t *rd = (jringdata_t*)jheap_malloc(total);
    if (!rd) {
        return NULL;
    }

    memset(rd, 0, sizeof(jringdata_t));
    rd->max_producers = cfg->max_producers;
    rd->cur_producers = 0;
    rd->max_consumers = cfg->max_consumers;
    rd->cur_consumers = 0;
    rd->hold_num = cfg->hold_num;
    rd->wake_num = cfg->wake_num;
    rd->read_mode = (cfg->max_consumers == 1) ? JRINGDATA_READ_SHARED : cfg->read_mode;
    rd->disable_rw = 0;
    rd->min_read_stale = 0;
    rd->min_read_lock = 0;
    rd->rw_count = 0;
    rd->get_size = cfg->get_size ? cfg->get_size : get_size_def;

    rd->idx_ctx.total_len = idx_num;
    rd->idx_ctx.unit_size = cfg->idx_size;
    rd->data_ctx.total_len = capacity;
    rd->data_ctx.unit_size = 1;

    /* 计算各偏移 */
    uint32_t off = 0;
    rd->idx_ctx.buf_offset = off;
    off += idx_buf_size;
    rd->data_ctx.buf_offset = off;
    off += data_buf_size;
    rd->idx_ctx.read_index_offset = ridx_size ? off : 0;
    off += ridx_size;
    rd->data_ctx.read_index_offset = rdata_size ? off : 0;
    off += rdata_size;
    rd->producer_offset = prod_act_size ? off : 0;
    off += prod_act_size;
    rd->consumer_offset = cons_act_size ? off : 0;
    off += cons_act_size;
    rd->total_size = off;

    /* 清空尾部未使用的内存（如有） */
    if (off > idx_buf_size + data_buf_size) {
        memset(rd->data + idx_buf_size + data_buf_size, 0, off - idx_buf_size - data_buf_size);
    }

    jthread_mutex_init(&rd->mutex);
    jthread_cond_init(&rd->not_empty, 1);     /* 单调时钟 */
    jthread_cond_init(&rd->not_full,  1);

    return rd;
}

/**
 * @brief   销毁带索引的数据环形缓冲区
 */
void jringdata_uninit(jringdata_t *rd)
{
    if (!rd)
        return;

    jringdata_stop(rd);
    jthread_mutex_destroy(&rd->mutex);
    jthread_cond_destroy(&rd->not_empty);
    jthread_cond_destroy(&rd->not_full);
    jheap_free(rd);
}

/**
 * @brief   允许读写
 */
int jringdata_start(jringdata_t *rd)
{
    if (!rd)
        return -1;

    jthread_mutex_lock(&rd->mutex);
    rd->disable_rw = 0;
    jthread_mutex_unlock(&rd->mutex);
    return 0;
}

/**
 * @brief   停止并禁止读写
 */
void jringdata_stop(jringdata_t *rd)
{
    if (!rd)
        return;

    jthread_mutex_lock(&rd->mutex);
    rd->disable_rw = 1;
    while (rd->rw_count) {
        jthread_cond_broadcast(&rd->not_empty);
        jthread_cond_broadcast(&rd->not_full);
        jthread_mutex_unlock(&rd->mutex);
        jthread_yield();
        jthread_mutex_lock(&rd->mutex);
    }
    jthread_mutex_unlock(&rd->mutex);
}

/**
 * @brief   获取当前缓冲区中的有效索引个数和裸数据字节数
 */
uint32_t jringdata_size(jringdata_t *rd, int consumer_id, uint32_t *data_size)
{
    if (!rd)
        return 0;

    jthread_mutex_lock(&rd->mutex);

    if (rd->min_read_stale && rd->read_mode == JRINGDATA_READ_EXCLUSIVE && consumer_id == -1) {
        update_min_read_index(rd);
    }

    uint32_t idx_sz = 0;
    uint32_t data_sz = 0;

    if (consumer_id == -1) {
        idx_sz = rd->idx_ctx.data_len;
        data_sz = rd->data_ctx.data_len;
    } else if (rd->max_consumers == 1) {
        idx_sz = rd->idx_ctx.data_len;
        data_sz = rd->data_ctx.data_len;
    } else if (rd->read_mode == JRINGDATA_READ_SHARED) {
        idx_sz = rd->idx_ctx.data_len;
        data_sz = rd->data_ctx.data_len;
    } else {
        if (consumer_id >= 0 && (uint32_t)consumer_id < rd->max_consumers) {
            uint8_t *act = JRD_CONS_ACT(rd);
            if (act[consumer_id]) {
                uint32_t *ridx = JRD_RIDX_ARR(rd);
                uint32_t *rdata = JRD_RDATA_ARR(rd);
                idx_sz = rd->idx_ctx.write_index - ridx[consumer_id];
                data_sz = rd->data_ctx.write_index - rdata[consumer_id];
            }
        }
    }

    jthread_mutex_unlock(&rd->mutex);

    if (data_size)
        *data_size = data_sz;
    return idx_sz;
}

/**
 * @brief   获取缓冲区总容量
 */
uint32_t jringdata_capacity(jringdata_t *rd, uint32_t *data_size)
{
    if (!rd) {
        if (data_size)
            *data_size = 0;
        return 0;
    }
    if (data_size)
        *data_size = rd->data_ctx.total_len;
    return rd->idx_ctx.total_len;
}

/**
 * @brief   获取成员数量
 */
int jringdata_members(jringdata_t *rd, uint32_t *producers, uint32_t *consumers)
{
    if (!rd)
        return -1;
    if (!producers && !consumers)
        return -1;

    jthread_mutex_lock(&rd->mutex);
    if (producers)
        *producers = rd->max_producers > 1 ? rd->cur_producers : 1;
    if (consumers)
        *consumers = rd->max_consumers > 1 ? rd->cur_consumers : 1;
    jthread_mutex_unlock(&rd->mutex);
    return 0;
}

/*----------------------------------------------------------------------------
  Write / Writev 公共参数结构（内部使用）
----------------------------------------------------------------------------*/

/**
 * @brief   连续写入参数
 */
struct jringdata_write_continuous_arg {
    const void *idx;
    const void *data;
};

/**
 * @brief   分散写入参数
 */
struct jringdata_write_discrete_arg {
    const void **idx;
    const void **data;
};

/**
 * @brief   写入参数联合体
 */
struct jringdata_write_arg {
    int32_t is_discrete;
    uint32_t num;
    union {
        struct jringdata_write_continuous_arg c;
        struct jringdata_write_discrete_arg d;
    } v;
};

/*----------------------------------------------------------------------------
  Read / Readv 公共参数结构（内部使用）
----------------------------------------------------------------------------*/

/**
 * @brief   连续读取参数
 */
struct jringdata_read_continuous_arg {
    void *idx;
    void *data;
    uint32_t len;
};

/**
 * @brief   分散读取参数
 */
struct jringdata_read_discrete_arg {
    void **idx;
    void **data;
    uint32_t *len;
};

/**
 * @brief   读取参数联合体
 */
struct jringdata_read_arg {
    int32_t is_discrete;
    uint32_t num;
    union {
        struct jringdata_read_continuous_arg c;
        struct jringdata_read_discrete_arg d;
    } v;
};

/*----------------------------------------------------------------------------
  write_common：公共写入流程（所有逻辑内联）
----------------------------------------------------------------------------*/

/**
 * @brief   计算当前最大可写索引数及所需数据空间
 */
static uint32_t calc_write_size(jringdata_t *rd, struct jringdata_write_arg *arg, uint32_t *data_len)
{
    uint32_t max_can = 0;
    uint32_t data_need = 0;
    uint32_t num = arg->num;
    uint32_t idx_space = rd->idx_ctx.total_len - rd->idx_ctx.data_len;
    uint32_t data_space = rd->data_ctx.total_len - rd->data_ctx.data_len;

    if (!arg->is_discrete) {
        const void *idx_ptr = arg->v.c.idx;
        uint32_t idx_size = rd->idx_ctx.unit_size;
        const uint8_t *idx_src = (const uint8_t *)idx_ptr;
        for (uint32_t i = 0; i < num && i < idx_space; ++i) {
            uint32_t dlen = rd->get_size((const void*)(idx_src + i * idx_size));
            if (data_need + dlen > data_space)
                break;
            data_need += dlen;
            ++max_can;
        }
    } else {
        const void **idx_arr = arg->v.d.idx;
        for (uint32_t i = 0; i < num && i < idx_space; ++i) {
            uint32_t dlen = rd->get_size(idx_arr[i]);
            if (data_need + dlen > data_space)
                break;
            data_need += dlen;
            ++max_can;
        }
    }
    *data_len = data_need;
    return max_can;
}

/**
 * @brief   write 和 writev 的公共写入流程
 * @param   rd              管理器指针
 * @param   producer_id     生产者 ID
 * @param   arg             写入参数（包含连续/分散模式和数据）
 * @param   strategy        策略标志
 * @param   timeout_or_tries策略参数（阻塞超时ms或重试次数，-1无限）
 * @param   pdropped        传入最少丢弃索引个数，返回实际丢弃数（可以为NULL）
 * @return  成功返回写入索引数，失败返回 -1
 * @note    处理顺序：先尝试阻塞/重试，若仍不足且允许 DROP，则丢弃旧数据并重试。
 *          非 COMPLETE 模式下允许部分写入。
 */
static int jringdata_write_common(jringdata_t *rd, int producer_id,
                                  struct jringdata_write_arg *arg,
                                  uint32_t strategy, int timeout_or_tries,
                                  uint32_t *pdropped)
{
    int complete = (strategy & JRINGDATA_COMPLETE) ? 1 : 0;
    int drop     = (strategy & JRINGDATA_DROP)     ? 1 : 0;
    int block    = (strategy & JRINGDATA_BLOCK)    ? 1 : 0;
    int retry    = (strategy & JRINGDATA_RETRY)    ? 1 : 0;

    const void *idx_ptr = NULL;
    const void *data_ptr = NULL;
    const void **idx_arr = NULL;
    const void **data_arr = NULL;

    uint32_t num = arg->num;
    uint32_t len = 0;

    /* 根据模式提取指针并计算总数据长度 */
    if (!arg->is_discrete) {
        idx_ptr = arg->v.c.idx;
        data_ptr = arg->v.c.data;
        len = calc_data_len_for_input_idx(rd, idx_ptr, num);
        if (len && !data_ptr) {
            return -1;
        }
    } else {
        idx_arr = arg->v.d.idx;
        data_arr = arg->v.d.data;
        len = calc_data_len_for_input_idxv(rd, idx_arr, num);
        if (len && !data_arr) {
            return -1;
        }
        for (uint32_t i = 0; i < num; ++i) {
            uint32_t dlen = rd->get_size(idx_arr[i]);
            if (dlen && !data_arr[i]) {
                return -1;
            }
        }
    }

    /* 计算最小需求：完全写入需 num 个索引和 len 字节数据，部分写入只需至少 1 个索引 */
    uint32_t need_idx  = complete ? num : 1;
    uint32_t need_data = complete ? len :
                         (arg->is_discrete ? rd->get_size(idx_arr[0]) : rd->get_size(idx_ptr));

    /* 总容量不足则永远无法写入成功 */
    if (need_idx > rd->idx_ctx.total_len || need_data > rd->data_ctx.total_len) {
        return -1;
    }

    uint32_t write_num = 0;
    uint32_t write_data = 0;
    uint32_t idx_space;
    uint32_t data_space;

redo:
    jthread_mutex_lock(&rd->mutex);
    ++rd->rw_count;

    /* 主循环：等待空间满足（阻塞/重试） */
    do {
        /* 正在销毁ringdata */
        if (rd->disable_rw) {
            goto err;
        }

        /* 校验生产者有效性 */
        if (rd->max_producers > 1 && !JRD_PROD_ACT(rd)[producer_id]) {
            goto err;
        }

        /* 惰性更新 */
        if (rd->min_read_stale)
            update_min_read_index(rd);

        /* 获取可写剩余空间 */
        idx_space = rd->idx_ctx.total_len - rd->idx_ctx.data_len;
        data_space = rd->data_ctx.total_len - rd->data_ctx.data_len;

        if (idx_space >= num && data_space >= len) {
            /* 空间满足，直接进入后续步骤 */
            write_num = num;
            write_data = len;
            break;
        } else if (idx_space < need_idx || data_space < need_data) {
            /* 快速检查：连最小需求都满足不了，直接进入等待/丢弃 */
            write_num = 0;
            write_data = 0;
        } else {
            /* 计算当前最大可写索引数及所需数据空间，判断是否满足写入条件 */
            write_num = calc_write_size(rd, arg, &write_data);
            if (write_num == num) {
                break;
            }
        }

        /* 没有重试或阻塞策略且没有丢数据机制 */
        if ((!block && !retry) || !timeout_or_tries) {
            break;
        }

        /* 未满足条件，尝试等待或重试 */
        if (block) {
            if (timeout_or_tries == -1) {
                /* 无限等待 */
                jthread_cond_wait(&rd->not_full, &rd->mutex);
            } else {
                /* 超时等待 */
                uint64_t t1, t2;
                t1 = jtime_monomsec_get();
                jthread_cond_mtimewait(&rd->not_full, &rd->mutex, timeout_or_tries);
                t2 = jtime_monomsec_get();
                timeout_or_tries = ((int)(t2 - t1) < timeout_or_tries) ?
                    (timeout_or_tries - (int)(t2 - t1)) : 0;
            }
        } else { /* retry */
            if (timeout_or_tries > 0)
                --timeout_or_tries;
            jthread_mutex_unlock(&rd->mutex);
            jthread_yield();
            jthread_mutex_lock(&rd->mutex);
        }

    } while (1);

    /* 如果空间仍不够，且允许丢弃旧数据，则执行丢弃 */
    if (write_num != num && drop) {
        if (rd->min_read_lock) {
            --rd->rw_count;
            jthread_mutex_unlock(&rd->mutex);
            jthread_yield();
            goto redo;
        }

        if (!complete && (num > rd->idx_ctx.total_len ||
            /* 非完整写且全部空间不足以写，直接丢弃全部缓冲数据 */
            len > rd->data_ctx.total_len)) {
            uint32_t dropped = rd->idx_ctx.data_len;
            drop_old_data(rd, &dropped, rd->idx_ctx.total_len,
                rd->data_ctx.total_len);
            if (pdropped)
                *pdropped = dropped;
            write_num = calc_write_size(rd, arg, &write_data);
        } else {
            /* 丢弃到可以完整写 */
            if (drop_old_data(rd, pdropped, num, len) < 0) {
                goto err;
            }
            write_num = num;
            write_data = len;
        }
    }

    /* 检查写入条件 */
    if (write_num < need_idx) {
        goto err;
    }


    /* ---- 执行数据复制 ---- */
    {
        /* 计算起始元素序号和字节偏移 */
        uint8_t *idx_buf = JRD_IDX_BUF(rd);
        uint8_t *data_buf = JRD_DATA_BUF(rd);

        uint32_t idx_size = rd->idx_ctx.unit_size;
        uint32_t idx_mask = rd->idx_ctx.total_len - 1;
        uint32_t idx_wpos = rd->idx_ctx.write_index & idx_mask;
        uint32_t data_mask = rd->data_ctx.total_len - 1;
        uint32_t data_wpos = rd->data_ctx.write_index & data_mask;

        /* 单生产者优化：数据拷贝期间临时解锁 */
        if (rd->max_producers == 1)
            jthread_mutex_unlock(&rd->mutex);

        if (!arg->is_discrete) {
            /* 写入索引 */
            const uint8_t *idx_src = (const uint8_t*)idx_ptr;
            uint32_t idx_tail = rd->idx_ctx.total_len - idx_wpos;
            uint32_t idx_first = (write_num <= idx_tail) ? write_num : idx_tail;
            memcpy(idx_buf + idx_wpos * idx_size, idx_src, idx_first * idx_size);
            if (write_num > idx_tail) {
                memcpy(idx_buf, idx_src + idx_first * idx_size, (write_num - idx_first) * idx_size);
            }

            /* 写入数据 */
            if (write_data) {
                const uint8_t *data_src = (const uint8_t*)data_ptr;
                uint32_t data_tail = rd->data_ctx.total_len - data_wpos;
                uint32_t data_first = (write_data <= data_tail) ? write_data : data_tail;
                memcpy(data_buf + data_wpos, data_src, data_first);
                if (write_data > data_tail) {
                    memcpy(data_buf, data_src + data_first, write_data - data_first);
                }
            }
        } else {
            for (uint32_t i = 0; i < write_num; ++i) {
                /* 写入索引 */
                uint32_t idx_pos = idx_wpos & idx_mask;
                memcpy(idx_buf + idx_pos * idx_size, idx_arr[i], idx_size);
                ++idx_wpos;
                /* 写入数据 */
                uint32_t dlen = rd->get_size(idx_arr[i]);
                if (dlen) {
                    const uint8_t *data_src = (const uint8_t*)data_arr[i];
                    uint32_t data_pos = data_wpos & data_mask;
                    uint32_t data_tail = rd->data_ctx.total_len - data_pos;
                    uint32_t data_first = (dlen <= data_tail) ? dlen : data_tail;
                    memcpy(data_buf + data_pos, data_src, data_first);
                    if (dlen > data_tail) {
                        memcpy(data_buf, data_src + data_first, dlen - data_first);
                    }
                    data_wpos += dlen;
                }
            }
        }

        if (rd->max_producers == 1)
            jthread_mutex_lock(&rd->mutex);

        /* 更新写指针 */
        rd->idx_ctx.write_index += write_num;
        rd->idx_ctx.data_len += write_num;
        rd->data_ctx.write_index += write_data;
        rd->data_ctx.data_len += write_data;
    }

    if (rd->idx_ctx.data_len >= rd->wake_num)
        jthread_cond_broadcast(&rd->not_empty);

    --rd->rw_count;
    jthread_mutex_unlock(&rd->mutex);
    return (int)write_num;

err:
    --rd->rw_count;
    jthread_mutex_unlock(&rd->mutex);
    return -1;
}

/*----------------------------------------------------------------------------
  read_common：公共读取流程（所有逻辑内联）
----------------------------------------------------------------------------*/

/**
 * @brief   计算当前消费者可读取的索引数及对应数据量
 */
static uint32_t calc_read_size(jringdata_t *rd, uint32_t c_idx, uint32_t avail_idx,
    struct jringdata_read_arg *arg, uint32_t *data_len)
{
    uint32_t max_can = 0;
    uint32_t data_need = 0;
    uint32_t num = arg->num;

    uint8_t *idx_buf = JRD_IDX_BUF(rd);
    uint32_t idx_size = rd->idx_ctx.unit_size;
    uint32_t idx_mask = rd->idx_ctx.total_len - 1;
    uint32_t rpos = c_idx & idx_mask;

    if (!arg->is_discrete) {
        uint32_t user_len = arg->v.c.len;
        for (uint32_t i = 0; i < num && i < avail_idx; ++i) {
            void *entry = (void *)(idx_buf + rpos * idx_size);
            uint32_t dlen = rd->get_size(entry);
            if (data_need + dlen > user_len)
                break;
            data_need += dlen;
            ++max_can;
            rpos = (rpos + 1) & idx_mask;
        }
    } else {
        uint32_t *lens = arg->v.d.len;
        for (uint32_t i = 0; i < num && i < avail_idx; ++i) {
            void *entry = idx_buf + rpos * idx_size;
            uint32_t dlen = rd->get_size(entry);
            if (dlen > lens[i])
                break;
            data_need += dlen;
            ++max_can;
            rpos = (rpos + 1) & idx_mask;
        }
    }

    *data_len = data_need;
    return max_can;
}

/**
 * @brief   read 和 readv 的公共读取流程
 * @param   rd              管理器指针
 * @param   consumer_id     消费者 ID
 * @param   arg             读取参数（包含连续/分散模式和数据缓冲区）
 * @param   total_num       输出读取前的消费者可用索引总数（可NULL）
 * @param   total_size      输出读取前的消费者可用数据总字节数（可NULL）
 * @param   strategy        策略标志
 * @param   timeout_or_tries策略参数（阻塞超时ms或重试次数，-1无限）
 * @return  成功返回实际读取的索引数，失败返回 -1
 * @note    处理 COMPLETE、BLOCK、RETRY 策略。
 *          共享读模式下拷贝数据期间会设置 min_read_lock 以防止数据被覆盖。
 */
static int jringdata_read_common(jringdata_t *rd, int consumer_id,
                                 struct jringdata_read_arg *arg,
                                 uint32_t *total_num, uint32_t *total_size,
                                 uint32_t strategy, int timeout_or_tries)
{
    int complete = (strategy & JRINGDATA_COMPLETE) ? 1 : 0;
    int block    = (strategy & JRINGDATA_BLOCK)    ? 1 : 0;
    int retry    = (strategy & JRINGDATA_RETRY)    ? 1 : 0;

    /* 最小需求量：完全读取需 num 个索引，部分读取至少需 1 个索引 */
    uint32_t num = arg->num;
    uint32_t need_idx = complete ? num : 1;

    uint32_t read_num = 0;
    uint32_t read_data = 0;
    uint32_t c_idx = 0, c_data = 0;
    uint32_t avail_idx = 0, avail_data = 0;
    int shared_mode = 0;

    jthread_mutex_lock(&rd->mutex);
    ++rd->rw_count;

    /* 单消费者固定 ID 为 0，无需校验 active 数组 */
    if (rd->max_consumers == 1) {
        consumer_id = 0;
    } else if (consumer_id < 0 || (uint32_t)consumer_id >= rd->max_consumers) {
        goto err;
    }

    shared_mode = (rd->max_consumers == 1 || rd->read_mode == JRINGDATA_READ_SHARED);

    /* 主循环：等待数据可用 */
    do {
        /* 正在销毁ringbuf */
        if (rd->disable_rw)
            goto err;

        /* 校验消费者有效性 */
        if (rd->max_consumers > 1 && !JRD_CONS_ACT(rd)[consumer_id])
            goto err;

        /* 获取当前消费者的读位置及可用数据量 */
        if (shared_mode) {
            c_idx = rd->idx_ctx.min_read_index;
            c_data = rd->data_ctx.min_read_index;
            avail_idx = rd->idx_ctx.data_len;
            avail_data = rd->data_ctx.data_len;
        } else {
            uint32_t *ridx = JRD_RIDX_ARR(rd);
            uint32_t *rdata = JRD_RDATA_ARR(rd);
            c_idx = ridx[consumer_id];
            c_data = rdata[consumer_id];
            avail_idx = rd->idx_ctx.write_index - c_idx;
            avail_data = rd->data_ctx.write_index - c_data;
        }

        /* 检查数据缓冲区是否足够 */
        if (avail_idx > 0) {
            read_num = calc_read_size(rd, c_idx, avail_idx, arg, &read_data);
            if ((read_num == num) || (!complete && read_num > 0)) {
                break;
            } else {
                /* 循环缓冲区索引足够，但调用者提供的数据 buf 无法容纳数据，永久性错误 */
                if (avail_idx >= need_idx || read_num < avail_idx) {
                    goto err;
                }
                /* 完全模式且不足 num 个索引，或部分模式但 0 个索引，继续等待 */
            }
        }

        /* 没有重试或阻塞策略，退出 */
        if ((!block && !retry) || !timeout_or_tries) {
            goto err;
        }

        /* 等待或重试 */
        if (block) {
            if (timeout_or_tries == -1) {
                jthread_cond_wait(&rd->not_empty, &rd->mutex);
            } else {
                uint64_t t1, t2;
                t1 = jtime_monomsec_get();
                jthread_cond_mtimewait(&rd->not_empty, &rd->mutex, timeout_or_tries);
                t2 = jtime_monomsec_get();
                timeout_or_tries = ((int)(t2 - t1) < timeout_or_tries) ?
                                   (timeout_or_tries - (int)(t2 - t1)) : 0;
            }
        } else { /* retry */
            if (timeout_or_tries > 0)
                --timeout_or_tries;
            jthread_mutex_unlock(&rd->mutex);
            jthread_yield();
            jthread_mutex_lock(&rd->mutex);
        }
    } while (1);

    /* 最终校验：至少满足最小需求 */
    if (read_num < need_idx) {
        goto err;
    }

    /* 设定输出参数：读取前的可用总量（包含本次读取的数据） */
    if (total_num)
        *total_num = avail_idx;
    if (total_size)
        *total_size = avail_data;


    /* ---- 执行数据复制 ---- */
    {
        /* 计算起始元素序号和字节偏移 */
        uint8_t *idx_buf = JRD_IDX_BUF(rd);
        uint8_t *data_buf = JRD_DATA_BUF(rd);

        uint32_t idx_size = rd->idx_ctx.unit_size;
        uint32_t idx_mask = rd->idx_ctx.total_len - 1;
        uint32_t data_mask = rd->data_ctx.total_len - 1;

        /* 单用户模式下，数据拷贝前临时解锁（防止长时间持锁） */
        if (rd->max_consumers == 1) {
            rd->min_read_lock = 1;
            jthread_mutex_unlock(&rd->mutex);
        }

        if (!arg->is_discrete) {
            /* 读取索引 */
            void *idx_dst = arg->v.c.idx;
            void *data_dst = arg->v.c.data;
            uint32_t rpos_idx = c_idx & idx_mask;
            uint32_t idx_tail = rd->idx_ctx.total_len - rpos_idx;
            uint32_t idx_first = (read_num <= idx_tail) ? read_num : idx_tail;
            memcpy(idx_dst, idx_buf + rpos_idx * idx_size, idx_first * idx_size);
            if (read_num > idx_tail) {
                memcpy((uint8_t*)idx_dst + idx_first * idx_size,
                    idx_buf, (read_num - idx_first) * idx_size);
            }
            /* 读取数据 */
            if (read_data) {
                uint32_t rpos_data = c_data & data_mask;
                uint32_t data_tail = rd->data_ctx.total_len - rpos_data;
                uint32_t data_first = (read_data <= data_tail) ? read_data : data_tail;
                memcpy(data_dst, data_buf + rpos_data, data_first);
                if (read_data > data_tail) {
                    memcpy((uint8_t*)data_dst + data_first, data_buf, read_data - data_first);
                }
            }
        } else {
            void **idx_dst_arr = arg->v.d.idx;
            void **data_dst_arr = arg->v.d.data;

            uint32_t idx_pos = c_idx & idx_mask;
            uint32_t data_pos = c_data & data_mask;
            for (uint32_t i = 0; i < read_num; ++i) {
                /* 读取索引 */
                memcpy(idx_dst_arr[i], idx_buf + idx_pos * idx_size, idx_size);
                idx_pos = (idx_pos + 1) & idx_mask;

                /* 读取数据 */
                uint32_t dlen = rd->get_size(idx_dst_arr[i]);
                if (dlen) {
                    uint32_t data_tail = rd->data_ctx.total_len - data_pos;
                    uint32_t data_first = (dlen <= data_tail) ? dlen : data_tail;
                    memcpy(data_dst_arr[i], data_buf + data_pos, data_first);
                    if (dlen > data_tail) {
                        memcpy((uint8_t*)data_dst_arr[i] + data_first, data_buf, dlen - data_first);
                    }
                    data_pos = (data_pos + dlen) & data_mask;
                }
            }
        }

        if (rd->max_consumers == 1) {
            jthread_mutex_lock(&rd->mutex);
            rd->min_read_lock = 0;
        }

        /* 更新读位置 */
        if (shared_mode) {
            rd->idx_ctx.min_read_index += read_num;
            rd->idx_ctx.data_len -= read_num;
            rd->data_ctx.min_read_index += read_data;
            rd->data_ctx.data_len -= read_data;
        } else {
            uint32_t *ridx = JRD_RIDX_ARR(rd);
            uint32_t *rdata = JRD_RDATA_ARR(rd);
            ridx[consumer_id] += read_num;
            rdata[consumer_id] += read_data;
            if (!rd->min_read_stale && c_idx == rd->idx_ctx.min_read_index) {
                rd->min_read_stale = 1;
            }
            rd->idx_ctx.data_len = rd->idx_ctx.write_index - rd->idx_ctx.min_read_index;
            rd->data_ctx.data_len = rd->data_ctx.write_index - rd->data_ctx.min_read_index;
        }
    }

    jthread_cond_broadcast(&rd->not_full);
    --rd->rw_count;
    jthread_mutex_unlock(&rd->mutex);
    return (int)read_num;

err:
    if (total_num)
        *total_num = 0;
    if (total_size)
        *total_size = 0;
    --rd->rw_count;
    jthread_mutex_unlock(&rd->mutex);
    return -1;
}

/*----------------------------------------------------------------------------
  对外接口：Write / Writev
----------------------------------------------------------------------------*/

int jringdata_write(jringdata_t *rd, int producer_id, const void *idx, uint32_t num,
                    const void *data, uint32_t strategy, int arg, uint32_t *pdropped)
{
    if (!rd || !idx || !num)
        return -1;

    struct jringdata_write_arg warg;
    warg.is_discrete = 0;
    warg.num = num;
    warg.v.c.idx = idx;
    warg.v.c.data = data;

    return jringdata_write_common(rd, producer_id, &warg, strategy, arg, pdropped);
}

int jringdata_writev(jringdata_t *rd, int producer_id, const void **idx, uint32_t num,
                     const void **data, uint32_t strategy, int arg, uint32_t *pdropped)
{
    if (!rd || !idx || !num)
        return -1;

    struct jringdata_write_arg warg;
    warg.is_discrete = 1;
    warg.num = num;
    warg.v.d.idx = idx;
    warg.v.d.data = data;

    return jringdata_write_common(rd, producer_id, &warg, strategy, arg, pdropped);
}

/*----------------------------------------------------------------------------
  对外接口：Read / Readv
----------------------------------------------------------------------------*/

int jringdata_read(jringdata_t *rd, int consumer_id, void *idx, uint32_t num,
                   void *data, uint32_t len, uint32_t* total_num, uint32_t* total_size,
                   uint32_t strategy, int arg)
{
    if (!rd || !idx || !num)
        return -1;

    struct jringdata_read_arg rarg;
    rarg.is_discrete = 0;
    rarg.num = num;
    rarg.v.c.idx = idx;
    rarg.v.c.data = data;
    rarg.v.c.len = len;

    return jringdata_read_common(rd, consumer_id, &rarg, total_num, total_size, strategy, arg);
}

int jringdata_readv(jringdata_t *rd, int consumer_id, void **idx, uint32_t num,
                    void **data, uint32_t *len, uint32_t* total_num, uint32_t* total_size,
                    uint32_t strategy, int arg)
{
    if (!rd || !idx || !num)
        return -1;

    struct jringdata_read_arg rarg;
    rarg.is_discrete = 1;
    rarg.num = num;
    rarg.v.d.idx = idx;
    rarg.v.d.data = data;
    rarg.v.d.len = len;

    return jringdata_read_common(rd, consumer_id, &rarg, total_num, total_size, strategy, arg);
}

/*----------------------------------------------------------------------------
  生产者管理
----------------------------------------------------------------------------*/

/**
 * @brief   添加一个生产者
 */
int jringdata_add_producer(jringdata_t *rd)
{
    if (!rd)
        return -1;
    if (rd->max_producers == 1)
        return 0;

    jthread_mutex_lock(&rd->mutex);
    uint8_t *prod = JRD_PROD_ACT(rd);
    uint32_t i = rd->cur_producers;
    if (rd->cur_producers < rd->max_producers) {
        if (!prod[i]) {
            goto end;
        } else {
            for (i = 0; i < rd->max_producers; ++i) {
                if (!prod[i])
                    goto end;
            }
        }
    }
    jthread_mutex_unlock(&rd->mutex);
    return -1;
end:
    prod[i] = 1;
    ++rd->cur_producers;
    jthread_mutex_unlock(&rd->mutex);
    return (int)i;
}

/**
 * @brief   移除一个生产者，producer_id 为 -1 时移除所有
 */
int jringdata_del_producer(jringdata_t *rd, int producer_id)
{
    if (!rd)
        return -1;
    if (rd->max_producers == 1) {
        return (producer_id == 0 || producer_id == -1) ? 0 : -1;
    }

    jthread_mutex_lock(&rd->mutex);
    uint8_t *prod = JRD_PROD_ACT(rd);
    if (producer_id == -1) {
        for (uint32_t i = 0; i < rd->max_producers; ++i)
            prod[i] = 0;
        rd->cur_producers = 0;
        goto end;
    }

    if (producer_id < 0 || (uint32_t)producer_id >= rd->max_producers || !prod[producer_id]) {
        jthread_mutex_unlock(&rd->mutex);
        return -1;
    }
    prod[producer_id] = 0;
    --rd->cur_producers;

end:
    jthread_cond_broadcast(&rd->not_full);
    jthread_mutex_unlock(&rd->mutex);
    return 0;
}

/*----------------------------------------------------------------------------
  消费者管理
----------------------------------------------------------------------------*/

/**
 * @brief   添加一个消费者，返回消费者 ID
 */
int jringdata_add_consumer(jringdata_t *rd, int use_ridx)
{
    if (!rd)
        return -1;
    if (rd->max_consumers == 1)
        return 0;

    jthread_mutex_lock(&rd->mutex);
    uint8_t *act = JRD_CONS_ACT(rd);
    uint32_t i = rd->cur_consumers;
    if (rd->cur_consumers < rd->max_consumers) {
        if (!act[i]) {
            goto end;
        } else {
            for (i = 0; i < rd->max_consumers; ++i) {
                if (!act[i])
                    goto end;
            }
        }
    }
    jthread_mutex_unlock(&rd->mutex);
    return -1;
end:
    act[i] = 1;
    ++rd->cur_consumers;
    if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
        uint32_t *ridx = JRD_RIDX_ARR(rd);
        uint32_t *rdata = JRD_RDATA_ARR(rd);
        if (use_ridx) {
            ridx[i] = rd->idx_ctx.min_read_index;
            rdata[i] = rd->data_ctx.min_read_index;
        } else {
            ridx[i] = rd->idx_ctx.write_index;
            rdata[i] = rd->data_ctx.write_index;
        }
    }
    jthread_mutex_unlock(&rd->mutex);
    return (int)i;
}

/**
 * @brief   移除一个消费者，consumer_id 为 -1 时移除所有
 */
int jringdata_del_consumer(jringdata_t *rd, int consumer_id)
{
    if (!rd)
        return -1;
    if (rd->max_consumers == 1) {
        if (consumer_id == 0 || consumer_id == -1) {
            jthread_mutex_lock(&rd->mutex);
            rd->idx_ctx.min_read_index = rd->idx_ctx.write_index;
            rd->idx_ctx.data_len = 0;
            rd->data_ctx.min_read_index = rd->data_ctx.write_index;
            rd->data_ctx.data_len = 0;
            jthread_cond_broadcast(&rd->not_full);
            jthread_mutex_unlock(&rd->mutex);
            return 0;
        }
        return -1;
    }

    jthread_mutex_lock(&rd->mutex);
    uint8_t *act = JRD_CONS_ACT(rd);
    if (consumer_id == -1) {
        for (uint32_t i = 0; i < rd->max_consumers; ++i)
            act[i] = 0;
        rd->cur_consumers = 0;
        goto end;
    }

    if (consumer_id < 0 || (uint32_t)consumer_id >= rd->max_consumers || !act[consumer_id]) {
        jthread_mutex_unlock(&rd->mutex);
        return -1;
    }
    act[consumer_id] = 0;
    --rd->cur_consumers;

end:
    update_min_read_index(rd);
    jthread_cond_broadcast(&rd->not_full);
    jthread_cond_broadcast(&rd->not_empty);
    jthread_mutex_unlock(&rd->mutex);
    return 0;
}

/**
 * @brief   丢弃消费者的部分或全部未读数据
 */
int jringdata_drop_data(jringdata_t *rd, int consumer_id, uint32_t dropped)
{
    if (!rd)
        return -1;

    jthread_mutex_lock(&rd->mutex);
    while (rd->min_read_lock) {
        jthread_mutex_unlock(&rd->mutex);
        jthread_yield();
        jthread_mutex_lock(&rd->mutex);
    }

    uint32_t idx_avail, drop_idx, drop_data;
    uint8_t *act = NULL;

    if (rd->max_consumers == 1) {
        if (consumer_id == 0 || consumer_id == -1) {
            goto end2;
        }
        jthread_mutex_unlock(&rd->mutex);
        return -1;
    }

    act = JRD_CONS_ACT(rd);
    if (consumer_id == -1) {
        if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
            uint32_t *ridx = JRD_RIDX_ARR(rd);
            uint32_t *rdata = JRD_RDATA_ARR(rd);
            for (uint32_t i = 0, j = 0; i < rd->max_consumers && j < rd->cur_consumers; ++i) {
                if (act[i]) {
                    idx_avail = rd->idx_ctx.write_index - ridx[i];
                    drop_idx = (dropped == 0 || dropped > idx_avail) ? idx_avail : dropped;
                    // 计算对应的数据长度
                    uint32_t drop_data_len = calc_data_len_for_idx_range(rd, ridx[i], drop_idx);
                    ridx[i] += drop_idx;
                    rdata[i] += drop_data_len;
                    ++j;
                }
            }
            update_min_read_index(rd);
            goto end1;
        } else {
            goto end2;
        }
    }

    if (consumer_id < 0 || (uint32_t)consumer_id >= rd->max_consumers || !act[consumer_id]) {
        jthread_mutex_unlock(&rd->mutex);
        return -1;
    }

    if (rd->read_mode == JRINGDATA_READ_EXCLUSIVE) {
        uint32_t *ridx = JRD_RIDX_ARR(rd);
        uint32_t *rdata = JRD_RDATA_ARR(rd);
        idx_avail = rd->idx_ctx.write_index - ridx[consumer_id];
        drop_idx = (dropped == 0 || dropped > idx_avail) ? idx_avail : dropped;
        drop_data = calc_data_len_for_idx_range(rd, ridx[consumer_id], drop_idx);
        ridx[consumer_id] += drop_idx;
        rdata[consumer_id] += drop_data;
        update_min_read_index(rd);
        goto end1;
    } else {
        goto end2;
    }

end2:
    // 共享模式：直接丢弃全局数据
    idx_avail = rd->idx_ctx.data_len;
    drop_idx = (dropped == 0 || dropped > idx_avail) ? idx_avail : dropped;
    drop_data = calc_data_len_for_idx_range(rd, rd->idx_ctx.min_read_index, drop_idx);
    rd->idx_ctx.min_read_index += drop_idx;
    rd->idx_ctx.data_len -= drop_idx;
    rd->data_ctx.min_read_index += drop_data;
    rd->data_ctx.data_len -= drop_data;

end1:
    jthread_cond_broadcast(&rd->not_full);
    jthread_mutex_unlock(&rd->mutex);
    return 0;
}

