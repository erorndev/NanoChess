#include "time.h"
#include "platform.h"

int option_move_overhead = 15;

uint64_t get_time_ms(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = true;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

void sleep_ms(unsigned ms) {
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
#endif
}

struct SearchLimits allocate_time(const struct Board *b, uint64_t time_left, uint64_t inc, int movestogo) {
    struct SearchLimits lim = {0};
    if (time_left == 0)
        return lim;

    const uint64_t overhead = (uint64_t)option_move_overhead;
    uint64_t usable = (time_left > overhead) ? (time_left - overhead) : (time_left * 3 / 4);
    if (usable < 1)
        usable = 1;

    int pieces = popcount64(b->all);
    int pawns = popcount64(b->pieces[W_PAWN] | b->pieces[B_PAWN]);
    int t_base = (time_left < 30000 ? 16 : 20);
    int moves = (movestogo > 0 && movestogo <= 50)
                    ? movestogo
                    : (t_base + (pieces * t_base) / 32 + (pawns - pieces / 2));
    if (moves < 12)
        moves = 12;

    uint64_t base = usable / (uint64_t)moves;
    uint64_t scale_num = (movestogo > 0) ? 16 : (16 + (uint64_t)((32 - pieces) / 3));
    uint64_t optimum = (base * scale_num) / 16 + (inc * 4 / 5);

    if (!inc && pieces <= 8 && optimum > usable / 20)
        optimum = usable / 20;

    if (time_left < 1500) {
        uint64_t cap = usable * 4 / 10 + inc * 7 / 10;
        if (optimum > cap)
            optimum = cap;
    }

    if (optimum > usable * 8 / 10)
        optimum = usable * 8 / 10;
    if (optimum < 1)
        optimum = 1;

    uint64_t hard = (time_left < 1500) ? (usable / 2) : (usable * 3 / 4);
    uint64_t hard_scale = (!inc && time_left < 10000)
                              ? (optimum * 7 / 4)
                              : (optimum * 5 / 2 + inc * 4 / 5);
    if (hard_scale < hard)
        hard = hard_scale;
    if (hard < optimum)
        hard = optimum;
    if (hard > usable)
        hard = usable;
    if (hard < 1)
        hard = 1;

    lim.soft_time = optimum;
    lim.hard_time = hard;
    return lim;
}
