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
#include "jringbuf.h"
#include "jthread.h"
#include "jheap.h"

/*----------------------------------------------------------------------------
  环形缓冲区主结构（柔性数组布局）
----------------------------------------------------------------------------*/

/**
 * @brief 环形缓冲区管理器（按元素管理）
 *
 * 尾部连续内存布局（由偏移量索引）：
 *   - buffer[byte_capacity]              数据缓冲区
 *   - read_index[max_consumers]          消费者读位置数组 (uint32_t)，仅 max_consumers>1且JRINGBUF_READ_EXCLUSIVE
 *   - producer[max_producers]            生产者有效性数组 (uint8_t)，仅 max_producers>1
 *   - consumer[max_consumers]            消费者有效性数组 (uint8_t)，仅 max_consumers>1
 */
struct jringbuf {
    uint32_t        max_producers;      // 最大生产者数量
    uint32_t        cur_producers;      // 当前生产者数量
    uint32_t        max_consumers;      // 最大消费者数量
    uint32_t        cur_consumers;      // 当前消费者数量
    uint32_t        hold_num;           // 历史窗口大小（元素个数）
    uint32_t        wake_num;           // 唤醒窗口大小（元素个数）
    enum jringbuf_read_mode read_mode;  // 读指针管理模式
    uint8_t         disable_rw;         // 是否禁止读写
    uint8_t         min_read_stale;     // 1 表示 min_read_index 需要重新计算（惰性）
    uint8_t         min_read_lock;      // 1 表示正在读取数据，不能改变 min_read_index (单消费者)
    uint32_t        rw_count;           // 正在读写的生产者或消费者数目
    uint32_t        total_size;         // 数据区总大小（字节）
    uint32_t        producer_offset;    // 生产者有效性数组偏移（字节）
    uint32_t        consumer_offset;    // 消费者有效性数组偏移（字节）

    uint32_t        capacity;           // 缓冲区元素总个数（2的幂）
    uint32_t        unit_size;          // 单个元素大小（字节）
    uint32_t        data_len;           // 有效数据元素个数 = write_index - min_read_index
    uint32_t        write_index;        // 绝对写位置（元素个数），单调递增，利用自然溢出
    uint32_t        min_read_index;     // 活跃消费者中最小的读位置（元素个数）
    uint32_t        buf_offset;         // 数据缓冲区起始偏移（字节）
    uint32_t        read_index_offset;  // 消费者读索引数组偏移（字节）

    jthread_mutex_t mutex;              // 全局互斥锁
    jthread_cond_t  not_empty;          // 数据可用条件变量（单调时钟）
    jthread_cond_t  not_full;           // 空间可用条件变量（单调时钟）

    uint8_t         data[];             // 柔性数组起始
};

/*----------------------------------------------------------------------------
  内部宏：通过偏移访问各数组
----------------------------------------------------------------------------*/

#define JRB_BUF(rb)       ((uint8_t*)((rb)->data + (rb)->buf_offset))            // 数据缓冲区
#define JRB_CONS_IDX(rb)  ((uint32_t*)((rb)->data + (rb)->read_index_offset))    // 消费者读索引数组（元素位置）
#define JRB_PROD_ACT(rb)  ((uint8_t*)((rb)->data + (rb)->producer_offset))       // 生产者活跃数组
#define JRB_CONS_ACT(rb)  ((uint8_t*)((rb)->data + (rb)->consumer_offset))       // 消费者活跃数组

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
 * @brief   遍历活跃消费者并重新计算 min_read_index 及 data_len（均以元素为单位）
 */
