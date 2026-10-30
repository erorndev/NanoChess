#include "evaluate.h"
#include "attacks.h"
#include "platform.h"

#include <stdlib.h>

#define S(mg, eg) ((int32_t)((uint32_t)(eg) << 16) + (mg))

static inline int16_t mg_val(int32_t s) {
    return (int16_t)s;
}

static inline int16_t eg_val(int32_t s) {
    return (int16_t)((uint32_t)(s + 0x8000) >> 16);
}

// clang-format off
static const int mg_piece_val[6] = { 82, 336, 363, 477, 1025, 0 };
static const int eg_piece_val[6] = { 94, 277, 297, 512,  936, 0 };
static const int phase_weight[6] = {  0,   1,   1,   2,    4, 0 };

static const int16_t mg_table[6][64] = {
    { // PAWN
          0,   0,   0,   0,   0,   0,   0,   0,
         98, 134,  61,  95,  68, 126,  34, -11,
         -6,   7,  26,  31,  65,  56,  25, -20,
        -14,  13,   6,  21,  23,  12,  17, -23,
        -27,  -2,  -5,  12,  17,   6,  10, -25,
        -26,  -4,  -4, -10,   3,   3,  33, -12,
        -35,  -1, -20, -23, -15,  24,  38, -22,
          0,   0,   0,   0,   0,   0,   0,   0
    },
    { // KNIGHT
        -167, -89, -34, -49,  61, -97, -15, -107,
         -73, -41,  72,  36,  23,  62,   7,  -17,
         -47,  60,  37,  65,  84, 129,  73,   44,
          -9,  17,  19,  53,  37,  69,  18,   22,
         -13,   4,  16,  13,  28,  19,  21,   -8,
         -23,  -9,  12,  10,  19,  17,  25,  -16,
         -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23
    },
    { // BISHOP
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21
    },
    { // ROOK
         32,  42,  32,  51,  63,   9,  31,  43,
         27,  32,  58,  62,  80,  67,  26,  44,
         -5,  19,  26,  36,  17,  45,  61,  16,
        -24, -11,   7,  26,  24,  35,  -8, -20,
        -36, -26, -12,  -1,   9,  -7,   6, -23,
        -45, -25, -16, -17,   3,   0,  -5, -33,
        -44, -16, -20,  -9,  -1,  11,  -6, -71,
        -19, -13,   1,  17,  16,   7, -37, -26
    },
    { // QUEEN
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50
    },
    { // KING
        -65,  23,  16, -15, -56, -34,   2,  13,
         29,  -1, -20,  -7,  -8,  -4, -38, -29,
         -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
          1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14
    }
};

static const int16_t eg_table[6][64] = {
    { // PAWN
          0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
         94, 100,  85,  67,  56,  53,  82,  84,
         32,  24,  13,   5,  -2,   4,  17,  17,
         13,   9,  -3,  -7,  -7,  -8,   3,  -1,
          4,   7,  -6,   1,   0,  -5,  -1,  -8,
         13,   8,   8,  10,  13,   0,   2,  -7,
          0,   0,   0,   0,   0,   0,   0,   0
    },
    { // KNIGHT
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64
    },
    { // BISHOP
        -14, -21, -11,  -8,  -7,  -9, -17, -24,
         -8,  -4,   7, -12,  -3, -13,  -4, -14,
          2,  -8,   0,  -1,  -2,   6,   0,   4,
         -3,   9,  12,   9,  14,  10,   3,   2,
         -6,   3,  13,  19,   7,  10,  -3,  -9,
        -12,  -3,   8,  10,  13,   3,  -7, -15,
        -14, -18,  -7,  -1,   4,  -9, -15, -27,
        -23,  -9, -23,  -5,  -9, -16,  -5, -17
    },
    { // ROOK
         13,  10,  18,  15,  12,  12,   8,   5,
         11,  13,  13,  11,  -3,   3,   8,   3,
          7,   7,   7,   5,   4,  -3,  -5,  -3,
          4,   3,  13,   1,   2,   1,  -1,   2,
          3,   5,   8,   4,  -5,  -6,  -8, -11,
         -4,   0,  -5,  -1,  -7, -12,  -8, -16,
         -6,  -6,   0,   2,  -9,  -9, -11,  -3,
         -9,   2,   3,  -1,  -5, -13,   4, -20
    },
    { // QUEEN
         -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41
    },
    { // KING
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
         10,  17,  23,  15,  20,  45,  44,  13,
         -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
    }
};

