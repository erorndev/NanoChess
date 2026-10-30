#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "movegen.h"
#include "platform.h"
#include "types.h"
#include "zobrist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Feature indices for linear evaluation decomposition
enum FeatIdx {
    F_MAT_PAWN = 0,
    F_MAT_KNIGHT,
    F_MAT_BISHOP,
    F_MAT_ROOK,
    F_MAT_QUEEN,
    F_PASSED_R1,
    F_PASSED_PROT_R1 = F_PASSED_R1 + 6,
    F_KNIGHT_MOB0 = F_PASSED_PROT_R1 + 6,
    F_BISHOP_MOB0 = F_KNIGHT_MOB0 + 9,
    F_ROOK_MOB0 = F_BISHOP_MOB0 + 14,
    F_QUEEN_MOB0 = F_ROOK_MOB0 + 15,
    F_PAWN_PHALANX = F_QUEEN_MOB0 + 28,
    F_PAWN_SUPPORTED,
    F_PAWN_DOUBLED,
    F_PAWN_ISOLATED,
    F_PASSED_FREE,
    F_PASSED_BLOCKED,
    F_KNIGHT_OUTPOST,
    F_BISHOP_OUTPOST,
    F_BISHOP_PAIR,
    F_ROOK_OPEN,
    F_ROOK_SEMI,
    F_ROOK_SEVENTH,
    F_PAWN_THREAT_MINOR,
    F_PAWN_THREAT_MAJOR,
    F_MINOR_THREAT_MAJOR,
    F_KING_SHIELD,
    F_KING_OPEN,
    F_TEMPO,
    F_PSQT_START,
    FEAT_COUNT = F_PSQT_START + 6 * 64
};

struct SparseFeat {
    uint16_t idx;
    int16_t coeff;
};

struct Sample {
    struct SparseFeat feats[128];
    int n_feats;
    double base_mg;
    double base_eg;
    int phase;
    double eg_scale;
    double target;
};

// Baseline PeSTO tables
static const int base_mg_piece[6] = { 82, 336, 363, 477, 1025, 0 };
static const int base_eg_piece[6] = { 94, 277, 297, 512,  936, 0 };
static const int phase_weight[6]  = {  0,   1,   1,   2,    4, 0 };