static void update_min_read_index(jringbuf_t *rb)
{
    uint32_t old_min = rb->min_read_index;
    uint32_t min_idx = rb->write_index;   /* 初始设为最远位置 */

    /* 1. 基于活跃消费者计算“最慢读指针” */
    if (rb->read_mode == JRINGBUF_READ_EXCLUSIVE) {
        uint32_t *idx = JRB_CONS_IDX(rb);
        uint8_t  *act = JRB_CONS_ACT(rb);

        for (uint32_t i = 0, j = 0; i < rb->max_consumers && j < rb->cur_consumers; ++i) {
            if (act[i]) {
                ++j;
                /* 距离大的更旧（利用无符号减法自然溢出） */
                if ((rb->write_index - idx[i]) > (rb->write_index - min_idx)) {
                    min_idx = idx[i];
                }
            }
        }
    }

    /* 2. 如果没有活跃消费者，min_idx 保持为 write_index，此时只受 hold_num 约束 */
    if (rb->hold_num > 0) {
        uint32_t hold_min = rb->write_index - rb->hold_num;

        /* 目标下限是 hold_min，但绝不能回退到 old_min 之前 */
        uint32_t target_min = hold_min;
        if ((rb->write_index - old_min) < rb->hold_num) {
            /* 之前已经比 hold_min 更靠后了，就从 old_min 往前推进 */
            target_min = old_min;
        }

        /* min_idx 取 target_min 和 min_idx 中的较小值（即更靠后） */
        if ((rb->write_index - target_min) > (rb->write_index - min_idx)) {
            min_idx = target_min;
        }
    }

    /* 3. 更新全局状态 */
    rb->min_read_index = min_idx;
    rb->data_len       = rb->write_index - min_idx;
    rb->min_read_stale = 0;
}

/*----------------------------------------------------------------------------
  核心接口实现
----------------------------------------------------------------------------*/

/**
 * @brief   创建环形缓冲区
 */
jringbuf_t* jringbuf_init(const jringbuf_cfg_t *cfg)
{
    if (cfg->capacity == 0 || cfg->unit_size == 0 || cfg->max_producers < 1 || cfg->max_consumers < 1)
        return NULL;

    uint32_t elem_capacity = next_pow2(cfg->capacity);
    uint32_t byte_capacity = elem_capacity * cfg->unit_size;
    uint32_t cons_idx_size = (cfg->max_consumers > 1 && cfg->read_mode == JRINGBUF_READ_EXCLUSIVE) ?  cfg->max_consumers * sizeof(uint32_t) : 0;
    uint32_t prod_act_size = (cfg->max_producers > 1) ? cfg->max_producers * sizeof(uint8_t)  : 0;
    uint32_t cons_act_size = (cfg->max_consumers > 1) ? cfg->max_consumers * sizeof(uint8_t)  : 0;

    size_t total = sizeof(jringbuf_t) + byte_capacity + cons_idx_size + prod_act_size + cons_act_size;

    jringbuf_t *rb = (jringbuf_t*)jheap_malloc(total);
    if (!rb)
        return NULL;

    memset(rb, 0, sizeof(jringbuf_t));
    rb->max_producers  = cfg->max_producers;
    rb->cur_producers  = 0;
    rb->max_consumers  = cfg->max_consumers;
    rb->cur_consumers  = 0;
    rb->hold_num       = cfg->hold_num;
    rb->wake_num       = cfg->wake_num;
    rb->read_mode      = (cfg->max_consumers == 1) ? JRINGBUF_READ_SHARED : cfg->read_mode;
    rb->disable_rw     = 0;
    rb->rw_count       = 0;

    rb->capacity       = elem_capacity;          // 元素个数
    rb->unit_size      = cfg->unit_size;
    rb->data_len       = 0;
    rb->write_index    = 0;
    rb->min_read_index = 0;
    rb->min_read_stale = 0;
    rb->min_read_lock  = 0;

    uint32_t off = 0;
    rb->buf_offset        = off; off += byte_capacity;
    rb->read_index_offset = cons_idx_size ? off : 0; off += cons_idx_size;
    rb->producer_offset   = prod_act_size ? off : 0; off += prod_act_size;
    rb->consumer_offset   = cons_act_size ? off : 0; off += cons_act_size;
    rb->total_size        = off;
    if (off > byte_capacity) {
        memset(rb->data + byte_capacity, 0, off - byte_capacity);
    }

    jthread_mutex_init(&rb->mutex);
    jthread_cond_init(&rb->not_empty, 1);     /* 单调时钟 */
    jthread_cond_init(&rb->not_full,  1);

    return rb;
}