static const int32_t passed_rank_bonus[8] = {
    S(0, 0), S(5, 11), S(12, 25), S(24, 52), S(49, 85), S(84, 139), S(136, 218), S(0, 0)
};

static const int32_t passed_prot_bonus[8] = {
    S(0, 0), S(3, 6), S(6, 9), S(12, 18), S(21, 31), S(32, 48), S(48, 72), S(0, 0)
};

static const int32_t knight_mob[9] = {
    S(-36, -40), S(-21, -24), S(-9, -10), S(0, -1), S(6, 7), S(13, 14), S(17, 20), S(21, 24), S(24, 24)
};

static const int32_t bishop_mob[14] = {
    S(-41, -44), S(-24, -28), S(-11, -14), S(-4, -6), S(2, 3),   S(6, 7),   S(13, 14),
    S(18, 20),   S(21, 25),   S(26, 29),   S(29, 33), S(32, 34), S(34, 37), S(36, 38)
};

static const int32_t rook_mob[15] = {
    S(-28, -36), S(-14, -24), S(-10, -14), S(-5, -6), S(0, 0),   S(4, 6),   S(7, 12), S(11, 17),
    S(13, 22),   S(17, 26),   S(19, 31),   S(21, 33), S(23, 36), S(25, 38), S(26, 41)
};

static const int32_t queen_mob[28] = {
    S(-24, -36), S(-16, -26), S(-10, -18), S(-7, -12), S(-3, -6), S(1, 0),   S(2, 4),
    S(4, 8),     S(6, 12),    S(7, 16),    S(10, 20),  S(12, 23), S(15, 26), S(15, 29),
    S(16, 32),   S(17, 34),   S(18, 36),   S(19, 38),  S(20, 40), S(21, 41), S(22, 42),
    S(23, 43),   S(24, 44),   S(25, 45),   S(26, 46),  S(26, 47), S(27, 48), S(27, 48)
};

static const int32_t pawn_phalanx_bonus    = S(9, 7);
static const int32_t pawn_supported_bonus  = S(13, 10);
static const int32_t pawn_doubled_penalty  = S(11, 22);
static const int32_t pawn_isolated_penalty = S(12, 6);
static const int32_t passed_free_bonus     = S(10, 29);
static const int32_t passed_blocked_pen    = S(7, 14);
static const int32_t knight_outpost_bonus  = S(24, 14);
static const int32_t bishop_outpost_bonus  = S(15, 10);
static const int32_t bishop_pair_bonus     = S(32, 54);
static const int32_t rook_open_bonus       = S(26, 14);
static const int32_t rook_semi_bonus       = S(13, 7);
static const int32_t rook_seventh_bonus    = S(21, 30);
static const int32_t pawn_threat_minor     = S(32, 24);
static const int32_t pawn_threat_major     = S(44, 32);
static const int32_t minor_threat_major    = S(28, 18);
static const int32_t king_shield_bonus     = S(9, 0);
static const int32_t king_open_penalty     = S(17, 0);
static const int32_t tempo_bonus           = S(18, 9);

static const int king_atk_weight[6] = { 0, 2, 2, 3, 5, 0 };
// clang-format on

static int32_t psqt[12][64];
static bitboard_t file_mask[8];
static bitboard_t adj_files_mask[8];
static bitboard_t passed_mask[COLOR_NB][64];
static bitboard_t outpost_mask[COLOR_NB][64];
static bitboard_t king_zone_mask[COLOR_NB][64];

