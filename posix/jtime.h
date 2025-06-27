/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE     1   // timegm函数需要此定义
#endif
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JTIME_DIV_1E3(N)    (((uint64_t)(N) * 274877907) >> 38) // N <= 4294967295
#define JTIME_DIV_1E6(N)    (((uint64_t)(N) * 1125899907) >> 50) // N <= 4294967295

/**
 * @brief   毫秒级睡眠
 */
#define jtime_msleep(msec)  usleep((msec) * 1000)

/**
 * @brief   微秒级睡眠
 */
#define jtime_usleep(usec)  usleep(usec)


/**
 * @brief   秒时间的变量类型
 */
typedef time_t jtime_t;

/**
 * @brief   分解时间的变量类型
 * @note    和linux的struct tm的变量时间是不同的
 */
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  wday;
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;
    uint16_t msec;
    uint16_t reserved;
} jtime_tm_t;

/**
 * @brief   秒+毫秒时间的变量类型
 */
typedef struct {
    jtime_t sec;
    uint16_t msec;
    uint16_t reserved;
} jtime_mt_t;

/**
 * @brief   秒+微秒时间的变量类型
 */
typedef struct {
    jtime_t sec;
    uint32_t usec;
} jtime_ut_t;

/**
 * @brief   秒+纳秒时间的变量类型
 */
typedef struct {
    jtime_t sec;
    uint32_t nsec;
} jtime_nt_t;

/* 内部接口：系统分解时间转自定义分解时间 */
static inline void jtime_tm_to_jtm(const struct tm *tm, jtime_tm_t *jtm)
{
    jtm->year   = tm->tm_year + 1900;
    jtm->month  = tm->tm_mon + 1;
    jtm->day    = tm->tm_mday;
    jtm->wday   = tm->tm_wday;
    jtm->hour   = tm->tm_hour;
    jtm->min    = tm->tm_min;
    jtm->sec    = tm->tm_sec;
}

/* 内部接口：自定义分解时间转系统分解时间 */
static inline void jtime_jtm_to_tm(const jtime_tm_t *jtm, struct tm *tm)
{
    tm->tm_year = jtm->year - 1900;
    tm->tm_mon  = jtm->month - 1;
    tm->tm_mday = jtm->day;
    tm->tm_wday = jtm->wday;
    tm->tm_hour = jtm->hour;
    tm->tm_min  = jtm->min;
    tm->tm_sec  = jtm->sec;
}

/**
 * @brief   UTC的秒+毫秒时间转换为分解时间
 * @param   jmt [IN] 秒+毫秒时间结构体(UTC)
 * @param   jtm [OUT] 分解时间结构体
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  无返回值
 * @note    要得到的jtm是本地分解时间时，才需要传入zone_sec；否则传入0得到UTC分解时间
 */
static inline void jtime_mtime_to_tm(const jtime_mt_t *jmt, jtime_tm_t *jtm, int zone_sec)
{
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    time_t sec = jmt->sec + zone_sec;
    gmtime_r(&sec, &tm);
    jtime_tm_to_jtm(&tm, jtm);
    jtm->msec = jmt->msec;
}

/**
 * @brief   分解时间转换为UTC的秒+毫秒时间
 * @param   jtm [IN] 分解时间结构体
 * @param   jmt [OUT] 秒+毫秒时间结构体(UTC)
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  无返回值
 * @note    如果传入的jtm是本地分解时间时才需要传入zone_sec；否则jtm是UTC分解时间传入0
 */
static inline void jtime_tm_to_mtime(const jtime_tm_t *jtm, jtime_mt_t *jmt, int zone_sec)
{
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    jtime_jtm_to_tm(jtm, &tm);
    jmt->sec = timegm(&tm) - zone_sec;
    jmt->msec = jtm->msec;
}

/**
 * @brief   获取本地时间和UTC时间的差值秒数
 * @param   无参数
 * @return  返回差值秒数
 * @note    无
 */
static inline int jtime_localutc_diff(void)
{
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    time_t day_sec = 86400;
    gmtime_r(&day_sec, &tm);
    return day_sec - mktime(&tm);
}

/**
 * @brief   本地分解时间转换为UTC的秒时间
 * @param   jtm [IN] 分解时间结构体
 * @return  返回UTC的秒时间
 * @note    无
 */
static inline jtime_t jtime_localtime_make(const jtime_tm_t *jtm)
{
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    jtime_jtm_to_tm(jtm, &tm);
    return mktime(&tm);
}

/**
 * @brief   UTC分解时间转换为UTC的秒时间
 * @param   jtm [IN] 分解时间结构体
 * @return  返回UTC的秒时间
 * @note    无
 */