/**
 * @brief   销毁环形缓冲区
 */
void jringbuf_uninit(jringbuf_t *rb)
{
    if (!rb)
        return;

    jringbuf_stop(rb);
    jthread_mutex_destroy(&rb->mutex);
    jthread_cond_destroy(&rb->not_empty);
    jthread_cond_destroy(&rb->not_full);
    jheap_free(rb);
}

/**
 * @brief   允许读写
 */
int jringbuf_start(jringbuf_t *rb)
{
    if (!rb)
        return -1;

    jthread_mutex_lock(&rb->mutex);
    rb->disable_rw = 0;
    jthread_mutex_unlock(&rb->mutex);
    return 0;
}

/**
 * @brief   停止并禁止读写
 */
void jringbuf_stop(jringbuf_t *rb)
{
    if (!rb)
        return;

    /* 唤醒所有等待线程，让它们自己退出 */
    jthread_mutex_lock(&rb->mutex);
    rb->disable_rw = 1;
    while (rb->rw_count) {
        jthread_cond_broadcast(&rb->not_empty);
        jthread_cond_broadcast(&rb->not_full);
        jthread_mutex_unlock(&rb->mutex);
        jthread_yield();
        jthread_mutex_lock(&rb->mutex);
    }
    jthread_mutex_unlock(&rb->mutex);
}

/**
 * @brief   获取有效元素个数或指定消费者的未读元素个数
 */
uint32_t jringbuf_size(jringbuf_t *rb, int consumer_id)
{
    if (!rb)
        return 0;

    jthread_mutex_lock(&rb->mutex);

    if (rb->min_read_stale && rb->read_mode == JRINGBUF_READ_EXCLUSIVE && consumer_id == -1) {
        update_min_read_index(rb);
    }

    uint32_t sz = 0;
    if (consumer_id == -1) {
        sz = rb->data_len;
    } else if (rb->max_consumers == 1) {
        sz = rb->data_len;
    } else if (rb->read_mode == JRINGBUF_READ_SHARED) {
        sz = rb->data_len;
    } else {
        if (consumer_id >= 0 && (uint32_t)consumer_id < rb->max_consumers) {
            if (JRB_CONS_ACT(rb)[consumer_id]) {
                sz = rb->write_index - JRB_CONS_IDX(rb)[consumer_id];
            }
        }
    }

    jthread_mutex_unlock(&rb->mutex);
    return sz;
}

/**
 * @brief   获取缓冲区容量（元素个数）
 */
uint32_t jringbuf_capacity(jringbuf_t *rb)
{
    return rb ? rb->capacity : 0;
}

/**
 * @brief   获取成员数量
 */
int jringbuf_members(jringbuf_t *rb, uint32_t *producers, uint32_t *consumers)
{
    if (!rb)
        return -1;
    if (!producers && !consumers)
        return -1;

    jthread_mutex_lock(&rb->mutex);
    if (producers)
        *producers = rb->max_producers > 1 ? rb->cur_producers : 1;
    if (consumers)
        *consumers = rb->max_consumers > 1 ? rb->cur_consumers : 1;
    jthread_mutex_unlock(&rb->mutex);
    return 0;
}

/**
 * @brief   向缓冲区写入数据
 */
