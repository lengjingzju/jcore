/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "jheap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JBITMAP_SHIFT           6
#define JBITMAP_UMASK           63UL
#define JBITMAP_USIZE           64
#define JBITMAP_NSIZE(nbits)    ((nbits + JBITMAP_UMASK) >> JBITMAP_SHIFT)

/**
 * @brief   位图数组中元素类型
 */
typedef uint64_t jbitmap_t;

/**
 * @brief   获取尾部零位的数量
 * @param   n [IN] 要处理的数字
 * @return  返回值
 * @note    无
 */
static inline int jbitmap_tail_zeros(uint64_t n)
{
#if defined(__GNUC__) || defined(__clang__)
    return n ? __builtin_ctzll(n) : 64;
#elif defined(_MSC_VER)
    unsigned long index;
    return _BitScanForward64(&index, n) ? (int)index : 64;
#else
    int count = 0;
    if (n == 0)            return 64;
    if (!(n & 0xFFFFFFFF)) { count += 32; n >>= 32; }
    if (!(n & 0xFFFF))     { count += 16; n >>= 16; }
    if (!(n & 0xFF))       { count += 8;  n >>= 8;  }
    if (!(n & 0xF))        { count += 4;  n >>= 4;  }
    if (!(n & 0x3))        { count += 2;  n >>= 2;  }
    return count + !(n & 0x1);
#endif
}

/**
 * @brief   获取指定位置置1的值
 * @param   pos [IN] 要置位的位置
 * @return  返回值
 * @note    无
 */
static inline jbitmap_t jbitmap_pmask(int pos)
{
    return 1ULL << (pos & JBITMAP_UMASK);
}

/**
 * @brief   获取指定位数置1的值
 * @param   nbits [IN] 位图的总位数
 * @return  返回值
 * @note    nbits不能大于JBITMAP_USIZE
 */
static inline jbitmap_t jbitmap_nmask(int nbits)
{
    static const jbitmap_t jbitmap_mask_lut[JBITMAP_USIZE + 1] = {
        0x0,
        0x1, 0x3, 0x7, 0xF,
        0x1F, 0x3F, 0x7F, 0xFF,
        0x1FF, 0x3FF, 0x7FF, 0xFFF,
        0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
        0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF,
        0x1FFFFF, 0x3FFFFF, 0x7FFFFF, 0xFFFFFF,
        0x1FFFFFF, 0x3FFFFFF, 0x7FFFFFF, 0xFFFFFFF,
        0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
        0x1FFFFFFFF, 0x3FFFFFFFF, 0x7FFFFFFFF, 0xFFFFFFFFF,
        0x1FFFFFFFFF, 0x3FFFFFFFFF, 0x7FFFFFFFFF, 0xFFFFFFFFFF,
        0x1FFFFFFFFFF, 0x3FFFFFFFFFF, 0x7FFFFFFFFFF, 0xFFFFFFFFFFF,
        0x1FFFFFFFFFFF, 0x3FFFFFFFFFFF, 0x7FFFFFFFFFFF, 0xFFFFFFFFFFFF,
        0x1FFFFFFFFFFFF, 0x3FFFFFFFFFFFF, 0x7FFFFFFFFFFFF, 0xFFFFFFFFFFFFF,
        0x1FFFFFFFFFFFFF, 0x3FFFFFFFFFFFFF, 0x7FFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF,
        0x1FFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFF,
        0x1FFFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF
    };
    return jbitmap_mask_lut[nbits];
}

/**
 * @brief   位图置位1位
 * @param   dst [OUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  无返回值
 * @note    用户自己保证不超出位图范围
 */
static inline void jbitmap_set_bit(jbitmap_t *dst, int pos)
{
    dst[pos >> JBITMAP_SHIFT] |= jbitmap_pmask(pos);
}

/**
 * @brief   位图清零1位
 * @param   dst [OUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  无返回值
 * @note    用户自己保证不超出位图范围
 */
