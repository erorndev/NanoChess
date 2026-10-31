#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include(<stdbit.h>)
#include <stdbit.h>
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#elif defined(_MSC_VER) && defined(__clang__)
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif
#include <intrin.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <time.h>
#include <unistd.h>
#endif

#if !defined(__STDC_NO_THREADS__) && defined(__has_include) && __has_include(<threads.h>)
#include <threads.h>
typedef thrd_t thread_t;
typedef thrd_start_t thread_fn_t;
static inline bool thread_create(thread_t *t, thread_fn_t fn, void *arg) {
    return thrd_create(t, fn, arg) == thrd_success;
}
static inline void thread_join(thread_t t) {
    thrd_join(t, NULL);
}
#elif defined(_WIN32)
typedef HANDLE thread_t;
typedef int (*thread_fn_t)(void *);
static inline bool thread_create(thread_t *t, thread_fn_t fn, void *arg) {
    *t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(void *)fn, arg, 0, NULL);
    return *t != NULL;
}
static inline void thread_join(thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}
#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef int (*thread_fn_t)(void *);
static inline bool thread_create(thread_t *t, thread_fn_t fn, void *arg) {
    return pthread_create(t, NULL, (void *(*)(void *))(void *)fn, arg) == 0;
}
static inline void thread_join(thread_t t) {
    pthread_join(t, NULL);
}
#endif

[[maybe_unused]] static const uint8_t DEBRUIJN_64[64] = {
    0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 34, 28, 16, 3,  61, 54, 58, 38, 50, 42, 21,
    44, 35, 32, 29, 18, 17, 10, 4,  12, 62, 46, 55, 26, 59, 40, 33, 15, 53, 37, 20, 43, 31,
    19, 9,  11, 45, 25, 39, 14, 36, 13, 30, 8,  24, 7,  23, 6,  22, 5,  63, 0,  0};

[[nodiscard]] static inline int popcount64(uint64_t bb) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) && defined(stdc_count_ones_ull)
    return (int)stdc_count_ones_ull(bb);
#elif defined(__clang__) || defined(__GNUC__)
    return __builtin_popcountll(bb);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    return (int)__popcnt64(bb);
#elif defined(_MSC_VER) && defined(_M_IX86)
    return (int)(__popcnt((uint32_t)bb) + __popcnt((uint32_t)(bb >> 32)));
#else
    bb -= (bb >> 1) & 0x5555555555555555ULL;
    bb = (bb & 0x3333333333333333ULL) + ((bb >> 2) & 0x3333333333333333ULL);
    bb = (bb + (bb >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((bb * 0x0101010101010101ULL) >> 56);
#endif
}

[[nodiscard]] static inline int lsb64(uint64_t bb) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) && defined(stdc_trailing_zeros_ull)
    return (int)stdc_trailing_zeros_ull(bb);
#elif defined(__clang__) || defined(__GNUC__)
    return __builtin_ctzll(bb);
#elif defined(_MSC_VER) && defined(_M_X64)
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return (int)idx;
#elif defined(_MSC_VER) && defined(_M_IX86)
    unsigned long idx;
    if ((uint32_t)bb) {
        _BitScanForward(&idx, (uint32_t)bb);
        return (int)idx;
    }
    _BitScanForward(&idx, (uint32_t)(bb >> 32));
    return (int)(idx + 32);
#else
    return (int)DEBRUIJN_64[((bb & -(int64_t)bb) * 0x03f79d714e081a3fULL) >> 58];
#endif
}

[[nodiscard]] static inline int msb64(uint64_t bb) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) && defined(stdc_leading_zeros_ull)
    return (int)(63 - stdc_leading_zeros_ull(bb));
#elif defined(__clang__) || defined(__GNUC__)
    return 63 - __builtin_clzll(bb);
#elif defined(_MSC_VER) && defined(_M_X64)
    unsigned long idx;
    _BitScanReverse64(&idx, bb);
    return (int)idx;
#else
    int r = 0;
    if (bb > 0xFFFFFFFFULL) {
        bb >>= 32;
        r += 32;
    }
    if (bb > 0xFFFFULL) {
        bb >>= 16;
        r += 16;
    }
    if (bb > 0xFFULL) {
        bb >>= 8;
        r += 8;
    }
    if (bb > 0xFULL) {
        bb >>= 4;
        r += 4;
    }
    if (bb > 0x3ULL) {
        bb >>= 2;
        r += 2;
    }
    if (bb > 0x1ULL) {
        r += 1;
    }
    return r;
#endif
}