int jringbuf_write(jringbuf_t *rb, int producer_id, const void *data, uint32_t len,
                   uint32_t strategy, int arg, uint32_t *pdropped)
{
    if (!rb || !data || !len)
        return -1;

    int complete = (strategy & JRINGBUF_COMPLETE) ? 1 : 0;
    int drop     = (strategy & JRINGBUF_DROP)     ? 1 : 0;
    int block    = (strategy & JRINGBUF_BLOCK)    ? 1 : 0;
    int retry    = (strategy & JRINGBUF_RETRY)    ? 1 : 0;
    uint32_t dropped = pdropped ? *pdropped : 0;
    uint32_t space = 0;                 /* 剩余元素空间 */
    uint32_t need = complete ? len : 1; /* 最少需要元素个数 */
    uint32_t drop_need, drop_amount;
    uint32_t to_write;

redo:
    jthread_mutex_lock(&rb->mutex);
    ++rb->rw_count;

    if (need > rb->capacity) {
        goto err;
    }

    do {
        /* 正在销毁ringbuf */
        if (rb->disable_rw) {
            goto err;
        }

        /* 校验生产者有效性 */
        if (rb->max_producers > 1 && !JRB_PROD_ACT(rb)[producer_id]) {
            goto err;
        }

        /* 惰性更新 */
        if (rb->min_read_stale)
            update_min_read_index(rb);

        /* 获取可写剩余元素空间 */
        space = rb->capacity - rb->data_len;

        /* 满足 len，直接跳出 */
        if (space >= len) {
            break;
        }

        /* 没有重试或阻塞策略且没有丢数据机制 */
        if ((!block && !retry) || !arg) {
            break;
        }

        /* 未满足条件，尝试等待或重试 */
        if (block) {
            if (arg == -1) {
                /* 无限等待 */
                jthread_cond_wait(&rb->not_full, &rb->mutex);
            } else {
                /* 超时等待 */
                uint64_t t1, t2;
                t1 = jtime_monomsec_get();
                jthread_cond_mtimewait(&rb->not_full, &rb->mutex, arg);
                t2 = jtime_monomsec_get();
                arg = ((int)(t2 - t1) < arg) ? (arg - (int)(t2 - t1)) : 0;
            }
        } else { /* retry */
            if (arg > 0) {
                --arg;
            }
            jthread_mutex_unlock(&rb->mutex);
            jthread_yield();
            jthread_mutex_lock(&rb->mutex);
        }
    } while (1);

    /* 如果空间仍不够，且允许丢弃旧数据，则执行丢弃 */
    if (space < len && drop) {
        if (rb->min_read_lock) {
            --rb->rw_count;
            jthread_mutex_unlock(&rb->mutex);
            jthread_yield();
            goto redo;
        }

        drop_need = len - space;               // 至少需要丢弃的元素个数
        drop_amount = (dropped < drop_need || rb->data_len < drop_need) ? drop_need :
                               (dropped < rb->data_len ? dropped : rb->data_len);

        /* 非完全写可以对齐到小于drop_need */
        if (drop_amount > rb->data_len && !complete) {
            drop_amount = rb->data_len;
        }

        if (drop_amount <= rb->data_len) {
            uint32_t new_min = rb->min_read_index + drop_amount;
            if (pdropped)
                *pdropped = drop_amount;

            /* 移动消费者读位置，避免非法读 */
            if (rb->read_mode == JRINGBUF_READ_EXCLUSIVE && rb->max_consumers > 1) {
                uint32_t *idx = JRB_CONS_IDX(rb);
                uint8_t  *act = JRB_CONS_ACT(rb);
                for (uint32_t i = 0, j = 0; i < rb->max_consumers && j < rb->cur_consumers; ++i) {
                    if (act[i]) {
                        ++j;
                        if ((rb->write_index - idx[i]) > (rb->write_index - drop_amount)) {
                            idx[i] = new_min;
                        }
                    }
                }
            }

            rb->min_read_index = new_min;
            rb->data_len       = rb->write_index - new_min;
        } else {
            /* 丢弃全部也不够 */
            goto err;
        }

        space = rb->capacity - rb->data_len;
    }

    /* 检查写入条件 */
    if (space < need) {
        goto err;
    }

    /* 实际写入元素个数 */
    to_write = (len <= space) ? len : space;

    {
        /* 计算起始元素序号和字节偏移 */
        uint32_t wpos  = rb->write_index & (rb->capacity - 1);
        uint32_t tail  = rb->capacity - wpos;
        uint32_t first = (to_write <= tail) ? to_write : tail;

        /* 单生产者优化：数据拷贝期间临时解锁 */
        if (rb->max_producers == 1)
            jthread_mutex_unlock(&rb->mutex);

        /* 分段拷贝，避免跨越尾部 */
        uint8_t *ring = JRB_BUF(rb);
        memcpy(ring + wpos * rb->unit_size, data, first * rb->unit_size);
        if (to_write > tail) {
            memcpy(ring, (const uint8_t*)data + first * rb->unit_size, (to_write - tail) * rb->unit_size);
        }

        if (rb->max_producers == 1)
            jthread_mutex_lock(&rb->mutex);

        rb->write_index += to_write;
        rb->data_len    += to_write;
    }

    if (rb->data_len >= rb->wake_num)
        jthread_cond_broadcast(&rb->not_empty);
    --rb->rw_count;
    jthread_mutex_unlock(&rb->mutex);

    return (int)to_write;

err:
    --rb->rw_count;
    jthread_mutex_unlock(&rb->mutex);
    return -1;
}