static inline jtime_t jtime_utctime_make(const jtime_tm_t *jtm)
{
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    jtime_jtm_to_tm(jtm, &tm);
    return timegm(&tm);
}

/**
 * @brief   获取本地分解时间
 * @param   jtm [OUT] 分解时间结构体
 * @return  返回jtm的指针
 * @note    jtime_localtime_get获取的毫秒是精确值，耗时大概是jtime_localtime_geta的4倍
 */
static inline jtime_tm_t* jtime_localtime_get(jtime_tm_t *jtm)
{
    struct timeval tv;
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    jtime_tm_to_jtm(&tm, jtm);
    jtm->msec = JTIME_DIV_1E3(tv.tv_usec);

    return jtm;
}

/**
 * @brief   获取本地分解时间，毫秒是近似值
 * @param   jtm [OUT] 分解时间结构体
 * @return  返回jtm的指针
 * @note    jtime_localtime_geta获取的毫秒是近似值，耗时大概是jtime_localtime_get的1/4
 */
static inline jtime_tm_t* jtime_localtime_geta(jtime_tm_t *jtm)
{
    struct timespec ts;
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
    localtime_r(&ts.tv_sec, &tm);
    jtime_tm_to_jtm(&tm, jtm);
    jtm->msec = JTIME_DIV_1E6(ts.tv_nsec);

    return jtm;
}

/**
 * @brief   获取UTC分解时间
 * @param   jtm [OUT] 分解时间结构体
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  返回jtm的指针
 * @note    如果zone_sec传入值，则分解时间也加上了时区偏移
 *          jtime_utctime_get获取的毫秒是精确值，耗时大概是jtime_utctime_geta的4倍
 */
static inline jtime_tm_t* jtime_utctime_get(jtime_tm_t *jtm, int zone_sec)
{
    struct timeval tv;
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    gettimeofday(&tv, NULL);
    time_t sec = tv.tv_sec + zone_sec;
    gmtime_r(&sec, &tm);
    jtime_tm_to_jtm(&tm, jtm);
    jtm->msec = JTIME_DIV_1E3(tv.tv_usec);

    return jtm;
}

/**
 * @brief   获取UTC分解时间，毫秒是近似值
 * @param   jtm [OUT] 分解时间结构体
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  返回jtm的指针
 * @note    如果zone_sec传入值，则分解时间也加上了时区偏移
 *          jtime_utctime_geta获取的毫秒是近似值，耗时大概是jtime_utctime_get的1/4
 */
static inline jtime_tm_t* jtime_utctime_geta(jtime_tm_t *jtm, int zone_sec)
{
    struct timespec ts;
#ifdef __cplusplus
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
#else
    struct tm tm = {0};
#endif
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
    ts.tv_sec += zone_sec;
    gmtime_r(&ts.tv_sec, &tm);
    jtime_tm_to_jtm(&tm, jtm);
    jtm->msec = JTIME_DIV_1E6(ts.tv_nsec);

    return jtm;
}

/**
 * @brief   从本地分解时间设置系统时间
 * @param   jtm [IN] 分解时间结构体
 * @return  成功返回0，失败返回-1
 * @note    无
 */
static inline int jtime_localtime_set(const jtime_tm_t *jtm)
{
    struct timeval tv;

    tv.tv_sec = jtime_localtime_make(jtm);
    tv.tv_usec = jtm->msec * 1000;
    return settimeofday(&tv, NULL);
}

/**
 * @brief   从UTC分解时间设置系统时间
 * @param   jtm [IN] 分解时间结构体
 * @return  成功返回0，失败返回-1
 * @note    无
 */
static inline int jtime_utctime_set(const jtime_tm_t *jtm)
{
    struct timeval tv;

    tv.tv_sec = jtime_utctime_make(jtm);
    tv.tv_usec = jtm->msec * 1000;
    return settimeofday(&tv, NULL);
}

/**
 * @brief   获取UTC时间秒数
 * @param   无参数
 * @return  返回时间秒数
 * @note    无
 */
static inline jtime_t jtime_utcsec_get(void)
{
    jtime_t sec = time(NULL);
    return sec;
}

/**
 * @brief   获取UTC时间毫秒数
 * @param   无参数
 * @return  返回时间毫秒数
 * @note    jtime_utcmsec_get获取精确值，耗时大概是jtime_utcmsec_geta的4倍
 */
static inline uint64_t jtime_utcmsec_get(void)
{
    uint64_t msec = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    msec = tv.tv_sec * 1000ULL + JTIME_DIV_1E3(tv.tv_usec);
    return msec;
}

