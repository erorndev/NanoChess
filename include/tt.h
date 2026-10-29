#pragma once

#include "types.h"
#include <stddef.h>

enum TTFlag { TT_NONE = 0, TT_EXACT = 1, TT_LOWER = 2, TT_UPPER = 3 };

struct TTEntry {
    uint64_t key;
    move_t move;
    int16_t score;
    int16_t eval;
    uint8_t depth;
    uint8_t flag_age;
};

void init_tt(size_t mb);
void free_tt(void);
void clear_tt(void);
void step_tt_age(void);

[[nodiscard]] bool probe_tt(uint64_t key, struct TTEntry *out);
void store_tt(uint64_t key, int depth, int ply, int score, int eval, enum TTFlag flag, move_t move);

[[nodiscard]] int score_to_tt(int score, int ply);
[[nodiscard]] int score_from_tt(int score, int ply);
