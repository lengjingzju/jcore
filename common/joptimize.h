/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief   避免编译警告的属性设置
 */
#define JATTR_UNUSED            __attribute__((unused))         // 变量/函数未使用
#define JATTR_FALLTHROUGH       __attribute__((fallthrough))    // switch-case中无break，继续下一个case

/**
 * @brief   函数或语句的属性设置，以便于编译器进一步优化
 */
#define JATTR_LIKELY(cond)      __builtin_expect(!!(cond), 1)   // 条件判断中走这条分支概率很高
#define JATTR_UNLIKELY(cond)    __builtin_expect(!!(cond), 0)   // 条件判断中走这条分支概率很低
#define JATTR_UNREACHABLE()     __builtin_unreachable()         // CPU永远不会运行到此处代码，用于无效分支
#define JATTR_NORETURN          __attribute__((noreturn))       // 标记函数为不返回
#define JATTR_INLINE            __attribute__((always_inline))  // 强制函数内联
#define JATTR_NOINLINE          __attribute__((noinline))       // 禁止函数内联
#define JATTR_PURE              __attribute__((pure))           // 标记函数为纯函数(无副作用，返回值仅依赖输入)
#define JATTR_COLD              __attribute__((cold))           // 标记函数为冷路径(优化为更小的代码体积)，不太可能被执行
#define JATTR_HOT               __attribute__((hot))            // 标记函数为热路径(优化为更快执行)，常被执行

/**
 * @brief   改变编译行为的属性设置
 */
#define JATTR_ALIGNED(n)        __attribute__((aligned(n)))     // 指定结构体字节对齐数，必须是2的N次方
#define JATTR_PACKED            __attribute__((packed))         // 指定结构体字节紧密分配，即对齐到1字节

/**
 * @brief   改变运行行为的属性设置
 */
#define JATTR_CONSTRUCTOR       __attribute__((constructor))    // 标记main函数运行前运行此函数
#define JATTR_DESTRUCTOR        __attribute__((destructor))     // 标记main函数退出后运行此函数

/**
 * @brief   翻转字节顺序
 * @param   p [INOUT] 要翻转的数据指针
 * @return  无返回值
 * @note    无
 */
static inline void jbyte2_reverse(void *p)  { uint16_t *val = (uint16_t *)p; *val = __builtin_bswap16(*val); }
static inline void jbyte4_reverse(void *p)  { uint32_t *val = (uint32_t *)p; *val = __builtin_bswap32(*val); }
static inline void jbyte8_reverse(void *p)  { uint64_t *val = (uint64_t *)p; *val = __builtin_bswap64(*val); }

/**
 * @brief   获取尾缀零位数量
 * @param   n [IN] 要获取的值
 * @return  返回数量
 * @note    返回输入数二进制表示从最低位开始(右起)的连续的零位的个数
 */
static inline int jbit32_ctz(uint32_t n)    { return n ? __builtin_ctz(n) : 32; }
static inline int jbit64_ctz(uint64_t n)    { return n ? __builtin_ctzll(n) : 64; }

/**
 * @brief   获取尾缀第一个1的序号
 * @param   n [IN] 要获取的值
 * @return  返回序号；输入为0时返回0
 * @note    返回输入数二进制表示从最低位开始(右起)的第1个壹位的序号(序号从1开始算)
 *          除输入为0外等价ffs = ctz + 1
 */
static inline int jbit32_ffs(int32_t n)     { return __builtin_ffs(n); }
static inline int jbit64_ffs(int64_t n)     { return __builtin_ffsll(n); }

/**
 * @brief   获取前导零位数量
 * @param   n [IN] 要获取的值
 * @return  返回数量
 * @note    返回输入数二进制表示从最高位开始(左起)的连续的零位的个数
 */
static inline int jbit32_clz(uint32_t n)    { return n ? __builtin_clz(n) : 32; }
static inline int jbit64_clz(uint64_t n)    { return n ? __builtin_clzll(n) : 64; }

/**
 * @brief   获取和前导符号位相同位的数量
 * @param   n [IN] 要获取的值
 * @return  返回数量；输入为0时返回0
 * @note    返回从最高有效位(左起)开始，连续与符号位相同的位的个数(不包括符号位本身)
 *          输入为非负数时等价clrsb = clz - 1
 */
static inline int jbit32_clrsb(int32_t n)   { return __builtin_clrsb(n); }
static inline int jbit64_clrsb(int64_t n)   { return __builtin_clrsbll(n); }

/**
 * @brief   获取位1的个数
 * @param   n [IN] 要获取的值
 * @return  返回数量
 * @note    返回输入数二进制表示中，位为1的数量
 */