static inline void jbitmap_clear_bit(jbitmap_t *dst, int pos)
{
    dst[pos >> JBITMAP_SHIFT] &= ~jbitmap_pmask(pos);
}

/**
 * @brief   位图改变1位
 * @param   dst [OUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  无返回值
 * @note    用户自己保证不超出位图范围
 */
static inline void jbitmap_change_bit(jbitmap_t *dst, int pos)
{
    dst[pos >> JBITMAP_SHIFT] ^= jbitmap_pmask(pos);
}

/**
 * @brief   位图获取指定位
 * @param   src [IN] 要处理的位图
 * @param   pos [IN] 要获取的位置
 * @return  指定位为1时返回true；否则返回false
 * @note    用户自己保证不超出位图范围
 */
static inline bool jbitmap_test_bit(const jbitmap_t *src, int pos)
{
    return (src[pos >> JBITMAP_SHIFT] & jbitmap_pmask(pos)) != 0;
}

/**
 * @brief   位图获取指定位并置位
 * @param   src [INOUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  指定位旧值为1时返回true；否则返回false
 * @note    用户自己保证不超出位图范围
 */
static inline bool jbitmap_test_and_set_bit(jbitmap_t *src, int pos)
{
    int idx = pos >> JBITMAP_SHIFT;
    jbitmap_t mask = jbitmap_pmask(pos);
    bool ret = (src[idx] & mask) != 0;
    src[idx] |= mask;
    return ret;
}

/**
 * @brief   位图获取指定位并清零位
 * @param   src [INOUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  指定位旧值为1时返回true；否则返回false
 * @note    用户自己保证不超出位图范围
 */
static inline bool jbitmap_test_and_clear_bit(jbitmap_t *src, int pos)
{
    int idx = pos >> JBITMAP_SHIFT;
    jbitmap_t mask = jbitmap_pmask(pos);
    bool ret = (src[idx] & mask) != 0;
    src[idx] &= ~mask;
    return ret;
}

/**
 * @brief   位图获取指定位并改变位
 * @param   src [INOUT] 要处理的位图
 * @param   pos [IN] 要处理的位置
 * @return  指定位旧值为1时返回true；否则返回false
 * @note    用户自己保证不超出位图范围
 */
static inline bool jbitmap_test_and_change_bit(jbitmap_t *src, int pos)
{
    int idx = pos >> JBITMAP_SHIFT;
    jbitmap_t mask = jbitmap_pmask(pos);
    bool ret = (src[idx] & mask) != 0;
    src[idx] ^= mask;
    return ret;
}