static const int16_t mg_table[6][64] = {
    {
          0,   0,   0,   0,   0,   0,   0,   0,
         98, 134,  61,  95,  68, 126,  34, -11,
         -6,   7,  26,  31,  65,  56,  25, -20,
        -14,  13,   6,  21,  23,  12,  17, -23,
        -27,  -2,  -5,  12,  17,   6,  10, -25,
        -26,  -4,  -4, -10,   3,   3,  33, -12,
        -35,  -1, -20, -23, -15,  24,  38, -22,
          0,   0,   0,   0,   0,   0,   0,   0
    },
    {
        -167, -89, -34, -49,  61, -97, -15, -107,
         -73, -41,  72,  36,  23,  62,   7,  -17,
         -47,  60,  37,  65,  84, 129,  73,   44,
          -9,  17,  19,  53,  37,  69,  18,   22,
         -13,   4,  16,  13,  28,  19,  21,   -8,
         -23,  -9,  12,  10,  19,  17,  25,  -16,
         -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23
    },
    {
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21
    },
    {
         32,  42,  32,  51,  63,   9,  31,  43,
         27,  32,  58,  62,  80,  67,  26,  44,
         -5,  19,  26,  36,  17,  45,  61,  16,
        -24, -11,   7,  26,  24,  35,  -8, -20,
        -36, -26, -12,  -1,   9,  -7,   6, -23,
        -45, -25, -16, -17,   3,   0,  -5, -33,
        -44, -16, -20,  -9,  -1,  11,  -6, -71,
        -19, -13,   1,  17,  16,   7, -37, -26
    },
    {
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50
    },
    {
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
    {
          0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
         94, 100,  85,  67,  56,  53,  82,  84,
         32,  24,  13,   5,  -2,   4,  17,  17,
         13,   9,  -3,  -7,  -7,  -8,   3,  -1,
          4,   7,  -6,   1,   0,  -5,  -1,  -8,
         13,   8,   8,  10,  13,   0,   2,  -7,
          0,   0,   0,   0,   0,   0,   0,   0
    },
    {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64
    },
    {
        -14, -21, -11,  -8,  -7,  -9, -17, -24,
         -8,  -4,   7, -12,  -3, -13,  -4, -14,
          2,  -8,   0,  -1,  -2,   6,   0,   4,
         -3,   9,  12,   9,  14,  10,   3,   2,
         -6,   3,  13,  19,   7,  10,  -3,  -9,
        -12,  -3,   8,  10,  13,   3,  -7, -15,
        -14, -18,  -7,  -1,   4,  -9, -15, -27,
        -23,  -9, -23,  -5,  -9, -16,  -5, -17
    },
    {
         13,  10,  18,  15,  12,  12,   8,   5,
         11,  13,  13,  11,  -3,   3,   8,   3,
          7,   7,   7,   5,   4,  -3,  -5,  -3,
          4,   3,  13,   1,   2,   1,  -1,   2,
          3,   5,   8,   4,  -5,  -6,  -8, -11,
         -4,   0,  -5,  -1,  -7, -12,  -8, -16,
         -6,  -6,   0,   2,  -9,  -9, -11,  -3,
         -9,   2,   3,  -1,  -5, -13,   4, -20
    },
    {
        -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41
    },
    {
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

static bitboard_t t_file_mask[8];
static bitboard_t t_adj_files_mask[8];
static bitboard_t t_passed_mask[COLOR_NB][64];
static bitboard_t t_outpost_mask[COLOR_NB][64];
static bitboard_t t_king_zone_mask[COLOR_NB][64];
static const int king_atk_weight[6] = {0, 2, 2, 3, 5, 0};

static void init_tuner_masks(void) {
    for (int f = 0; f < 8; ++f) {
        t_file_mask[f] = 0x0101010101010101ULL << f;
        t_adj_files_mask[f] = 0;
        if (f > 0)
            t_adj_files_mask[f] |= 0x0101010101010101ULL << (f - 1);
        if (f < 7)
            t_adj_files_mask[f] |= 0x0101010101010101ULL << (f + 1);
    }
    for (int sq = 0; sq < 64; ++sq) {
        int r = sq >> 3, f = sq & 7;
        bitboard_t files = t_file_mask[f] | t_adj_files_mask[f];
        bitboard_t w_front = 0, b_front = 0;
        for (int rr = r + 1; rr < 8; ++rr)
            w_front |= 0xFFULL << (rr * 8);
        for (int rr = r - 1; rr >= 0; --rr)
            b_front |= 0xFFULL << (rr * 8);
        t_passed_mask[WHITE][sq] = files & w_front;
        t_passed_mask[BLACK][sq] = files & b_front;
        t_outpost_mask[WHITE][sq] = t_adj_files_mask[f] & w_front;
        t_outpost_mask[BLACK][sq] = t_adj_files_mask[f] & b_front;
        bitboard_t kz = king_attacks[sq] | (1ULL << sq);
        t_king_zone_mask[WHITE][sq] = kz | ((kz << 8) & ~0ULL);
        t_king_zone_mask[BLACK][sq] = kz | (kz >> 8);
    }
}

static inline int sq_dist(int s1, int s2) {
    int df = abs((s1 & 7) - (s2 & 7));
    int dr = abs((s1 >> 3) - (s2 >> 3));
    return df > dr ? df : dr;
}

static void extract_side_feats(const struct Board *b, enum Color us, int sign, int16_t *dense,
                                double *base_mg, double *base_eg) {
    enum Color them = (enum Color)(us ^ 1);
    bitboard_t our_pawns = b->pieces[make_piece(us, PAWN)];
    bitboard_t their_pawns = b->pieces[make_piece(them, PAWN)];
    int our_ksq = lsb64(b->pieces[make_piece(us, KING)]);
    int their_ksq = lsb64(b->pieces[make_piece(them, KING)]);

    bitboard_t their_pawn_atks, our_pawn_atks;
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
    bitboard_t enemy_king_zone = t_king_zone_mask[them][their_ksq];
    bitboard_t their_minors =
        b->pieces[make_piece(them, KNIGHT)] | b->pieces[make_piece(them, BISHOP)];
    bitboard_t their_majors =
        b->pieces[make_piece(them, ROOK)] | b->pieces[make_piece(them, QUEEN)];
    bitboard_t our_minor_atks = 0;
    int atk_units = 0, atk_count = 0;

    bitboard_t phalanx = our_pawns & ((our_pawns & ~0x0101010101010101ULL) >> 1);
    dense[F_PAWN_PHALANX] += (int16_t)(sign * popcount64(phalanx));

    bitboard_t phalanx_all = phalanx | (phalanx << 1);
    bitboard_t supported = our_pawns & our_pawn_atks;
    dense[F_PAWN_SUPPORTED] += (int16_t)(sign * popcount64(supported));

    bitboard_t doubled = our_pawns & (us == WHITE ? (our_pawns << 8) : (our_pawns >> 8));
    dense[F_PAWN_DOUBLED] -= (int16_t)(sign * popcount64(doubled));

    dense[F_PAWN_THREAT_MINOR] += (int16_t)(sign * popcount64(our_pawn_atks & their_minors));
    dense[F_PAWN_THREAT_MAJOR] += (int16_t)(sign * popcount64(our_pawn_atks & their_majors));

    bitboard_t pbb = our_pawns;
    while (pbb) {
        int sq = lsb64(pbb);
        int tsq = (us == WHITE) ? (sq ^ 56) : sq;
        dense[F_MAT_PAWN] += (int16_t)sign;
        dense[F_PSQT_START + PAWN * 64 + tsq] += (int16_t)sign;

        int f = sq & 7;
        int rel_r = (us == WHITE) ? (sq >> 3) : (7 - (sq >> 3));

        if (!(t_adj_files_mask[f] & our_pawns))
            dense[F_PAWN_ISOLATED] -= (int16_t)sign;

        if (!(t_passed_mask[us][sq] & their_pawns) && rel_r >= 1 && rel_r <= 6) {
            dense[F_PASSED_R1 + (rel_r - 1)] += (int16_t)sign;
            if ((1ULL << sq) & (our_pawn_atks | phalanx_all))
                dense[F_PASSED_PROT_R1 + (rel_r - 1)] += (int16_t)sign;

            int stop_sq = (us == WHITE) ? (sq + 8) : (sq - 8);
            if (!(b->all & (1ULL << stop_sq)))
                dense[F_PASSED_FREE] += (int16_t)sign;
            else if (b->occ[them] & (1ULL << stop_sq))
                dense[F_PASSED_BLOCKED] -= (int16_t)sign;

            if (rel_r >= 3) {
                int k_prox = sq_dist(their_ksq, stop_sq) * 4 - sq_dist(our_ksq, stop_sq) * 2;
                *base_eg += sign * (k_prox * (rel_r - 2));
            }
        }
        pbb &= pbb - 1;
    }

    bitboard_t outpost_ranks = (us == WHITE) ? 0x0000FFFFFF000000ULL : 0x000000FFFFFF0000ULL;

    bitboard_t nbb = b->pieces[make_piece(us, KNIGHT)];
    while (nbb) {
        int sq = lsb64(nbb);
        int tsq = (us == WHITE) ? (sq ^ 56) : sq;
        dense[F_MAT_KNIGHT] += (int16_t)sign;
        dense[F_PSQT_START + KNIGHT * 64 + tsq] += (int16_t)sign;

        bitboard_t atk = knight_attacks[sq];
        our_minor_atks |= atk;
        dense[F_KNIGHT_MOB0 + popcount64(atk & mob_area)] += (int16_t)sign;

        if (((1ULL << sq) & outpost_ranks & our_pawn_atks) &&
            !(t_outpost_mask[us][sq] & their_pawns))
            dense[F_KNIGHT_OUTPOST] += (int16_t)sign;

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[KNIGHT];
            atk_count++;
        }
        nbb &= nbb - 1;
    }

    bitboard_t bbb = b->pieces[make_piece(us, BISHOP)];
    if (popcount64(bbb) >= 2)
        dense[F_BISHOP_PAIR] += (int16_t)sign;

    while (bbb) {
        int sq = lsb64(bbb);
        int tsq = (us == WHITE) ? (sq ^ 56) : sq;
        dense[F_MAT_BISHOP] += (int16_t)sign;
        dense[F_PSQT_START + BISHOP * 64 + tsq] += (int16_t)sign;

        bitboard_t atk = bishop_attacks((enum Square)sq, b->all);
        our_minor_atks |= atk;
        dense[F_BISHOP_MOB0 + popcount64(atk & mob_area)] += (int16_t)sign;

        if (((1ULL << sq) & outpost_ranks & our_pawn_atks) &&
            !(t_outpost_mask[us][sq] & their_pawns))
            dense[F_BISHOP_OUTPOST] += (int16_t)sign;

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[BISHOP];
            atk_count++;
        }
        bbb &= bbb - 1;
    }

    dense[F_MINOR_THREAT_MAJOR] += (int16_t)(sign * popcount64(our_minor_atks & their_majors));

    bitboard_t rbb = b->pieces[make_piece(us, ROOK)];
    int seventh_rank = (us == WHITE) ? RANK_7 : RANK_2;
    int eigth_rank = (us == WHITE) ? RANK_8 : RANK_1;
    while (rbb) {
        int sq = lsb64(rbb);
        int tsq = (us == WHITE) ? (sq ^ 56) : sq;
        dense[F_MAT_ROOK] += (int16_t)sign;
        dense[F_PSQT_START + ROOK * 64 + tsq] += (int16_t)sign;

        bitboard_t atk = rook_attacks((enum Square)sq, b->all);
        dense[F_ROOK_MOB0 + popcount64(atk & mob_area)] += (int16_t)sign;

        int f = sq & 7;
        if (!(t_file_mask[f] & our_pawns)) {
            if (!(t_file_mask[f] & their_pawns))
                dense[F_ROOK_OPEN] += (int16_t)sign;
            else
                dense[F_ROOK_SEMI] += (int16_t)sign;
        }

        if ((sq >> 3) == seventh_rank &&
            ((their_ksq >> 3) == eigth_rank || (their_pawns & (0xFFULL << (seventh_rank * 8)))))
            dense[F_ROOK_SEVENTH] += (int16_t)sign;

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[ROOK];
            atk_count++;
        }
        rbb &= rbb - 1;
    }

    bitboard_t qbb = b->pieces[make_piece(us, QUEEN)];
    while (qbb) {
        int sq = lsb64(qbb);
        int tsq = (us == WHITE) ? (sq ^ 56) : sq;
        dense[F_MAT_QUEEN] += (int16_t)sign;
        dense[F_PSQT_START + QUEEN * 64 + tsq] += (int16_t)sign;

        bitboard_t atk = queen_attacks((enum Square)sq, b->all);
        dense[F_QUEEN_MOB0 + popcount64(atk & mob_area)] += (int16_t)sign;

        if (atk & enemy_king_zone) {
            atk_units += king_atk_weight[QUEEN];
            atk_count++;
        }
        qbb &= qbb - 1;
    }

    int tksq = (us == WHITE) ? (our_ksq ^ 56) : our_ksq;
    dense[F_PSQT_START + KING * 64 + tksq] += (int16_t)sign;

    int kf = our_ksq & 7;
    int kr = our_ksq >> 3;
    if (kf <= FILE_C || kf >= FILE_F) {
        bitboard_t shield = king_attacks[our_ksq] & our_pawns;
        dense[F_KING_SHIELD] += (int16_t)(sign * popcount64(shield));
    }
    if (!(t_file_mask[kf] & our_pawns))
        dense[F_KING_OPEN] -= (int16_t)sign;

    if (their_majors || popcount64(their_minors) >= 2) {
        int adv_rank = us == WHITE ? kr : (7 - kr);
        if (adv_rank >= RANK_3) {
            *base_mg -= sign * 25 * (adv_rank - RANK_2);
            *base_eg -= sign * 20 * (adv_rank - RANK_2);
        }
    }

    if (atk_count >= 2)
        *base_mg += sign * (atk_units * atk_units);
}