static inline int jbit8_count(uint8_t n)    { return __builtin_popcount(n); }
static inline int jbit32_count(uint32_t n)  { return __builtin_popcount(n); }
static inline int jbit64_count(uint64_t n)  { return __builtin_popcountll(n); }

#elif defined(_MSC_VER)
#include <intrin.h>
#include <stdlib.h>

#define JATTR_UNUSED                        /* 无效果 */
#define JATTR_FALLTHROUGH                   /* 无效果 */

#define JATTR_LIKELY(cond)                  (cond) /* 无效果 */
#define JATTR_UNLIKELY(cond)                (cond) /* 无效果 */
#define JATTR_UNREACHABLE()                 do {} while (0) /* 无效果 */
#define JATTR_NORETURN                      __declspec(noreturn)
#define JATTR_INLINE                        __forceinline
#define JATTR_NOINLINE                      __declspec(noinline)
#define JATTR_PURE                          /* 无效果 */
#define JATTR_COLD                          /* 无效果 */
#define JATTR_HOT                           /* 无效果 */

#define JATTR_ALIGNED(n)                    __declspec(align(n))
#define JATTR_PACKED                        __declspec(align(1))

#define JATTR_CONSTRUCTOR                   "attribute 'constructor' is not supported!" /* 不支持 */
#define JATTR_DESTRUCTOR                    "attribute 'destructor' is not supported!" /* 不支持 */

static inline void jbyte2_reverse(void *p)  { uint16_t *val = (uint16_t *)p; *val = _byteswap_ushort(*val); }
static inline void jbyte4_reverse(void *p)  { uint32_t *val = (uint32_t *)p; *val = _byteswap_ulong(*val); }
static inline void jbyte8_reverse(void *p)  { uint64_t *val = (uint64_t *)p; *val = _byteswap_uint64(*val); }

static inline int jbit32_ctz(uint32_t n)    { unsigned long i; return _BitScanForward(&i, n) ? (int)i : 32; }
static inline int jbit64_ctz(uint64_t n)    { unsigned long i; return _BitScanForward64(&i, n) ? (int)i : 64; }

static inline int jbit32_ffs(int32_t n)     { unsigned long i; return _BitScanForward(&i, n) ? (int)i + 1 : 0; }
static inline int jbit64_ffs(int64_t n)     { unsigned long i; return _BitScanForward64(&i, n) ? (int)i + 1 : 0; }

static inline int jbit32_clz(uint32_t n)    { unsigned long i; return _BitScanReverse(&i, n) ? (int)(31 - i) : 32; }
static inline int jbit64_clz(uint64_t n)    { unsigned long i; return _BitScanReverse64(&i, n) ? (int)(63 - i) : 64; }

static inline int jbit32_clrsb(int32_t n)   { return (n >= 0 ? jbit32_clz(n) : jbit32_clz(~n)) - 1; }
static inline int jbit64_clrsb(int64_t n)   { return (n >= 0 ? jbit64_clz(n) : jbit64_clz(~n)) - 1; }

static inline int jbit8_count(uint8_t n)    { return __popcnt(n); }
static inline int jbit32_count(uint32_t n)  { return __popcnt(n); }
static inline int jbit64_count(uint64_t n)  { return (int)__popcnt64(n); }

#else

#define JATTR_UNUSED                        /* 无效果 */
#define JATTR_FALLTHROUGH                   /* 无效果 */

#define JATTR_LIKELY(cond)                  (cond) /* 无效果 */
#define JATTR_UNLIKELY(cond)                (cond) /* 无效果 */
#define JATTR_UNREACHABLE()                 do {} while (0) /* 无效果 */
#define JATTR_NORETURN                      /* 无效果 */
#define JATTR_INLINE                        inline
#define JATTR_NOINLINE                      /* 无效果 */
#define JATTR_PURE                          /* 无效果 */
#define JATTR_COLD                          /* 无效果 */
#define JATTR_HOT                           /* 无效果 */

#define JATTR_ALIGNED(n)                    "attribute 'aligned' is not supported!" /* 不支持 */
#define JATTR_PACKED                        "attribute 'packed' is not supported!" /* 不支持 */

#define JATTR_CONSTRUCTOR                   "attribute 'constructor' is not supported!" /* 不支持 */
#define JATTR_DESTRUCTOR                    "attribute 'destructor' is not supported!" /* 不支持 */