/**
 * @brief   从缓冲区读取数据
 */
int jringbuf_read(jringbuf_t *rb, int consumer_id, void *buf, uint32_t len,
                  uint32_t *size, uint32_t strategy, int arg)
{
    if (!rb || !buf || !len)
        return -1;

    int complete = (strategy & JRINGBUF_COMPLETE) ? 1 : 0;
    int block    = (strategy & JRINGBUF_BLOCK)    ? 1 : 0;
    int retry    = (strategy & JRINGBUF_RETRY)    ? 1 : 0;

    uint32_t avail = 0;
    uint32_t need = complete ? len : 1;       /* 最少需要的数据量（元素个数） */
    uint32_t c_read = 0, to_read = 0;
    int shared_mode = 0;

    jthread_mutex_lock(&rb->mutex);
    ++rb->rw_count;

    /* 单消费者固定 ID 为 0，无需校验 active 数组 */
    if (rb->max_consumers == 1) {
        consumer_id = 0;
    } else if (consumer_id < 0 || (uint32_t)consumer_id >= rb->max_consumers) {
        goto err;
    }

    shared_mode = (rb->max_consumers == 1 || rb->read_mode == JRINGBUF_READ_SHARED);

    do {
        /* 正在销毁ringbuf */
        if (rb->disable_rw) {
            goto err;
        }

        /* 校验消费者有效性 */
        if (rb->max_consumers > 1 && !JRB_CONS_ACT(rb)[consumer_id]) {
            goto err;
        }

        /* 获取该消费者或全局可用元素个数 */
        if (shared_mode) {
            avail = rb->data_len;
        } else {
            avail = rb->write_index - JRB_CONS_IDX(rb)[consumer_id];
        }

        /* 满足 need，直接跳出 */
        if (avail >= need) {
            break;
        }

        /* 没有重试或阻塞策略 */
        if ((!block && !retry) || !arg) {
            goto err;
        }

        if (block) {
            if (arg == -1) {
                /* 无限等待 */
                jthread_cond_wait(&rb->not_empty, &rb->mutex);
            } else {
                /* 超时等待 */
                uint64_t t1, t2;
                t1 = jtime_monomsec_get();
                jthread_cond_mtimewait(&rb->not_empty, &rb->mutex, arg);
                t2 = jtime_monomsec_get();
                arg = ((int)(t2 - t1) < arg) ? (arg - (int)(t2 - t1)) : 0;
            }
        } else { /* retry */
            if (arg > 0) {
                --arg;
            }
            jthread_mutex_unlock(&rb->mutex);
            jthread_yield();
            jthread_mutex_lock(&rb->mutex);
        }
    } while (1);

    /* 最终校验：至少满足最小需求 */
    if (avail < need) {
        goto err;
    }

    /* 实际读取部分 */
    if (size)
        *size = avail;

    c_read  = (rb->max_consumers == 1) ? rb->min_read_index :
                       (rb->read_mode == JRINGBUF_READ_SHARED) ? rb->min_read_index :
                       JRB_CONS_IDX(rb)[consumer_id];
    to_read = (len < avail) ? len : avail;

    {
        uint32_t rpos  = c_read & (rb->capacity - 1);
        uint32_t tail  = rb->capacity - rpos;
        uint32_t first = (to_read <= tail) ? to_read : tail;

        if (rb->max_consumers == 1) {
            rb->min_read_lock = 1;
            jthread_mutex_unlock(&rb->mutex);
        }

        uint8_t *ring = JRB_BUF(rb);
        memcpy(buf, ring + rpos * rb->unit_size, first * rb->unit_size);
        if (to_read > tail) {
            memcpy((uint8_t*)buf + first * rb->unit_size, ring, (to_read - tail) * rb->unit_size);
        }

        if (rb->max_consumers == 1) {
            jthread_mutex_lock(&rb->mutex);
            rb->min_read_lock = 0;
        }

        /* 更新读位置 */
        if (shared_mode) {
            rb->min_read_index += to_read;
            rb->data_len       -= to_read;
        } else {
            uint32_t *idx = JRB_CONS_IDX(rb);
            idx[consumer_id] += to_read;
            if (!rb->min_read_stale && c_read == rb->min_read_index) {
                rb->min_read_stale = 1;
            }
            rb->data_len = rb->write_index - rb->min_read_index;
        }
    }

    jthread_cond_broadcast(&rb->not_full);
    --rb->rw_count;
    jthread_mutex_unlock(&rb->mutex);

    return (int)to_read;
err:
    if (size)
        *size = 0;
    --rb->rw_count;
    jthread_mutex_unlock(&rb->mutex);
    return -1;
}

