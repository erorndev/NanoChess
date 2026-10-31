#include "nnue.h"
#include "board.h"
#include "platform.h"

#include "weights.inl"

static inline int nnue_feature(enum Color persp, enum Piece p, enum Square sq) {
    if (persp == WHITE) return (int)p * 64 + (int)sq;
    int rel_p = (p >= B_PAWN) ? ((int)p - 6) : ((int)p + 6);
    return rel_p * 64 + ((int)sq ^ 56);
}

static inline int32_t screlu(int32_t x) {
    if (x <= 0) return 0;
    if (x > 127) x = 127;
    return (x * x) >> 7;
}

void nnue_refresh(struct Board *b) {
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        b->acc[WHITE][i] = nnue_ft_biases[i];
        b->acc[BLACK][i] = nnue_ft_biases[i];
    }
    for (int p = 0; p < 12; ++p) {
        bitboard_t bb = b->pieces[p];
        while (bb) {
            enum Square sq = (enum Square)lsb64(bb);
            nnue_add_piece(b, (enum Piece)p, sq);
            bb &= bb - 1;
        }
    }
}

void nnue_add_piece(struct Board *b, enum Piece p, enum Square sq) {
    int fw = nnue_feature(WHITE, p, sq);
    int fb = nnue_feature(BLACK, p, sq);
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        b->acc[WHITE][i] += nnue_ft_weights[fw][i];
        b->acc[BLACK][i] += nnue_ft_weights[fb][i];
    }
}

void nnue_sub_piece(struct Board *b, enum Piece p, enum Square sq) {
    int fw = nnue_feature(WHITE, p, sq);
    int fb = nnue_feature(BLACK, p, sq);
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        b->acc[WHITE][i] -= nnue_ft_weights[fw][i];
        b->acc[BLACK][i] -= nnue_ft_weights[fb][i];
    }
}

void nnue_move_piece(struct Board *b, enum Piece p, enum Square from, enum Square to) {
    int fw_from = nnue_feature(WHITE, p, from);
    int fw_to = nnue_feature(WHITE, p, to);
    int fb_from = nnue_feature(BLACK, p, from);
    int fb_to = nnue_feature(BLACK, p, to);
    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        b->acc[WHITE][i] += (int16_t)nnue_ft_weights[fw_to][i] - (int16_t)nnue_ft_weights[fw_from][i];
        b->acc[BLACK][i] += (int16_t)nnue_ft_weights[fb_to][i] - (int16_t)nnue_ft_weights[fb_from][i];
    }
}

int evaluate_nnue(const struct Board *b) {
    enum Color us = b->side;
    enum Color them = (enum Color)(us ^ 1);
    int32_t act[NNUE_HIDDEN * 2];

    for (int i = 0; i < NNUE_HIDDEN; ++i) {
        act[i] = screlu(b->acc[us][i]);
        act[NNUE_HIDDEN + i] = screlu(b->acc[them][i]);
    }

    int32_t hidden[NNUE_HIDDEN];
    for (int k = 0; k < NNUE_HIDDEN; ++k) hidden[k] = nnue_l1_biases[k];

    for (int i = 0; i < NNUE_HIDDEN * 2; ++i) {
        int32_t a = act[i];
        if (!a) continue;
        const int8_t *w = nnue_l1_weights[i];
        for (int k = 0; k < NNUE_HIDDEN; ++k) hidden[k] += a * w[k];
    }

    int32_t out = nnue_l2_bias;
    for (int k = 0; k < NNUE_HIDDEN; ++k) {
        int32_t h_act = screlu(hidden[k] >> 6);
        out += h_act * nnue_l2_weights[k];
    }

    return (int)((out * 350) / 16256);
}