static inline void jbyte2_reverse(void *p)
{
    uint8_t *b = (uint8_t*)p;
    uint8_t t;
    t = b[0]; b[0] = b[1]; b[1] = t;
}
static inline void jbyte4_reverse(void *p)
{
    uint8_t *b = (uint8_t*)p;
    uint8_t t;
    t = b[0]; b[0] = b[3]; b[3] = t;
    t = b[1]; b[1] = b[2]; b[2] = t;
}
static inline void jbyte8_reverse(void *p)
{
    uint8_t *b = (uint8_t*)p;
    uint8_t t;
    t = b[0]; b[0] = b[7]; b[7] = t;
    t = b[1]; b[1] = b[6]; b[6] = t;
    t = b[2]; b[2] = b[5]; b[5] = t;
    t = b[3]; b[3] = b[4]; b[4] = t;
}

static inline int jbit32_ctz(uint32_t n)
{
    int c = 0;
    if (n == 0)             return 32;
    if (!(n & 0xFFFF))      { c += 16; n >>= 16; }
    if (!(n & 0xFF))        { c += 8;  n >>= 8;  }
    if (!(n & 0xF))         { c += 4;  n >>= 4;  }
    if (!(n & 0x3))         { c += 2;  n >>= 2;  }
    return c + !(n & 0x1);
}
static inline int jbit64_ctz(uint64_t n)
{
    int c = 0;
    if (n == 0)             return 64;
    if (!(n & 0xFFFFFFFF))  { c += 32; n >>= 32; }
    if (!(n & 0xFFFF))      { c += 16; n >>= 16; }
    if (!(n & 0xFF))        { c += 8;  n >>= 8;  }
    if (!(n & 0xF))         { c += 4;  n >>= 4;  }
    if (!(n & 0x3))         { c += 2;  n >>= 2;  }
    return c + !(n & 0x1);
}

static inline int jbit32_ffs(int32_t n)     { return n ? jbit32_ctz(n) + 1 : 0; }
static inline int jbit64_ffs(int64_t n)     { return n ? jbit64_ctz(n) + 1 : 0; }

static inline int jbit32_clz(uint32_t n)
{
    int c = 0;
    if (n == 0)                 return 32;
    if (n & 0xFFFF0000)         n >>= 16; else c += 16;
    if (n & 0xFF00)             n >>= 8 ; else c += 8;
    if (n & 0xF0)               n >>= 4 ; else c += 4;
    if (n & 0xC)                n >>= 2 ; else c += 2;
    return c + (n & 0x1);
}
static inline int jbit64_clz(uint64_t n)
{
    int c = 0;
    if (n == 0)                 return 64;
    if (n & 0xFFFFFFFF00000000) n >>= 32; else c += 32;
    if (n & 0xFFFF0000)         n >>= 16; else c += 16;
    if (n & 0xFF00)             n >>= 8 ; else c += 8;
    if (n & 0xF0)               n >>= 4 ; else c += 4;
    if (n & 0xC)                n >>= 2 ; else c += 2;
    return c + (n & 0x1);
}

static inline int jbit32_clrsb(int32_t n)   { return (n >= 0 ? jbit32_clz(n) : jbit32_clz(~n)) - 1; }
static inline int jbit64_clrsb(int64_t n)   { return (n >= 0 ? jbit64_clz(n) : jbit64_clz(~n)) - 1; }

static inline int jbit8_count(uint8_t n)
{
    static const uint8_t count_lut[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
    };
    return count_lut[n];
}
static inline int jbit32_count(uint32_t n)
{
    return jbit8_count((uint8_t)(n & 0xFF)) + jbit8_count((uint8_t)(n >> 8 & 0xFF))
           + jbit8_count((uint8_t)(n >> 16 & 0xFF)) + jbit8_count((uint8_t)(n >> 24));
}
static inline int jbit64_count(uint64_t n)
{
    return jbit32_count((uint32_t)(n & 0xFFFFFFFF)) + jbit32_count((uint32_t)(n >> 32));
}
#endif

#if (__WORDSIZE == 64) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9)
#define JU128_NATIVE_SUPPORT    1
#else
#define JU128_NATIVE_SUPPORT    0
#endif

#if JU128_NATIVE_SUPPORT
__extension__ typedef unsigned __int128 ju128_t;
static inline uint64_t ju128_high(ju128_t x) { return (uint64_t)(x >> 64); }
static inline uint64_t ju128_low(ju128_t x) { return (uint64_t)(x); }

static inline int ju128_cmp(ju128_t x, ju128_t y)
{
    return x > y ? 1 : (x < y ? -1 : 0);
}

static inline ju128_t ju128_add(ju128_t x, ju128_t y)
{
    return x + y;
}

static inline ju128_t ju128_sub(ju128_t x, ju128_t y)
{
    return x - y;
}