void init_eval(void) {
    for (int f = 0; f < 8; ++f) {
        file_mask[f] = 0x0101010101010101ULL << f;
        adj_files_mask[f] = 0;
        if (f > 0)
            adj_files_mask[f] |= 0x0101010101010101ULL << (f - 1);
        if (f < 7)
            adj_files_mask[f] |= 0x0101010101010101ULL << (f + 1);
    }

    for (int pt = PAWN; pt <= KING; ++pt) {
        for (int sq = 0; sq < 64; ++sq) {
            int w_mg = mg_piece_val[pt] + mg_table[pt][sq ^ 56];
            int w_eg = eg_piece_val[pt] + eg_table[pt][sq ^ 56];
            psqt[make_piece(WHITE, (enum PieceType)pt)][sq] = S(w_mg, w_eg);

            int b_mg = mg_piece_val[pt] + mg_table[pt][sq];
            int b_eg = eg_piece_val[pt] + eg_table[pt][sq];
            psqt[make_piece(BLACK, (enum PieceType)pt)][sq] = S(b_mg, b_eg);
        }
    }

    for (int sq = 0; sq < 64; ++sq) {
        int r = sq >> 3, f = sq & 7;
        bitboard_t files = file_mask[f] | adj_files_mask[f];

        bitboard_t w_front = 0, b_front = 0;
        for (int rr = r + 1; rr < 8; ++rr)
            w_front |= 0xFFULL << (rr * 8);
        for (int rr = r - 1; rr >= 0; --rr)
            b_front |= 0xFFULL << (rr * 8);

        passed_mask[WHITE][sq] = files & w_front;
        passed_mask[BLACK][sq] = files & b_front;

        outpost_mask[WHITE][sq] = adj_files_mask[f] & w_front;
        outpost_mask[BLACK][sq] = adj_files_mask[f] & b_front;

        bitboard_t kz = king_attacks[sq] | (1ULL << sq);
        king_zone_mask[WHITE][sq] = kz | ((kz << 8) & ~0ULL);
        king_zone_mask[BLACK][sq] = kz | (kz >> 8);
    }
}

static inline int sq_dist(int s1, int s2) {
    int df = abs((s1 & 7) - (s2 & 7));
    int dr = abs((s1 >> 3) - (s2 >> 3));
    return df > dr ? df : dr;
}

