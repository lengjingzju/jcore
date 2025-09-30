/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#pragma once
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <windows.h>

/*
 * 本接口提供三种时钟：
 * 1. utc: UTC时钟(日历时钟)，从1970-01-01 00:00:00 UTC开始计数
 * 2. local: 本地时钟(日历时钟)，UTC加上时区偏移
 * 3. mono: 单调时钟，从系统启动开始计算时间
 */

#ifdef __cplusplus
extern "C" {
#endif

#define JTIME_DIV_1E3(N)    (((uint64_t)(N) * 274877907) >> 38) // N <= 4294967295
#define JTIME_DIV_1E6(N)    (((uint64_t)(N) * 1125899907) >> 50) // N <= 4294967295

/**
 * @brief   毫秒级睡眠
 */
#define jtime_msleep(msec)  Sleep(msec)

/**
 * @brief   微秒级睡眠
 */
static inline int jtime_usleep(unsigned int usec)
{
    HANDLE hd = CreateWaitableTimerA(NULL, TRUE, NULL);
    if (!hd)
        return -1;

    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)(usec * 10); // 将微秒转换为100纳秒单位(负数表示相对时间)
    SetWaitableTimer(hd, &li, 0, NULL, NULL, FALSE);
    WaitForSingleObject(hd, INFINITE);
    CloseHandle(hd);
    return 0;
}

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
static inline void jtime_tm_to_jtm(const SYSTEMTIME *tm, jtime_tm_t *jtm)
{
    jtm->year           = tm->wYear;
    jtm->month          = (uint8_t)tm->wMonth;
    jtm->day            = (uint8_t)tm->wDay;
    jtm->wday           = (uint8_t)tm->wDayOfWeek;
    jtm->hour           = (uint8_t)tm->wHour;
    jtm->min            = (uint8_t)tm->wMinute;
    jtm->sec            = (uint8_t)tm->wSecond;
    jtm->msec           = (uint16_t)tm->wMilliseconds;
}

/* 内部接口：自定义分解时间转系统分解时间 */
static inline void jtime_jtm_to_tm(const jtime_tm_t *jtm, SYSTEMTIME *tm)
{
    tm->wYear           = jtm->year;
    tm->wMonth          = jtm->month;
    tm->wDay            = jtm->day;
    tm->wDayOfWeek      = jtm->wday;
    tm->wHour           = jtm->hour;
    tm->wMinute         = jtm->min;
    tm->wSecond         = jtm->sec;
    tm->wMilliseconds   = jtm->msec;
}

/* 内部接口：获取 100ns 为单位的UTC时间 */
static inline uint64_t jtime_utc100nsec_get(void)
{
    FILETIME ft;
    ULARGE_INTEGER uli;

    /* FILETIME结构体存储从1601-01-01 00:00:00起算的100纳秒为单位的时间，而UTC从1970-01-01 00:00:00起算 */
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart - 116444736000000000ULL;
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
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER uli;

    uli.QuadPart = (jmt->sec + zone_sec) * 10000000ULL + jmt->msec * 10000ULL + 116444736000000000ULL;
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    FileTimeToSystemTime(&ft, &st);
    jtime_tm_to_jtm(&st, jtm);
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
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER uli;

    jtime_jtm_to_tm(jtm, &st);
    SystemTimeToFileTime(&st, &ft);
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart = (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;

    jmt->sec  = (uint64_t)uli.QuadPart - zone_sec;
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
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        return -(tzi.Bias * 60);
    }
    return 0;
}

/**
 * @brief   本地分解时间转换为UTC的秒时间
 * @param   jtm [IN] 分解时间结构体
 * @return  返回UTC的秒时间
 * @note    无
 */
static inline jtime_t jtime_localtime_make(const jtime_tm_t *jtm)
{
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER uli;

    jtime_jtm_to_tm(jtm, &st);
    SystemTimeToFileTime(&st, &ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
}

/**
 * @brief   UTC分解时间转换为UTC的秒时间
 * @param   jtm [IN] 分解时间结构体
 * @return  返回UTC的秒时间
 * @note    无
 */
static inline jtime_t jtime_utctime_make(const jtime_tm_t *jtm)
{
    return jtime_localtime_make(jtm) - jtime_localutc_diff();
}

/**
 * @brief   获取本地分解时间
 * @param   jtm [OUT] 分解时间结构体
 * @return  返回jtm的指针
 * @note    无
 */
static inline jtime_tm_t* jtime_localtime_get(jtime_tm_t *jtm)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    jtime_tm_to_jtm(&st, jtm);
    return jtm;
}

