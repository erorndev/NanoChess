#pragma once

#include "types.h"
#include <string.h>

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define MAX_PLY 1024

struct StateInfo {
    uint8_t castling;
    enum Square ep;
    uint8_t halfmove;
    enum Piece captured;
    uint64_t key;
};

struct Board {
    bitboard_t pieces[12];
    bitboard_t occ[COLOR_NB];
    bitboard_t all;
    enum Piece mailbox[64];
    enum Color side;
    int16_t acc[COLOR_NB][32];
    struct StateInfo history[MAX_PLY];
    int ply;
};

void init_board(struct Board *b);
bool set_fen(struct Board *b, const char *fen);
void print_board(const struct Board *b);

[[nodiscard]] bool is_square_attacked(const struct Board *b, enum Square sq, enum Color by_side);
[[nodiscard]] bool in_check(const struct Board *b);

[[nodiscard]] bool do_move(struct Board *b, move_t m);
void undo_move(struct Board *b, move_t m);
void do_null_move(struct Board *b);
void undo_null_move(struct Board *b);

[[nodiscard]] static inline uint64_t board_key(const struct Board *b) {
    return b->history[b->ply].key;
}

[[nodiscard]] static inline bool is_repetition(const struct Board *b) {
    int start = b->ply - b->history[b->ply].halfmove;
    if (start < 0)
        start = 0;
    uint64_t cur = b->history[b->ply].key;
    for (int i = b->ply - 2; i >= start; i -= 2) {
        if (b->history[i].key == cur)
            return true;
    }
    return false;
}
