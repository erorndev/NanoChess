#pragma once

#include "types.h"

struct Zobrist {
    uint64_t pieces[12][64];
    uint64_t side;
    uint64_t castling[16];
    uint64_t ep[8];
};

extern struct Zobrist zobrist;

void init_zobrist(void);