static int32_t eval_side(const struct Board *b, enum Color us) {
    enum Color them = (enum Color)(us ^ 1);
    int32_t score = 0;

    bitboard_t our_pawns = b->pieces[make_piece(us, PAWN)];
    bitboard_t their_pawns = b->pieces[make_piece(them, PAWN)];
    int our_ksq = lsb64(b->pieces[make_piece(us, KING)]);
    int their_ksq = lsb64(b->pieces[make_piece(them, KING)]);

    bitboard_t their_pawn_atks;
    bitboard_t our_pawn_atks;
    if (us == WHITE) {
        our_pawn_atks = ((our_pawns & ~0x0101010101010101ULL) << 7) |
                        ((our_pawns & ~0x8080808080808080ULL) << 9);
        their_pawn_atks = ((their_pawns & ~0x0101010101010101ULL) >> 9) |
                          ((their_pawns & ~0x8080808080808080ULL) >> 7);
    } else {
        our_pawn_atks = ((our_pawns & ~0x0101010101010101ULL) >> 9) |
                        ((our_pawns & ~0x8080808080808080ULL) >> 7);
        their_pawn_atks = ((their_pawns & ~0x0101010101010101ULL) << 7) |
                          ((their_pawns & ~0x8080808080808080ULL) << 9);
    }

    bitboard_t mob_area = ~(our_pawns | b->pieces[make_piece(us, KING)] | their_pawn_atks);
    bitboard_t enemy_king_zone = king_zone_mask[them][their_ksq];
    bitboard_t their_minors =
        b->pieces[make_piece(them, KNIGHT)] | b->pieces[make_piece(them, BISHOP)];
    bitboard_t their_majors =
        b->pieces[make_piece(them, ROOK)] | b->pieces[make_piece(them, QUEEN)];
    bitboard_t our_minor_atks = 0;
    int atk_units = 0;
    int atk_count = 0;

    // Pawns
    bitboard_t phalanx = our_pawns & ((our_pawns & ~0x0101010101010101ULL) >> 1);
    score += popcount64(phalanx) * pawn_phalanx_bonus;

    bitboard_t phalanx_all = phalanx | (phalanx << 1);
    bitboard_t supported = our_pawns & our_pawn_atks;
    score += popcount64(supported) * pawn_supported_bonus;

    bitboard_t doubled = our_pawns & (us == WHITE ? (our_pawns << 8) : (our_pawns >> 8));
    score -= popcount64(doubled) * pawn_doubled_penalty;

    score += popcount64(our_pawn_atks & their_minors) * pawn_threat_minor;
    score += popcount64(our_pawn_atks & their_majors) * pawn_threat_major;

    bitboard_t pbb = our_pawns;
    while (pbb) {
        int sq = lsb64(pbb);
        score += psqt[make_piece(us, PAWN)][sq];

        int f = sq & 7;
        int rel_r = (us == WHITE) ? (sq >> 3) : (7 - (sq >> 3));

        if (!(adj_files_mask[f] & our_pawns))
            score -= pawn_isolated_penalty;

        if (!(passed_mask[us][sq] & their_pawns)) {
            score += passed_rank_bonus[rel_r];
            if ((1ULL << sq) & (our_pawn_atks | phalanx_all))
                score += passed_prot_bonus[rel_r];

            int stop_sq = (us == WHITE) ? (sq + 8) : (sq - 8);
            if (!(b->all & (1ULL << stop_sq)))
                score += passed_free_bonus;
            else if (b->occ[them] & (1ULL << stop_sq))
                score -= passed_blocked_pen;

            if (rel_r >= 3) {
                int k_prox = sq_dist(their_ksq, stop_sq) * 4 - sq_dist(our_ksq, stop_sq) * 2;
                score += S(0, k_prox * (rel_r - 2));
            }
        }
        pbb &= pbb - 1;
    }

    bitboard_t outpost_ranks = (us == WHITE) ? 0x0000FFFFFF000000ULL : 0x000000FFFFFF0000ULL;

    // Knights
    bitboard_t nbb = b->pieces[make_piece(us, KNIGHT)];
    while (nbb) {
        int sq = lsb64(nbb);
        score += psqt[make_piece(us, KNIGHT)][sq];

        bitboard_t atk = knight_attacks[sq];
        our_minor_atks |= atk;
        score += knight_mob[popcount64(atk & mob_area)];

        if (((1ULL << sq) & outpost_ranks & our_pawn_atks) &&
            !(outpost_mask[us][sq] & their_pawns)) {
            score += knight_outpost_bonus;
        }

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[KNIGHT];
            atk_count++;
        }
        nbb &= nbb - 1;
    }

    // Bishops
    bitboard_t bbb = b->pieces[make_piece(us, BISHOP)];
    if (popcount64(bbb) >= 2)
        score += bishop_pair_bonus;

    while (bbb) {
        int sq = lsb64(bbb);
        score += psqt[make_piece(us, BISHOP)][sq];

        bitboard_t atk = bishop_attacks((enum Square)sq, b->all);
        our_minor_atks |= atk;
        score += bishop_mob[popcount64(atk & mob_area)];

        if (((1ULL << sq) & outpost_ranks & our_pawn_atks) &&
            !(outpost_mask[us][sq] & their_pawns)) {
            score += bishop_outpost_bonus;
        }

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[BISHOP];
            atk_count++;
        }
        bbb &= bbb - 1;
    }

    score += popcount64(our_minor_atks & their_majors) * minor_threat_major;

    // Rooks
    bitboard_t rbb = b->pieces[make_piece(us, ROOK)];
    int seventh_rank = (us == WHITE) ? RANK_7 : RANK_2;
    int eigth_rank = (us == WHITE) ? RANK_8 : RANK_1;
    while (rbb) {
        int sq = lsb64(rbb);
        score += psqt[make_piece(us, ROOK)][sq];

        bitboard_t atk = rook_attacks((enum Square)sq, b->all);
        score += rook_mob[popcount64(atk & mob_area)];

        int f = sq & 7;
        if (!(file_mask[f] & our_pawns)) {
            if (!(file_mask[f] & their_pawns))
                score += rook_open_bonus;
            else
                score += rook_semi_bonus;
        }

        if ((sq >> 3) == seventh_rank &&
            ((their_ksq >> 3) == eigth_rank || (their_pawns & (0xFFULL << (seventh_rank * 8))))) {
            score += rook_seventh_bonus;
        }

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[ROOK];
            atk_count++;
        }
        rbb &= rbb - 1;
    }

    // Queens
    bitboard_t qbb = b->pieces[make_piece(us, QUEEN)];
    while (qbb) {
        int sq = lsb64(qbb);
        score += psqt[make_piece(us, QUEEN)][sq];

        bitboard_t atk = queen_attacks((enum Square)sq, b->all);
        score += queen_mob[popcount64(atk & mob_area)];

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[QUEEN];
            atk_count++;
        }
        qbb &= qbb - 1;
    }

    // King & King Safety
    score += psqt[make_piece(us, KING)][our_ksq];

    int kf = our_ksq & 7;
    int kr = our_ksq >> 3;
    if (kf <= FILE_C || kf >= FILE_F) {
        bitboard_t shield = king_attacks[our_ksq] & our_pawns;
        score += popcount64(shield) * king_shield_bonus;
    }
    if (!(file_mask[kf] & our_pawns))
        score -= king_open_penalty;

    if (their_majors || popcount64(their_minors) >= 2) {
        int adv_rank = us == WHITE ? kr : (7 - kr);
        if (adv_rank >= RANK_3)
            score -= S(25 * (adv_rank - RANK_2), 20 * (adv_rank - RANK_2));
    }

    if (atk_count >= 2) {
        int danger = atk_units * atk_units;
        score += S(danger, 0);
    }

    return score;
}