static bool extract_sample(const struct Board *b, double target_stm, struct Sample *out) {
    bitboard_t pawns = b->pieces[W_PAWN] | b->pieces[B_PAWN];
    bitboard_t non_pawns = b->all & ~(pawns | b->pieces[W_KING] | b->pieces[B_KING]);
    if (!non_pawns && !pawns)
        return false;

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
            return false;
        if (phase == 2 &&
            (popcount64(b->pieces[W_KNIGHT]) == 2 || popcount64(b->pieces[B_KNIGHT]) == 2))
            return false;
    }

    int16_t dense[FEAT_COUNT];
    memset(dense, 0, sizeof(dense));
    double base_mg = 0.0, base_eg = 0.0;

    extract_side_feats(b, b->side, +1, dense, &base_mg, &base_eg);
    extract_side_feats(b, (enum Color)(b->side ^ 1), -1, dense, &base_mg, &base_eg);
    dense[F_TEMPO] += 1;

    out->n_feats = 0;
    for (int i = 0; i < FEAT_COUNT; ++i) {
        if (dense[i] != 0 && out->n_feats < 128) {
            out->feats[out->n_feats].idx = (uint16_t)i;
            out->feats[out->n_feats].coeff = dense[i];
            out->n_feats++;
        }
    }

    out->base_mg = base_mg;
    out->base_eg = base_eg;
    out->phase = phase;
    out->eg_scale = 1.0;

    if (phase == 2 && popcount64(b->pieces[W_BISHOP]) == 1 &&
        popcount64(b->pieces[B_BISHOP]) == 1) {
        bitboard_t light_sq = 0x55AA55AA55AA55AAULL;
        if (((b->pieces[W_BISHOP] & light_sq) != 0) != ((b->pieces[B_BISHOP] & light_sq) != 0))
            out->eg_scale = 0.75;
    }

    out->target = target_stm;
    return true;
}