/**
 * @brief   位图获取从指定元素开始的顶一个非零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @param   index [IN] 搜索起始元素序号
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_bit(const jbitmap_t *src, int nbits, int index)
{
    int total = JBITMAP_NSIZE(nbits);
    int i;
    for (i = index; i < total; ++i) {
        if (src[i]) {
            int pos = (i << JBITMAP_SHIFT) + jbitmap_tail_zeros(src[i]);
            return pos < nbits ? pos : -1;
        }
    }
    return -1;
}

/**
 * @brief   位图获取第一个非零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_first_bit(const jbitmap_t *src, int nbits)
{
    return jbitmap_find_bit(src, nbits, 0);
}

/**
 * @brief   位图获取下一个非零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @param   pos [IN] 开始的位置(含)
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_next_bit(const jbitmap_t *src, int nbits, int pos)
{
    if (pos >= nbits)
        return -1;
    int idx = pos >> JBITMAP_SHIFT;
    jbitmap_t val = src[idx] >> (pos & JBITMAP_UMASK);
    if (val) {
        pos += jbitmap_tail_zeros(val);
        return pos < nbits ? pos : -1;
    }
    return jbitmap_find_bit(src, nbits, idx + 1);
}

/**
 * @brief   位图获取从指定元素开始的第一个零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @param   index [IN] 搜索起始元素序号
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_zero_bit(const jbitmap_t *src, int nbits, int index)
{
    int total = JBITMAP_NSIZE(nbits);
    jbitmap_t mask = ~(jbitmap_t)0;
    int i;
    for (i = index; i < total; ++i) {
        if (src[i] != mask) {
            int pos = (i << JBITMAP_SHIFT) + jbitmap_tail_zeros(~src[i]);
            return pos < nbits ? pos : -1;
        }
    }
    return -1;
}

/**
 * @brief   位图获取第一个零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_first_zero_bit(const jbitmap_t *src, int nbits)
{
    return jbitmap_find_zero_bit(src, nbits, 0);
}

/**
 * @brief   位图获取下一个零位的序号
 * @param   src [IN] 要处理的位图
 * @param   nbits [IN] 位图的总位数
 * @param   pos [IN] 开始的位置(含)
 * @return  成功返回序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_next_zero_bit(const jbitmap_t *src, int nbits, int pos)
{
    if (pos >= nbits)
        return -1;
    int idx = pos >> JBITMAP_SHIFT;
    jbitmap_t val = (~src[idx]) >> (pos & JBITMAP_UMASK);
    if (val) {
        pos += jbitmap_tail_zeros(val);
        return pos < nbits ? pos : -1;
    }
    return jbitmap_find_zero_bit(src, nbits, idx + 1);
}

/**
 * @brief   置位位图
 * @param   dst [OUT] 要置位的位图
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_fill(jbitmap_t *dst, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    idx += !mask;
    memset(dst, 0xFF, idx * sizeof(jbitmap_t));
    if (mask)
        dst[idx] |= mask;
}

/**
 * @brief   清零位图
 * @param   dst [OUT] 要清零的位图
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_zero(jbitmap_t *dst, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    idx += !mask;
    memset(dst, 0, idx * sizeof(jbitmap_t));
    if (mask)
        dst[idx] &= ~mask;
}

/**
 * @brief   复制位图
 * @param   dst [OUT] 复制后的位图
 * @param   src [IN] 被复制的位图
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_copy(jbitmap_t *dst, const jbitmap_t *src, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    idx += !mask;
    memcpy(dst, src, idx * sizeof(jbitmap_t));
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | (src[idx] & mask);
}

/**
 * @brief   位图与
 * @param   dst [OUT] 位图的计算结果
 * @param   src1 [IN] 要处理的位图源1
 * @param   src2 [IN] 要处理的位图源2
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_and(jbitmap_t *dst, const jbitmap_t *src1, const jbitmap_t *src2, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i)
        dst[i] = src1[i] & src2[i];
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | ((src1[idx] & src2[idx]) & mask);
}

/**
 * @brief   位图或
 * @param   dst [OUT] 位图的计算结果
 * @param   src1 [IN] 要处理的位图源1
 * @param   src2 [IN] 要处理的位图源2
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_or(jbitmap_t *dst, const jbitmap_t *src1, const jbitmap_t *src2, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i)
        dst[i] = src1[i] | src2[i];
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | ((src1[idx] | src2[idx]) & mask);
}

/**
 * @brief   位图异或
 * @param   dst [OUT] 位图的计算结果
 * @param   src1 [IN] 要处理的位图源1
 * @param   src2 [IN] 要处理的位图源2
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_xor(jbitmap_t *dst, const jbitmap_t *src1, const jbitmap_t *src2, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i)
        dst[i] = src1[i] ^ src2[i];
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | ((src1[idx] ^ src2[idx]) & mask);
}

/**
 * @brief   位图同或
 * @param   dst [OUT] 位图的计算结果
 * @param   src1 [IN] 要处理的位图源1
 * @param   src2 [IN] 要处理的位图源2
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_andnot(jbitmap_t *dst, const jbitmap_t *src1, const jbitmap_t *src2, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i)
        dst[i] = src1[i] & ~src2[i];
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | ((src1[idx] & ~src2[idx]) & mask);
}

/**
 * @brief   位图取反
 * @param   dst [OUT] 位图的计算结果
 * @param   src [IN] 要处理的位图源
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_complement(jbitmap_t *dst, const jbitmap_t *src, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i)
        dst[i] = ~src[i];
    if (mask)
        dst[idx] = (dst[idx] & ~mask) | ( (~src[idx]) & mask);
}

/**
 * @brief   判断位图相等
 * @param   src1 [IN] 要处理的位图源1
 * @param   src2 [IN] 要处理的位图源2
 * @param   nbits [IN] 位图的总位数
 * @return  相等返回true；不等返回false
 * @note    无
 */