/*----------------------------------------------------------------------------
  生产者管理
----------------------------------------------------------------------------*/

/**
 * @brief   添加一个生产者，返回生产者 ID
 */
int jringbuf_add_producer(jringbuf_t *rb)
{
    if (!rb)
        return -1;
    if (rb->max_producers == 1)
        return 0;

    jthread_mutex_lock(&rb->mutex);
    uint8_t *prod = JRB_PROD_ACT(rb);
    uint32_t i = rb->cur_producers;
    if (rb->cur_producers < rb->max_producers) {
        if (!prod[i]) {
            goto end;
        } else {
            for (i = 0; i < rb->max_producers; ++i) {
                if (!prod[i])
                    goto end;
            }
        }
    }
    jthread_mutex_unlock(&rb->mutex);
    return -1;
end:
    prod[i] = 1;
    ++rb->cur_producers;
    jthread_mutex_unlock(&rb->mutex);
    return (int)i;
}

/**
 * @brief   移除一个生产者，producer_id 为 -1 时移除所有
 */
int jringbuf_del_producer(jringbuf_t *rb, int producer_id)
{
    if (!rb)
        return -1;
    if (rb->max_producers == 1) {
        return (producer_id == 0 || producer_id == -1) ? 0 : -1;
    }

    jthread_mutex_lock(&rb->mutex);
    uint8_t *prod = JRB_PROD_ACT(rb);
    if (producer_id == -1) {
        for (uint32_t i = 0; i < rb->max_producers; ++i)
            prod[i] = 0;
        rb->cur_producers = 0;
        goto end;
    }

    if (producer_id < 0 || (uint32_t)producer_id >= rb->max_producers || !prod[producer_id]) {
        jthread_mutex_unlock(&rb->mutex);
        return -1;
    }
    prod[producer_id] = 0;
    --rb->cur_producers;

end:
    jthread_cond_broadcast(&rb->not_full);
    jthread_mutex_unlock(&rb->mutex);
    return 0;
}

/*----------------------------------------------------------------------------
  消费者管理
----------------------------------------------------------------------------*/

/**
 * @brief   添加一个消费者，返回消费者 ID
 */
int jringbuf_add_consumer(jringbuf_t *rb, int use_ridx)
{
    if (!rb)
        return -1;
    if (rb->max_consumers == 1)
        return 0;

    jthread_mutex_lock(&rb->mutex);
    uint8_t  *act = JRB_CONS_ACT(rb);
    uint32_t i = rb->cur_consumers;
    if (rb->cur_consumers < rb->max_consumers) {
        if (!act[i]) {
            goto end;
        } else {
            for (i = 0; i < rb->max_consumers; ++i) {
                if (!act[i])
                    goto end;
            }
        }
    }
    jthread_mutex_unlock(&rb->mutex);
    return -1;
end:
    act[i] = 1;
    ++rb->cur_consumers;
    if (rb->read_mode == JRINGBUF_READ_EXCLUSIVE) {
        uint32_t *idx = JRB_CONS_IDX(rb);
        idx[i] = use_ridx ? rb->min_read_index : rb->write_index; // 初始读位置（元素）
    }
    jthread_mutex_unlock(&rb->mutex);
    return (int)i;
}