/**
 * @brief   获取本地分解时间，毫秒是近似值
 * @param   jtm [OUT] 分解时间结构体
 * @return  返回jtm的指针
 * @note    Windows无直接对应COARSE的获取近似时间版本
 */
static inline jtime_tm_t* jtime_localtime_geta(jtime_tm_t *jtm)
{
    return jtime_localtime_get(jtm);
}

/**
 * @brief   获取UTC分解时间
 * @param   jtm [OUT] 分解时间结构体
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  返回jtm的指针
 * @note    如果zone_sec传入值，则分解时间也加上了时区偏移
 */
static inline jtime_tm_t* jtime_utctime_get(jtime_tm_t *jtm, int zone_sec)
{
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart += zone_sec * 10000000ULL;

    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    FileTimeToSystemTime(&ft, &st);
    jtime_tm_to_jtm(&st, jtm);

    return jtm;
}

/**
 * @brief   获取UTC分解时间，毫秒是近似值
 * @param   jtm [OUT] 分解时间结构体
 * @param   zone_sec [IN] 时区带来的偏移秒数
 * @return  返回jtm的指针
 * @note    如果zone_sec传入值，则分解时间也加上了时区偏移
 *          Windows无直接对应COARSE的获取近似时间版本
 */
static inline jtime_tm_t* jtime_utctime_geta(jtime_tm_t *jtm, int zone_sec)
{
    return jtime_utctime_get(jtm, zone_sec);
}

/**
 * @brief   从本地分解时间设置系统日历时间
 * @param   jtm [IN] 分解时间结构体
 * @return  成功返回0，失败返回-1
 * @note    无
 */
static inline int jtime_localtime_set(const jtime_tm_t *jtm)
{
    SYSTEMTIME st;
    jtime_jtm_to_tm(jtm, &st);
    return SetLocalTime(&st) ? 0 : -1;
}

/**
 * @brief   从UTC分解时间设置系统日历时间
 * @param   jtm [IN] 分解时间结构体
 * @return  成功返回0，失败返回-1
 * @note    无
 */
static inline int jtime_utctime_set(const jtime_tm_t *jtm)
{
    SYSTEMTIME st;
    jtime_jtm_to_tm(jtm, &st);
    return SetSystemTime(&st) ? 0 : -1;
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
 * @note    无
 */
static inline uint64_t jtime_utcmsec_get(void)
{
    return jtime_utc100nsec_get() / 10000ULL;
}

/**
 * @brief   获取UTC时间毫秒数近似值
 * @param   无参数
 * @return  返回时间毫秒数
 * @note    Windows无直接对应COARSE的获取近似时间版本
 */
static inline uint64_t jtime_utcmsec_geta(void)
{
    return jtime_utcmsec_get();
}

/**
 * @brief   获取UTC时间微秒数
 * @param   无参数
 * @return  返回时间微秒数
 * @note    无
 */
static inline uint64_t jtime_utcusec_get(void)
{
    return jtime_utc100nsec_get() / 10ULL;
}

/**
 * @brief   获取UTC时间纳秒数
 * @param   无参数
 * @return  返回时间纳秒数
 * @note    无
 */
static inline uint64_t jtime_utcnsec_get(void)
{
    return jtime_utc100nsec_get() * 100ULL;
}

/**
 * @brief   获取系统启动后的毫秒数
 * @param   无参数
 * @return  返回时间毫秒数
 * @note    无
 */
static inline uint64_t jtime_monomsec_get(void)
{
    return GetTickCount64();
}

/**
 * @brief   获取系统启动后的微秒数
 * @param   无参数
 * @return  返回时间微秒数
 * @note    无
 */
static inline uint64_t jtime_monousec_get(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000ULL) / freq.QuadPart;
}

/**
 * @brief   获取系统启动后的纳秒数
 * @param   无参数
 * @return  返回时间纳秒数
 * @note    无
 */