static void init_params(double *mg, double *eg) {
    mg[F_MAT_PAWN] = 82;    eg[F_MAT_PAWN] = 94;
    mg[F_MAT_KNIGHT] = 336; eg[F_MAT_KNIGHT] = 277;
    mg[F_MAT_BISHOP] = 363; eg[F_MAT_BISHOP] = 297;
    mg[F_MAT_ROOK] = 477;   eg[F_MAT_ROOK] = 512;
    mg[F_MAT_QUEEN] = 1025; eg[F_MAT_QUEEN] = 936;

    const int pr_mg[6] = {5, 12, 24, 49, 84, 136};
    const int pr_eg[6] = {11, 25, 52, 85, 139, 218};
    const int pp_mg[6] = {3, 6, 12, 21, 32, 48};
    const int pp_eg[6] = {6, 9, 18, 31, 48, 72};
    for (int i = 0; i < 6; ++i) {
        mg[F_PASSED_R1 + i] = pr_mg[i];
        eg[F_PASSED_R1 + i] = pr_eg[i];
        mg[F_PASSED_PROT_R1 + i] = pp_mg[i];
        eg[F_PASSED_PROT_R1 + i] = pp_eg[i];
    }

    const int km_mg[9] = {-36, -21, -9, 0, 6, 13, 17, 21, 24};
    const int km_eg[9] = {-40, -24, -10, -1, 7, 14, 20, 24, 24};
    for (int i = 0; i < 9; ++i) {
        mg[F_KNIGHT_MOB0 + i] = km_mg[i];
        eg[F_KNIGHT_MOB0 + i] = km_eg[i];
    }

    const int bm_mg[14] = {-41, -24, -11, -4, 2, 6, 13, 18, 21, 26, 29, 32, 34, 36};
    const int bm_eg[14] = {-44, -28, -14, -6, 3, 7, 14, 20, 25, 29, 33, 34, 37, 38};
    for (int i = 0; i < 14; ++i) {
        mg[F_BISHOP_MOB0 + i] = bm_mg[i];
        eg[F_BISHOP_MOB0 + i] = bm_eg[i];
    }

    const int rm_mg[15] = {-28, -14, -10, -5, 0, 4, 7, 11, 13, 17, 19, 21, 23, 25, 26};
    const int rm_eg[15] = {-36, -24, -14, -6, 0, 6, 12, 17, 22, 26, 31, 33, 36, 38, 41};
    for (int i = 0; i < 15; ++i) {
        mg[F_ROOK_MOB0 + i] = rm_mg[i];
        eg[F_ROOK_MOB0 + i] = rm_eg[i];
    }

    const int qm_mg[28] = {-24, -16, -10, -7, -3, 1,  2,  4,  6,  7,  10, 12, 15, 15,
                           16,  17,  18,  19, 20, 21, 22, 23, 24, 25, 26, 26, 27, 27};
    const int qm_eg[28] = {-36, -26, -18, -12, -6, 0,  4,  8,  12, 16, 20, 23, 26, 29,
                           32,  34,  36,  38,  40, 41, 42, 43, 44, 45, 46, 47, 48, 48};
    for (int i = 0; i < 28; ++i) {
        mg[F_QUEEN_MOB0 + i] = qm_mg[i];
        eg[F_QUEEN_MOB0 + i] = qm_eg[i];
    }

    mg[F_PAWN_PHALANX] = 9;       eg[F_PAWN_PHALANX] = 7;
    mg[F_PAWN_SUPPORTED] = 13;    eg[F_PAWN_SUPPORTED] = 10;
    mg[F_PAWN_DOUBLED] = 11;      eg[F_PAWN_DOUBLED] = 22;
    mg[F_PAWN_ISOLATED] = 12;     eg[F_PAWN_ISOLATED] = 6;
    mg[F_PASSED_FREE] = 10;       eg[F_PASSED_FREE] = 29;
    mg[F_PASSED_BLOCKED] = 7;     eg[F_PASSED_BLOCKED] = 14;
    mg[F_KNIGHT_OUTPOST] = 24;    eg[F_KNIGHT_OUTPOST] = 14;
    mg[F_BISHOP_OUTPOST] = 15;    eg[F_BISHOP_OUTPOST] = 10;
    mg[F_BISHOP_PAIR] = 32;       eg[F_BISHOP_PAIR] = 54;
    mg[F_ROOK_OPEN] = 26;         eg[F_ROOK_OPEN] = 14;
    mg[F_ROOK_SEMI] = 13;         eg[F_ROOK_SEMI] = 7;
    mg[F_ROOK_SEVENTH] = 21;      eg[F_ROOK_SEVENTH] = 30;
    mg[F_PAWN_THREAT_MINOR] = 32; eg[F_PAWN_THREAT_MINOR] = 24;
    mg[F_PAWN_THREAT_MAJOR] = 44; eg[F_PAWN_THREAT_MAJOR] = 32;
    mg[F_MINOR_THREAT_MAJOR] = 28; eg[F_MINOR_THREAT_MAJOR] = 18;
    mg[F_KING_SHIELD] = 9;        eg[F_KING_SHIELD] = 0;
    mg[F_KING_OPEN] = 17;         eg[F_KING_OPEN] = 0;
    mg[F_TEMPO] = 18;             eg[F_TEMPO] = 9;

    for (int pt = 0; pt < 6; ++pt) {
        for (int sq = 0; sq < 64; ++sq) {
            int idx = F_PSQT_START + pt * 64 + sq;
            mg[idx] = mg_table[pt][sq];
            eg[idx] = eg_table[pt][sq];
        }
    }
}

