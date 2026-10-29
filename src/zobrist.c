#include "zobrist.h"

struct Zobrist zobrist;

static uint64_t prng_state = 0x987654321ULL;

static inline uint64_t rand64(void) {
    prng_state ^= prng_state >> 12;
    prng_state ^= prng_state << 25;
    prng_state ^= prng_state >> 27;
    return prng_state * 0x2545F4914F6CDD1DULL;
}

void init_zobrist(void) {
    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            zobrist.pieces[p][sq] = rand64();
        }
    }
    zobrist.side = rand64();
    for (int c = 0; c < 16; ++c) {
        zobrist.castling[c] = rand64();
    }
    for (int f = 0; f < 8; ++f) {
        zobrist.ep[f] = rand64();
    }
}