static inline ju128_t ju128_mul(uint64_t x, uint64_t y)
{
    return (ju128_t)x * y;
}

static inline uint64_t ju128_div(ju128_t dividend, uint64_t divisor, uint64_t *remainder)
{
    uint64_t ret = (uint64_t)(dividend / divisor);
    *remainder = dividend - (ju128_t)ret * divisor;
    return ret;
}

#else

typedef struct _ju128 { uint64_t hi; uint64_t lo; } ju128_t;
static inline uint64_t ju128_high(ju128_t x) { return x.hi; }
static inline uint64_t ju128_low(ju128_t x) { return x.lo; }

static inline int ju128_cmp(ju128_t x, ju128_t y)
{
    if (x.hi > y.hi) return 1;
    if (x.hi < y.hi) return -1;
    if (x.lo > y.lo) return 1;
    if (x.lo < y.lo) return -1;
    return 0;
}

static inline ju128_t ju128_add(ju128_t x, ju128_t y)
{
    ju128_t ret = {0, 0};
    uint64_t diff = 0xFFFFFFFFFFFFFFFF - x.lo;
    if (y.lo > diff) {
        ret.lo = y.lo - diff - 1;
        ret.hi = x.hi + y.hi + 1;
    } else {
        ret.lo = x.lo + y.lo;
        ret.hi = x.hi + y.hi;
    }
    return ret;
}

static inline ju128_t ju128_sub(ju128_t x, ju128_t y)
{
    ju128_t ret = {0, 0};
    if (x.lo < y.lo) {
        uint64_t diff = 0xFFFFFFFFFFFFFFFF - y.lo;
        ret.lo = x.lo + diff + 1;
        ret.hi = x.hi - y.hi - 1;
    } else {
        ret.lo = x.lo - y.lo;
        ret.hi = x.hi - y.hi;
    }
    return ret;
}

static inline ju128_t ju128_mul(uint64_t x, uint64_t y)
{
    ju128_t ret;
#if defined(_MSC_VER) && defined(_M_AMD64)
    ret.lo = _umul128(x, y, &ret.hi);
#else
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x >> 32;
    const uint64_t b = x & M32;
    const uint64_t c = y >> 32;
    const uint64_t d = y & M32;

    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;

    const uint64_t mid1 = ad + (bd >> 32);
    const uint64_t mid2 = bc + (mid1 & M32);

    ret.hi = ac + (mid1 >> 32) + (mid2 >> 32);
    ret.lo = (bd & M32) | (mid2 & M32) << 32;
#endif
    return ret;
}

static inline uint64_t ju128_div(ju128_t dividend, uint64_t divisor, uint64_t *remainder)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    return _udiv128(dividend.hi, dividend.lo, divisor, remainder);
#else
    ju128_t ret = {0, 0};

    if (!dividend.hi) {
        ret.lo = dividend.lo / divisor;
        *remainder = dividend.lo % divisor;
        return ret.lo;
    }

    int32_t abits = 64 - jbit64_clz(dividend.hi);
    int32_t bbits = 64 - jbit64_clz(divisor);
    int32_t shift = 0;

    if (abits >= bbits) {
        /* dividend.hi >= divisor 时结果被截断 */
        shift = 64;
        while (abits > bbits) {
            if (dividend.hi >= (divisor << (abits - bbits))) {
                dividend.hi -= divisor << (abits - bbits);
                ret.hi |= (uint64_t)1;
            }
            ret.hi <<= 1;
            --abits;
        }
        if (dividend.hi >= divisor) {
            dividend.hi -= divisor;
            ret.hi |= (uint64_t)1;
        }
    } else {
        shift = 64;
        dividend.hi = (dividend.hi << (bbits - abits - 1)) | (dividend.lo >> (64 - (bbits - abits - 1)));
        dividend.lo <<= (bbits - abits - 1);
        shift -= (bbits - abits - 1);
    }

    while (--shift) {
        dividend.hi = (dividend.hi << 1) | (dividend.lo >> 63);
        dividend.lo <<= 1;
        if (dividend.hi >= divisor) {
            dividend.hi -= divisor;
            ret.lo |= (uint64_t)1;
        }
        ret.lo <<= 1;
    }

    dividend.hi = (dividend.hi << 1) | (dividend.lo >> 63);
    dividend.lo <<= 1;
    if (dividend.hi >= divisor) {
        dividend.hi -= divisor;
        ret.lo |= (uint64_t)1;
    }

    *remainder = dividend.hi;
    return ret.lo;
#endif
}
#endif

#ifdef __cplusplus
}
#endif