int evaluate(const struct Board *b) {
    bitboard_t pawns = b->pieces[W_PAWN] | b->pieces[B_PAWN];
    bitboard_t non_pawns = b->all & ~(pawns | b->pieces[W_KING] | b->pieces[B_KING]);
    if (!non_pawns && !pawns)
        return 0;

    int phase = 0;
    for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
        int cnt = popcount64(b->pieces[make_piece(WHITE, (enum PieceType)pt)] |
                             b->pieces[make_piece(BLACK, (enum PieceType)pt)]);
        phase += cnt * phase_weight[pt];
    }
    if (phase > 24)
        phase = 24;

    if (!pawns) {
        if (phase <= 1)
            return 0;
        if (phase == 2 &&
            (popcount64(b->pieces[W_KNIGHT]) == 2 || popcount64(b->pieces[B_KNIGHT]) == 2))
            return 0;
    }

    int32_t score = eval_side(b, b->side) - eval_side(b, (enum Color)(b->side ^ 1));
    score += tempo_bonus;

    int mg = mg_val(score);
    int eg = eg_val(score);

    // Opposite-colored bishops endgame scaling
    if (phase == 2 && popcount64(b->pieces[W_BISHOP]) == 1 &&
        popcount64(b->pieces[B_BISHOP]) == 1) {
        bitboard_t light_sq = 0x55AA55AA55AA55AAULL;
        bool w_light = (b->pieces[W_BISHOP] & light_sq) != 0;
        bool b_light = (b->pieces[B_BISHOP] & light_sq) != 0;
        if (w_light != b_light)
            eg = (eg * 3) / 4;
    }

    return (mg * phase + eg * (24 - phase)) / 24;
}