/**
 * @brief   移除一个消费者，consumer_id 为 -1 时移除所有
 */
int jringbuf_del_consumer(jringbuf_t *rb, int consumer_id)
{
    if (!rb)
        return -1;
    if (rb->max_consumers == 1) {
        if (consumer_id == 0 || consumer_id == -1) {
            jthread_mutex_lock(&rb->mutex);
            rb->min_read_index = rb->write_index;
            rb->data_len       = 0;
            jthread_cond_broadcast(&rb->not_full);
            jthread_mutex_unlock(&rb->mutex);
            return 0;
        }
        return -1;
    }

    jthread_mutex_lock(&rb->mutex);
    uint8_t *act = JRB_CONS_ACT(rb);
    if (consumer_id == -1) {
        for (uint32_t i = 0; i < rb->max_consumers; ++i)
            act[i] = 0;
        rb->cur_consumers = 0;
        goto end;
    }

    if (consumer_id < 0 || (uint32_t)consumer_id >= rb->max_consumers || !act[consumer_id]) {
        jthread_mutex_unlock(&rb->mutex);
        return -1;
    }
    act[consumer_id] = 0;
    --rb->cur_consumers;

end:
    update_min_read_index(rb);
    jthread_cond_broadcast(&rb->not_full);
    jthread_cond_broadcast(&rb->not_empty);
    jthread_mutex_unlock(&rb->mutex);
    return 0;
}

/**
 * @brief   丢弃消费者的部分或全部未读数据
 */
int jringbuf_drop_data(jringbuf_t *rb, int consumer_id, uint32_t dropped)
{
    if (!rb)
        return -1;

    jthread_mutex_lock(&rb->mutex);
    while (rb->min_read_lock) {
        jthread_mutex_unlock(&rb->mutex);
        jthread_yield();
        jthread_mutex_lock(&rb->mutex);
    }

    uint32_t avail, drop;
    uint8_t  *act = NULL;

    if (rb->max_consumers == 1) {
        if (consumer_id == 0 || consumer_id == -1) {
            goto end2;
        }
        jthread_mutex_unlock(&rb->mutex);
        return -1;
    }

    act = JRB_CONS_ACT(rb);
    if (consumer_id == -1) {
        if (rb->read_mode == JRINGBUF_READ_EXCLUSIVE) {
            uint32_t *idx = JRB_CONS_IDX(rb);
            for (uint32_t i = 0, j = 0; i < rb->max_consumers && j < rb->cur_consumers; ++i) {
                if (act[i]) {
                    avail = rb->write_index - idx[i];
                    drop = (dropped == 0 || dropped > avail) ? avail : dropped;
                    idx[i] += drop;
                    ++j;
                }
            }
            update_min_read_index(rb);
            goto end1;
        } else {
            goto end2;
        }
    }

    if (consumer_id < 0 || (uint32_t)consumer_id >= rb->max_consumers || !act[consumer_id]) {
        jthread_mutex_unlock(&rb->mutex);
        return -1;
    }

    if (rb->read_mode == JRINGBUF_READ_EXCLUSIVE) {
        uint32_t *idx = JRB_CONS_IDX(rb);
        avail = rb->write_index - idx[consumer_id];
        drop = (dropped == 0 || dropped > avail) ? avail : dropped;
        idx[consumer_id] += drop;
        update_min_read_index(rb);
        goto end1;
    } else {
        goto end2;
    }

end2:
    /* 共享模式：直接丢弃全局数据 */
    avail = rb->data_len;
    drop = (dropped == 0 || dropped > avail) ? avail : dropped;
    rb->min_read_index += drop;
    rb->data_len       -= drop;
end1:
    jthread_cond_broadcast(&rb->not_full);
    jthread_mutex_unlock(&rb->mutex);
    return 0;
}