static inline uint64_t jtime_mononsec_get(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000000ULL) / freq.QuadPart;
}

/**
 * @brief   获取UTC的秒+毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_utcmtime_get(jtime_mt_t *jmt)
{
    uint64_t ns100 = jtime_utc100nsec_get();
    jmt->sec = ns100 / 1000000ULL;
    jmt->msec = (uint16_t)((ns100 % 1000000ULL) / 10000);
}

/**
 * @brief   获取UTC的秒+近似毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    Windows无直接对应COARSE的获取近似时间版本
 */
static inline void jtime_utcmtime_geta(jtime_mt_t *jmt)
{
    jtime_utcmtime_get(jmt);
}

/**
 * @brief   获取UTC的秒+微秒时间
 * @param   jut [OUT] 秒+微秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_utcutime_get(jtime_ut_t *jut)
{
    uint64_t ns100 = jtime_utc100nsec_get();
    jut->sec = ns100 / 1000000ULL;
    jut->usec = (ns100 % 1000000ULL) / 10;
}

/**
 * @brief   获取UTC的秒+纳秒时间
 * @param   jnt [OUT] 秒+纳秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_utcntime_get(jtime_nt_t *jnt)
{
    uint64_t ns100 = jtime_utc100nsec_get();
    jnt->sec = ns100 / 10000000ULL;
    jnt->nsec = (ns100 % 10000000ULL) * 100ULL;
}


/**
 * @brief   获取系统启动后的秒+毫秒时间
 * @param   jmt [OUT] 秒+毫秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_monomtime_get(jtime_mt_t *jmt)
{
    uint64_t msec = jtime_monomsec_get();
    jmt->sec = msec / 1000;
    jmt->msec = msec % 1000;
}

/**
 * @brief   获取系统启动后的秒+微秒时间
 * @param   jut [OUT] 秒+微秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_monoutime_get(jtime_ut_t *jut)
{
    uint64_t usec = jtime_monousec_get();
    jut->sec = usec / 1000000;
    jut->usec = usec % 1000000;
}

/**
 * @brief   获取系统启动后的秒+纳秒时间
 * @param   jnt [OUT] 秒+纳秒时间结构体
 * @return  无返回值
 * @note    无
 */
static inline void jtime_monontime_get(jtime_nt_t *jnt)
{
    uint64_t nsec = jtime_mononsec_get();
    jnt->sec = nsec / 1000000000;
    jnt->nsec = nsec % 1000000000;
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
    if (msec >= 1000) {
        int sec = JTIME_DIV_1E3(msec);
        msec -= sec * 1000;
        jnt->sec += sec;
    }
    jnt->nsec += msec * 1000000;
    if (jnt->nsec >= 1000000000) {
        jnt->nsec -= 1000000000;
        jnt->sec += 1;
    }
}

/**
 * @brief   更新秒+纳秒时间结构体
 * @param   jnt [INOUT] 秒+纳秒时间结构体
 * @param   msec [IN] 要增加的纳秒数
 * @return  无返回值
 * @note    无
 */
static inline void jtime_ntime_nadd(jtime_nt_t *jnt, uint64_t nsec)
{
    if (nsec >= 1000000000) {
        uint64_t sec = nsec / 1000000000;
        nsec -= sec * 1000000000;
        jnt->sec += sec;
    }
    jnt->nsec += (uint32_t)nsec;
    if (jnt->nsec >= 1000000000) {
        jnt->nsec -= 1000000000;
        jnt->sec += 1;
    }
}

/**
 * @brief   获取当前时间延时后的时间
 * @param   jnt [OUT] 保存延时后的时间
 * @param   msec [IN] 延时毫秒数
 * @param   mono_clock [IN] 时钟定义：UTC日历时钟，1: 系统单调时钟
 * @return  返回延时后的时间
 * @note    无
 */
static inline void jtime_ntime_after(jtime_nt_t *jnt, uint32_t msec, int mono_clock)
{
    uint64_t nsec = mono_clock ? jtime_mononsec_get() : jtime_utcnsec_get();
    nsec += msec * 1000000ll;
    jnt->sec = nsec / 1000000000;
    jnt->nsec = nsec % 1000000000;
}

#ifdef __cplusplus
}
#endif