static inline bool jbitmap_equal(const jbitmap_t *src1, const jbitmap_t *src2, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i) {
        if (src1[i] != src2[i])
            return false;
    }
    return mask ? (src1[idx] & mask) == (src2[idx] & mask) : true;
}

/**
 * @brief   判断位图全空
 * @param   src [IN] 要处理的位图源
 * @param   nbits [IN] 位图的总位数
 * @return  全空返回true；不全空返回false
 * @note    无
 */
static inline bool jbitmap_empty(const jbitmap_t *src, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i) {
        if (src[i])
            return false;
    }
    return mask ? (src[idx] & mask) == 0 : true;
}

/**
 * @brief   判断位图全置位
 * @param   src [IN] 要处理的位图源
 * @param   nbits [IN] 位图的总位数
 * @return  全置位返回true；不全置位返回false
 * @note    无
 */
static inline bool jbitmap_full(const jbitmap_t *src, int nbits)
{
    int idx = JBITMAP_NSIZE(nbits) - 1;
    jbitmap_t mask = jbitmap_nmask(nbits & JBITMAP_UMASK);
    int i;
    idx += !mask;
    for (i = 0; i < idx; ++i) {
        if (~src[i])
            return false;
    }
    return mask ? (src[idx] & mask) == mask : true;
}

/**
 * @brief   位图置位一些位
 * @param   dst [OUT] 位图的计算结果
 * @param   pos [IN] 要处理的开始位置
 * @param   size [IN] 位图的总位数
 * @return  无返回值
 * @note    用户自己保证不超出位图范围
 */
static inline void jbitmap_set(jbitmap_t *dst, int pos, int size)
{
    int start_idx = pos >> JBITMAP_SHIFT;
    int start_offset = pos & JBITMAP_UMASK;
    int end_idx = (pos + size - 1) >> JBITMAP_SHIFT;
    int end_offset = (pos + size - 1) & JBITMAP_UMASK;
    if (start_idx == end_idx) {
        dst[start_idx] |= jbitmap_nmask(size) << start_offset;
    } else {
        dst[start_idx] |= ~jbitmap_nmask(start_offset);
        if (start_idx + 1 < end_idx)
            memset(dst + start_idx + 1, 0xFF, (end_idx - start_idx - 1) * sizeof(jbitmap_t));
        dst[end_idx] |= jbitmap_nmask(end_offset + 1);
    }
}

/**
 * @brief   位图置清零一些位
 * @param   dst [OUT] 位图的计算结果
 * @param   pos [IN] 要处理的开始位置
 * @param   size [IN] 位图的总位数
 * @return  无返回值
 * @note    用户自己保证不超出位图范围
 */
static inline void jbitmap_clear(jbitmap_t *dst, int pos, int size)
{
    int start_idx = pos >> JBITMAP_SHIFT;
    int start_offset = pos & JBITMAP_UMASK;
    int end_idx = (pos + size - 1) >> JBITMAP_SHIFT;
    int end_offset = (pos + size - 1) & JBITMAP_UMASK;
    if (start_idx == end_idx) {
        dst[start_idx] &= ~(jbitmap_nmask(size) << start_offset);
    } else {
        dst[start_idx] &= jbitmap_nmask(start_offset);
        if (start_idx + 1 < end_idx)
            memset(dst + start_idx + 1, 0, (end_idx - start_idx - 1) * sizeof(jbitmap_t));
        dst[end_idx] &= ~jbitmap_nmask(end_offset + 1);
    }
}

