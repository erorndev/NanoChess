#pragma once

#include "types.h"

struct Magic {
    bitboard_t *attacks;
    bitboard_t mask;
    uint64_t magic;
    int shift;
};

extern bitboard_t pawn_attacks[COLOR_NB][64];
extern bitboard_t knight_attacks[64];
extern bitboard_t king_attacks[64];
extern struct Magic bishop_magics[64];
extern struct Magic rook_magics[64];

void init_attacks(void);

[[nodiscard]] static inline bitboard_t bishop_attacks(enum Square sq, bitboard_t occ) {
    const struct Magic *m = &bishop_magics[sq];
    return m->attacks[((occ & m->mask) * m->magic) >> m->shift];
}

[[nodiscard]] static inline bitboard_t rook_attacks(enum Square sq, bitboard_t occ) {
    const struct Magic *m = &rook_magics[sq];
    return m->attacks[((occ & m->mask) * m->magic) >> m->shift];
}

[[nodiscard]] static inline bitboard_t queen_attacks(enum Square sq, bitboard_t occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}

[[nodiscard]] static inline bitboard_t piece_attacks(enum PieceType pt, enum Square sq,
                                                     bitboard_t occ) {
    switch (pt) {
    case KNIGHT:
        return knight_attacks[sq];
    case BISHOP:
        return bishop_attacks(sq, occ);
    case ROOK:
        return rook_attacks(sq, occ);
    case QUEEN:
        return queen_attacks(sq, occ);
    case KING:
        return king_attacks[sq];
    default:
        return 0;
    }
}