static inline double eval_sample(const struct Sample *s, const double *mg, const double *eg) {
    double m = s->base_mg;
    double e = s->base_eg;
    for (int i = 0; i < s->n_feats; ++i) {
        int idx = s->feats[i].idx;
        double c = s->feats[i].coeff;
        m += c * mg[idx];
        e += c * eg[idx];
    }
    e *= s->eg_scale;
    return (m * s->phase + e * (24 - s->phase)) / 24.0;
}

static inline double sigmoid(double K, double eval_cp) {
    return 1.0 / (1.0 + pow(10.0, -K * eval_cp / 400.0));
}

static void enforce_monotonic(double *arr, int n) {
    for (int pass = 0; pass < 4; ++pass) {
        for (int i = 1; i < n; ++i) {
            if (arr[i] < arr[i - 1]) {
                double avg = 0.5 * (arr[i] + arr[i - 1]);
                arr[i - 1] = avg;
                arr[i] = avg;
            }
        }
    }
}

static double compute_mse_for_K(const struct Sample *samples, int n, const double *mg, const double *eg, double K) {
    double total = 0.0;
    for (int i = 0; i < n; ++i) {
        double ev = eval_sample(&samples[i], mg, eg);
        double pred = sigmoid(K, ev);
        double diff = samples[i].target - pred;
        total += diff * diff;
    }
    return total / n;
}

static double optimize_K(const struct Sample *samples, int n, const double *mg, const double *eg) {
    double best_k = 1.0;
    double best_mse = 1e9;
    for (double k = 0.4; k <= 3.0; k += 0.05) {
        double mse = compute_mse_for_K(samples, n, mg, eg, k);
        if (mse < best_mse) {
            best_mse = mse;
            best_k = k;
        }
    }
    double start_k = best_k - 0.05;
    double end_k = best_k + 0.05;
    for (double k = start_k; k <= end_k; k += 0.005) {
        double mse = compute_mse_for_K(samples, n, mg, eg, k);
        if (mse < best_mse) {
            best_mse = mse;
            best_k = k;
        }
    }
    return best_k;
}