/**
 * @brief   找到连续的置1位
 * @param   src [IN] 要处理的位图源
 * @param   nbits [IN] 位图的总位数
 * @param   size [IN] 需要连续的位数
 * @return  成功返回第一个序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_bits(jbitmap_t *src, int nbits, int size)
{
    int idx = jbitmap_find_first_bit(src, nbits);
    int nidx;
    while (1) {
        if (idx < 0)
            return -1;
        nidx = jbitmap_find_next_zero_bit(src, nbits, idx + 1);
        if (nidx < 0)
            return nbits - idx >= size ? idx : -1;
        if (nidx - idx >= size)
            return idx;
        idx = jbitmap_find_next_bit(src, nbits, nidx + 1);
    }
    return -1;
}

/**
 * @brief   找到连续的置0位
 * @param   src [IN] 要处理的位图源
 * @param   nbits [IN] 位图的总位数
 * @param   size [IN] 需要连续的位数
 * @return  成功返回第一个序号；失败返回-1
 * @note    无
 */
static inline int jbitmap_find_zero_bits(jbitmap_t *src, int nbits, int size)
{
    int idx = jbitmap_find_first_zero_bit(src, nbits);
    int nidx;
    while (1) {
        if (idx < 0)
            return -1;
        nidx = jbitmap_find_next_bit(src, nbits, idx + 1);
        if (nidx < 0)
            return nbits - idx >= size ? idx : -1;
        if (nidx - idx >= size)
            return idx;
        idx = jbitmap_find_next_zero_bit(src, nbits, nidx + 1);
    }
    return -1;
}

/**
 * @brief   分配全零的位图
 * @param   nbits [IN] 位图的总位数
 * @return  成功返回非零指针；失败返回NULL
 * @note    需要使用jbitmap_free释放资源
 */
static inline jbitmap_t *jbitmap_zalloc(int nbits)
{
    int total = JBITMAP_NSIZE(nbits);
    return (jbitmap_t *)jheap_calloc(total, sizeof(jbitmap_t));
}

/**
 * @brief   调整位图大小
 * @param   new_nbits [IN] 新位图的总位数
 * @param   orig_nbits [IN] 原位图的总位数
 * @param   ptr [IN] 原位图指针
 * @return  成功返回非零指针；失败返回NULL
 * @note    需要使用jbitmap_free释放资源
 */
static inline jbitmap_t *jbitmap_realloc(int new_nbits, int orig_nbits, jbitmap_t *ptr)
{
    int cur = JBITMAP_NSIZE(new_nbits);
    int orig = JBITMAP_NSIZE(orig_nbits);
    if (cur <= orig)
        return ptr;
    ptr = (jbitmap_t *)jheap_realloc(ptr, cur * sizeof(jbitmap_t));
    if (ptr)
        memset(ptr + orig, 0, (cur - orig) * sizeof(jbitmap_t));
    return ptr;
}

/**
 * @brief   释放分配的位图
 * @param   ptr [IN] 位图指针
 * @return  无返回值
 * @note    无
 */
static inline void jbitmap_free(jbitmap_t *ptr)
{
    if (ptr)
        jheap_free(ptr);
}

/**
 * @brief   定义一个静态位图
 * @param   name [IN] 位图的名称
 * @param   nbits [IN] 位图的总位数
 * @return  无返回值概念
 * @note    nbits要是一个常量
 */
#define DECLARE_JBITMAP(name, nbits)    jbitmap_t name[JBITMAP_NSIZE(nbits)] = {0}

#ifdef __cplusplus
}
#endif