/**
 * @brief   获取UTC时间毫秒数近似值
 * @param   无参数
 * @return  返回时间毫秒数
 * @note    jtime_utcmsec_geta获取近似值，耗时大概是jtime_utcmsec_get的1/4
 */
static inline uint64_t jtime_utcmsec_geta(void)
{
    uint64_t msec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
    msec = ts.tv_sec * 1000ULL + JTIME_DIV_1E6(ts.tv_nsec);
    return msec;
}

/**
 * @brief   获取UTC时间微秒数
 * @param   无参数
 * @return  返回时间微秒数
 * @note    无
 */
static inline uint64_t jtime_utcusec_get(void)
{
    uint64_t usec = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    usec = tv.tv_sec * 1000000ULL + tv.tv_usec;
    return usec;
}

/**
 * @brief   获取UTC时间纳秒数
 * @param   无参数
 * @return  返回时间纳秒数
 * @note    无
 */
static inline uint64_t jtime_utcnsec_get(void)
{
    uint64_t nsec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    nsec = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return nsec;
}

/**
 * @brief   获取系统启动后的毫秒数
 * @param   无参数
 * @return  返回时间毫秒数
 * @note    无
 */
static inline uint64_t jtime_clockmsec_get(void)
{
    uint64_t msec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    msec = ts.tv_sec * 1000ULL + JTIME_DIV_1E6(ts.tv_nsec);
    return msec;
}

/**
 * @brief   获取系统启动后的微秒数
 * @param   无参数
 * @return  返回时间微秒数
 * @note    无
 */
static inline uint64_t jtime_clockusec_get(void)
{
    uint64_t usec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    usec = ts.tv_sec * 1000000ULL + JTIME_DIV_1E3(ts.tv_nsec);
    return usec;
}

/**
 * @brief   获取系统启动后的纳秒数
 * @param   无参数
 * @return  返回时间纳秒数
 * @note    无
 */
static inline uint64_t jtime_clocknsec_get(void)
{
    uint64_t nsec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    nsec = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return nsec;
}

/**
 * @brief   获取UTC的秒+毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    jtime_utcmtime_get获取精确值，耗时大概是jtime_utcmtime_geta的4倍
 */
static inline void jtime_utcmtime_get(jtime_mt_t *jmt)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    jmt->sec = tv.tv_sec;
    jmt->msec = JTIME_DIV_1E3(tv.tv_usec);
}

/**
 * @brief   获取UTC的秒+近似毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    jtime_utcmtime_geta获取近似值，耗时大概是jtime_utcmtime_get的1/4
 */
static inline void jtime_utcmtime_geta(jtime_mt_t *jmt)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
    jmt->sec = ts.tv_sec;
    jmt->msec = JTIME_DIV_1E6(ts.tv_nsec);
}

/**
 * @brief   获取UTC的秒+微秒时间
 * @param   jut [OUT] 秒+微秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_utcutime_get(jtime_ut_t *jut)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    jut->sec = tv.tv_sec;
    jut->usec = tv.tv_usec;
}

/**
 * @brief   获取UTC的秒+纳秒时间
 * @param   jnt [OUT] 秒+纳秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_utcntime_get(jtime_nt_t *jnt)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    jnt->sec = ts.tv_sec;
    jnt->nsec = ts.tv_nsec;
}

/**
 * @brief   获取clock的秒+毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_clockmtime_get(jtime_mt_t *jmt)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    jmt->sec = ts.tv_sec;
    jmt->msec = JTIME_DIV_1E6(ts.tv_nsec);
}

/**
 * @brief   获取clock的秒+微秒时间
 * @param   jut [OUT] 秒+微秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_clockutime_get(jtime_ut_t *jut)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    jut->sec = ts.tv_sec;
    jut->usec = JTIME_DIV_1E3(ts.tv_nsec);
}

/**
 * @brief   获取clock的秒+纳秒时间
 * @param   jnt [OUT] 秒+纳秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_clockntime_get(jtime_nt_t *jnt)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    jnt->sec = ts.tv_sec;
    jnt->nsec = ts.tv_nsec;
}

/**
 * @brief   更新秒+纳秒时间结构体
 * @param   jnt [INOUT] 秒+纳秒时间结构体
 * @param   msec [IN] 要增加的毫秒数
 * @return  无返回值
 * @note    无
 */
static inline void jtime_ntime_madd(jtime_nt_t *jnt, uint32_t msec)
{
    int sec = JTIME_DIV_1E3(msec);
    jnt->sec += sec;
    jnt->nsec += (msec - sec * 1000) * 1000000;
    if (jnt->nsec >= 1000000000) {
        jnt->nsec -= 1000000000;
        jnt->sec += 1;
    }
}

#ifdef __cplusplus
}
#endif