int main(int argc, char **argv) {
    init_zobrist();
    init_attacks();
    init_eval();
    init_tuner_masks();

    int max_samples = 700000;
    struct Sample *samples = (struct Sample *)calloc((size_t)max_samples, sizeof(struct Sample));
    if (!samples) {
        fprintf(stderr, "Failed to allocate memory for %d samples\n", max_samples);
        return 1;
    }

    const char *epd_path = (argc > 1) ? argv[1] : "build/quiet-labeled.epd";
    FILE *fp = fopen(epd_path, "r");
    if (!fp) {
        fprintf(stderr, "Could not open %s\n", epd_path);
        free(samples);
        return 1;
    }

    printf("Loading quiet dataset from %s (target %d samples)...\n", epd_path, max_samples);
    fflush(stdout);

    char line[512];
    int n_samples = 0;
    int line_count = 0;
    int mismatch_count = 0;

    double mg0[FEAT_COUNT], eg0[FEAT_COUNT];
    init_params(mg0, eg0);

    while (fgets(line, sizeof(line), fp) && n_samples < max_samples) {
        line_count++;
        char *q1 = strchr(line, '"');
        if (!q1) continue;
        char *q2 = strchr(q1 + 1, '"');
        if (!q2) continue;

        double white_win = -1.0;
        if (strncmp(q1 + 1, "1-0", 3) == 0) white_win = 1.0;
        else if (strncmp(q1 + 1, "0-1", 3) == 0) white_win = 0.0;
        else if (strncmp(q1 + 1, "1/2-1/2", 7) == 0) white_win = 0.5;
        else continue;

        char *c9 = strstr(line, " c9 ");
        if (c9) *c9 = '\0';
        else {
            char *semi = strchr(line, ';');
            if (semi) *semi = '\0';
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }

        struct Board b;
        if (!set_fen(&b, line)) continue;

        double target_stm = (b.side == WHITE) ? white_win : (1.0 - white_win);
        if (extract_sample(&b, target_stm, &samples[n_samples])) {
            if (n_samples < 5000) {
                int ev_eng = evaluate(&b);
                double ev_s = eval_sample(&samples[n_samples], mg0, eg0);
                if (abs(ev_eng - (int)round(ev_s)) > 1) {
                    mismatch_count++;
                    if (mismatch_count <= 3) {
                        printf("Verification mismatch sample %d: engine=%d sample=%.1f FEN=%s\n",
                               n_samples, ev_eng, ev_s, line);
                    }
                }
            }
            n_samples++;
        }
    }
    fclose(fp);

    printf("Successfully loaded %d samples from %d lines. Eval verification mismatches: %d\n",
           n_samples, line_count, mismatch_count);
    fflush(stdout);

    int n_train = (n_samples * 85) / 100;
    int n_val = n_samples - n_train;

    printf("Optimizing K scaling parameter on baseline eval...\n");
    fflush(stdout);
    double K = optimize_K(samples, n_train, mg0, eg0);
    printf("Optimal K: %.4f\n", K);

    double init_train_mse = compute_mse_for_K(samples, n_train, mg0, eg0, K);
    double init_val_mse = compute_mse_for_K(samples + n_train, n_val, mg0, eg0, K);
    printf("Initial MSE -> Train: %.6f | Val: %.6f\n\n", init_train_mse, init_val_mse);
    fflush(stdout);

    double mg[FEAT_COUNT], eg[FEAT_COUNT];
    memcpy(mg, mg0, sizeof(mg));
    memcpy(eg, eg0, sizeof(eg));

    double m_mg[FEAT_COUNT] = {0}, v_mg[FEAT_COUNT] = {0};
    double m_eg[FEAT_COUNT] = {0}, v_eg[FEAT_COUNT] = {0};
    const double lr = 0.5;
    const double beta1 = 0.9, beta2 = 0.999, eps = 1e-8;
    const double l2_reg = 1e-5;
    const double l2_psqt = 1e-6;
    const double scale_const = (K * log(10.0)) / 400.0;

    double best_val_mse = init_val_mse;
    double best_mg[FEAT_COUNT], best_eg[FEAT_COUNT];
    memcpy(best_mg, mg, sizeof(mg));
    memcpy(best_eg, eg, sizeof(eg));

    int epochs = 100;
    printf("Running Adam optimizer for %d epochs...\n", epochs);
    fflush(stdout);

    for (int epoch = 1; epoch <= epochs; ++epoch) {
        double g_mg[FEAT_COUNT] = {0};
        double g_eg[FEAT_COUNT] = {0};

        for (int i = 0; i < n_train; ++i) {
            const struct Sample *s = &samples[i];
            double ev = eval_sample(s, mg, eg);
            double sig = sigmoid(K, ev);
            double dloss = 2.0 * (sig - s->target) * sig * (1.0 - sig) * scale_const / n_train;
            double w_mg = dloss * (s->phase / 24.0);
            double w_eg = dloss * ((24.0 - s->phase) / 24.0) * s->eg_scale;

            for (int f = 0; f < s->n_feats; ++f) {
                int idx = s->feats[f].idx;
                double c = s->feats[f].coeff;
                g_mg[idx] += w_mg * c;
                g_eg[idx] += w_eg * c;
            }
        }

        // Share 35% horizontal gradient across mirrored files for symmetric PSQTs
        for (int pt = PAWN; pt <= QUEEN; ++pt) {
            int base = F_PSQT_START + pt * 64;
            for (int r = 0; r < 8; ++r) {
                for (int f = 0; f < 4; ++f) {
                    int sq1 = r * 8 + f;
                    int sq2 = r * 8 + (7 - f);
                    double gm1 = g_mg[base + sq1], gm2 = g_mg[base + sq2];
                    double ge1 = g_eg[base + sq1], ge2 = g_eg[base + sq2];
                    g_mg[base + sq1] = 0.65 * gm1 + 0.35 * gm2;
                    g_mg[base + sq2] = 0.65 * gm2 + 0.35 * gm1;
                    g_eg[base + sq1] = 0.65 * ge1 + 0.35 * ge2;
                    g_eg[base + sq2] = 0.65 * ge2 + 0.35 * ge1;
                }
            }
        }

        double b1_t = 1.0 - pow(beta1, epoch);
        double b2_t = 1.0 - pow(beta2, epoch);

        // Keep pawn material anchored at 82/94 (k starting at 1)
        for (int k = 1; k < FEAT_COUNT; ++k) {
            if (k >= F_PSQT_START && k < F_PSQT_START + 64) {
                int sq = k - F_PSQT_START;
                if (sq < 8 || sq >= 56)
                    continue;
            }
            double reg = (k >= F_PSQT_START) ? l2_psqt : l2_reg;
            double step_lr = (k >= F_PSQT_START) ? (lr * 0.75) : lr;

            g_mg[k] += reg * (mg[k] - mg0[k]);
            g_eg[k] += reg * (eg[k] - eg0[k]);

            m_mg[k] = beta1 * m_mg[k] + (1.0 - beta1) * g_mg[k];
            v_mg[k] = beta2 * v_mg[k] + (1.0 - beta2) * g_mg[k] * g_mg[k];
            mg[k] -= step_lr * (m_mg[k] / b1_t) / (sqrt(v_mg[k] / b2_t) + eps);

            if (k != F_KING_SHIELD && k != F_KING_OPEN) {
                m_eg[k] = beta1 * m_eg[k] + (1.0 - beta1) * g_eg[k];
                v_eg[k] = beta2 * v_eg[k] + (1.0 - beta2) * g_eg[k] * g_eg[k];
                eg[k] -= step_lr * (m_eg[k] / b1_t) / (sqrt(v_eg[k] / b2_t) + eps);
            }

            if (k >= F_PAWN_PHALANX && k <= F_TEMPO) {
                if (mg[k] < 1.0)
                    mg[k] = 1.0;
                if (k != F_KING_SHIELD && k != F_KING_OPEN && eg[k] < 1.0)
                    eg[k] = 1.0;
            }
        }

        // Safety clamps on piece material values
        if (mg[F_MAT_KNIGHT] < 280) mg[F_MAT_KNIGHT] = 280; if (mg[F_MAT_KNIGHT] > 400) mg[F_MAT_KNIGHT] = 400;
        if (eg[F_MAT_KNIGHT] < 220) eg[F_MAT_KNIGHT] = 220; if (eg[F_MAT_KNIGHT] > 360) eg[F_MAT_KNIGHT] = 360;
        if (mg[F_MAT_BISHOP] < 290) mg[F_MAT_BISHOP] = 290; if (mg[F_MAT_BISHOP] > 420) mg[F_MAT_BISHOP] = 420;
        if (eg[F_MAT_BISHOP] < 240) eg[F_MAT_BISHOP] = 240; if (eg[F_MAT_BISHOP] > 380) eg[F_MAT_BISHOP] = 380;
        if (mg[F_MAT_ROOK]   < 420) mg[F_MAT_ROOK]   = 420; if (mg[F_MAT_ROOK]   > 560) mg[F_MAT_ROOK]   = 560;
        if (eg[F_MAT_ROOK]   < 450) eg[F_MAT_ROOK]   = 450; if (eg[F_MAT_ROOK]   > 600) eg[F_MAT_ROOK]   = 600;
        if (mg[F_MAT_QUEEN]  < 880) mg[F_MAT_QUEEN]  = 880; if (mg[F_MAT_QUEEN]  > 1150) mg[F_MAT_QUEEN]  = 1150;
        if (eg[F_MAT_QUEEN]  < 800) eg[F_MAT_QUEEN]  = 800; if (eg[F_MAT_QUEEN]  > 1100) eg[F_MAT_QUEEN]  = 1100;

        // [-16, +16] cp clamp per PSQT table
        for (int pt = 0; pt < 6; ++pt) {
            int base = F_PSQT_START + pt * 64;
            int s_start = (pt == PAWN) ? 8 : 0;
            int s_end = (pt == PAWN) ? 56 : 64;
            for (int sq = s_start; sq < s_end; ++sq) {
                double dmg = mg[base + sq] - mg0[base + sq];
                double deg = eg[base + sq] - eg0[base + sq];
                if (dmg > 16.0) mg[base + sq] = mg0[base + sq] + 16.0;
                if (dmg < -16.0) mg[base + sq] = mg0[base + sq] - 16.0;
                if (deg > 16.0) eg[base + sq] = eg0[base + sq] + 16.0;
                if (deg < -16.0) eg[base + sq] = eg0[base + sq] - 16.0;
            }
        }

        if (mg[F_PASSED_R1] < 0.0) mg[F_PASSED_R1] = 0.0;
        if (eg[F_PASSED_R1] < 0.0) eg[F_PASSED_R1] = 0.0;
        if (mg[F_PASSED_PROT_R1] < 0.0) mg[F_PASSED_PROT_R1] = 0.0;
        if (eg[F_PASSED_PROT_R1] < 0.0) eg[F_PASSED_PROT_R1] = 0.0;

        enforce_monotonic(&mg[F_PASSED_R1], 6);
        enforce_monotonic(&eg[F_PASSED_R1], 6);
        enforce_monotonic(&mg[F_PASSED_PROT_R1], 6);
        enforce_monotonic(&eg[F_PASSED_PROT_R1], 6);
        enforce_monotonic(&mg[F_KNIGHT_MOB0], 9);
        enforce_monotonic(&eg[F_KNIGHT_MOB0], 9);
        enforce_monotonic(&mg[F_BISHOP_MOB0], 14);
        enforce_monotonic(&eg[F_BISHOP_MOB0], 14);
        enforce_monotonic(&mg[F_ROOK_MOB0], 15);
        enforce_monotonic(&eg[F_ROOK_MOB0], 15);
        enforce_monotonic(&mg[F_QUEEN_MOB0], 28);
        enforce_monotonic(&eg[F_QUEEN_MOB0], 28);

        double val_mse = compute_mse_for_K(samples + n_train, n_val, mg, eg, K);
        if (val_mse < best_val_mse) {
            best_val_mse = val_mse;
            memcpy(best_mg, mg, sizeof(mg));
            memcpy(best_eg, eg, sizeof(eg));
        }

        if (epoch % 10 == 0 || epoch == epochs) {
            printf("Epoch %2d/%d -> Val MSE: %.6f (best: %.6f)\n", epoch, epochs, val_mse, best_val_mse);
            fflush(stdout);
        }
    }

    memcpy(mg, best_mg, sizeof(mg));
    memcpy(eg, best_eg, sizeof(eg));

    double final_train_mse = compute_mse_for_K(samples, n_train, mg, eg, K);
    double final_val_mse = compute_mse_for_K(samples + n_train, n_val, mg, eg, K);
    printf("\nOptimization Finished!\n");
    printf("Final MSE -> Train: %.6f (delta: %+.6f) | Val: %.6f (delta: %+.6f)\n\n",
           final_train_mse, final_train_mse - init_train_mse,
           final_val_mse, final_val_mse - init_val_mse);

    // Write tuned parameters to build/tuned_eval.inc
    FILE *out_fp = fopen("build/tuned_eval.inc", "w");
    if (!out_fp) out_fp = stdout;

    const char *pt_names[6] = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};
    fprintf(out_fp, "static const int mg_piece_val[6] = { %d, %d, %d, %d, %d, 0 };\n",
            (int)round(mg[F_MAT_PAWN]), (int)round(mg[F_MAT_KNIGHT]), (int)round(mg[F_MAT_BISHOP]),
            (int)round(mg[F_MAT_ROOK]), (int)round(mg[F_MAT_QUEEN]));
    fprintf(out_fp, "static const int eg_piece_val[6] = { %d, %d, %d, %d, %d, 0 };\n",
            (int)round(eg[F_MAT_PAWN]), (int)round(eg[F_MAT_KNIGHT]), (int)round(eg[F_MAT_BISHOP]),
            (int)round(eg[F_MAT_ROOK]), (int)round(eg[F_MAT_QUEEN]));
    fprintf(out_fp, "static const int phase_weight[6] = {  0,   1,   1,   2,    4, 0 };\n\n");

    fprintf(out_fp, "static const int16_t mg_table[6][64] = {\n");
    for (int pt = 0; pt < 6; ++pt) {
        fprintf(out_fp, "    { // %s\n", pt_names[pt]);
        for (int r = 0; r < 8; ++r) {
            fprintf(out_fp, "        ");
            for (int f = 0; f < 8; ++f) {
                int sq = r * 8 + f;
                int v = (int)round(mg[F_PSQT_START + pt * 64 + sq]);
                fprintf(out_fp, "%4d%s", v, (r == 7 && f == 7) ? "" : (f == 7 ? ",\n" : ", "));
            }
        }
        fprintf(out_fp, "\n    }%s\n", pt == 5 ? "" : ",");
    }
    fprintf(out_fp, "};\n\n");

    fprintf(out_fp, "static const int16_t eg_table[6][64] = {\n");
    for (int pt = 0; pt < 6; ++pt) {
        fprintf(out_fp, "    { // %s\n", pt_names[pt]);
        for (int r = 0; r < 8; ++r) {
            fprintf(out_fp, "        ");
            for (int f = 0; f < 8; ++f) {
                int sq = r * 8 + f;
                int v = (int)round(eg[F_PSQT_START + pt * 64 + sq]);
                fprintf(out_fp, "%4d%s", v, (r == 7 && f == 7) ? "" : (f == 7 ? ",\n" : ", "));
            }
        }
        fprintf(out_fp, "\n    }%s\n", pt == 5 ? "" : ",");
    }
    fprintf(out_fp, "};\n\n");

    fprintf(out_fp, "static const int32_t passed_rank_bonus[8] = {\n    S(0, 0), ");
    for (int i = 0; i < 6; ++i)
        fprintf(out_fp, "S(%d, %d), ", (int)round(mg[F_PASSED_R1 + i]), (int)round(eg[F_PASSED_R1 + i]));
    fprintf(out_fp, "S(0, 0)\n};\n\n");

    fprintf(out_fp, "static const int32_t passed_prot_bonus[8] = {\n    S(0, 0), ");
    for (int i = 0; i < 6; ++i)
        fprintf(out_fp, "S(%d, %d), ", (int)round(mg[F_PASSED_PROT_R1 + i]),
                (int)round(eg[F_PASSED_PROT_R1 + i]));
    fprintf(out_fp, "S(0, 0)\n};\n\n");

    fprintf(out_fp, "static const int32_t knight_mob[9] = {\n    ");
    for (int i = 0; i < 9; ++i)
        fprintf(out_fp, "S(%d, %d)%s", (int)round(mg[F_KNIGHT_MOB0 + i]), (int)round(eg[F_KNIGHT_MOB0 + i]),
                i == 8 ? "\n};\n\n" : ", ");

    fprintf(out_fp, "static const int32_t bishop_mob[14] = {\n    ");
    for (int i = 0; i < 14; ++i)
        fprintf(out_fp, "S(%d, %d)%s", (int)round(mg[F_BISHOP_MOB0 + i]), (int)round(eg[F_BISHOP_MOB0 + i]),
                i == 13 ? "\n};\n\n" : (i == 6 ? ",\n    " : ", "));

    fprintf(out_fp, "static const int32_t rook_mob[15] = {\n    ");
    for (int i = 0; i < 15; ++i)
        fprintf(out_fp, "S(%d, %d)%s", (int)round(mg[F_ROOK_MOB0 + i]), (int)round(eg[F_ROOK_MOB0 + i]),
                i == 14 ? "\n};\n\n" : (i == 7 ? ",\n    " : ", "));

    fprintf(out_fp, "static const int32_t queen_mob[28] = {\n    ");
    for (int i = 0; i < 28; ++i)
        fprintf(out_fp, "S(%d, %d)%s", (int)round(mg[F_QUEEN_MOB0 + i]), (int)round(eg[F_QUEEN_MOB0 + i]),
                i == 27 ? "\n};\n\n" : ((i % 7 == 6) ? ",\n    " : ", "));

    const char *scalar_names[] = {
        "pawn_phalanx_bonus",   "pawn_supported_bonus", "pawn_doubled_penalty",
        "pawn_isolated_penalty", "passed_free_bonus",   "passed_blocked_pen",
        "knight_outpost_bonus", "bishop_outpost_bonus", "bishop_pair_bonus",
        "rook_open_bonus",      "rook_semi_bonus",      "rook_seventh_bonus",
        "pawn_threat_minor",    "pawn_threat_major",    "minor_threat_major",
        "king_shield_bonus",    "king_open_penalty",    "tempo_bonus"};
    for (int i = 0; i < 18; ++i) {
        int idx = F_PAWN_PHALANX + i;
        fprintf(out_fp, "static const int32_t %-22s = S(%d, %d);\n", scalar_names[i], (int)round(mg[idx]),
                (int)round(eg[idx]));
    }

    if (out_fp != stdout) {
        fclose(out_fp);
        printf("Tuned evaluation parameters successfully saved to build/tuned_eval.inc\n");
    }

    free(samples);
    return 0;
}
