#pragma once

#include "types.h"

#define NNUE_FEATURES 768
#define NNUE_HIDDEN 32
#define NNUE_GATE_MARGIN 120

struct Board;

void nnue_refresh(struct Board *b);
void nnue_add_piece(struct Board *b, enum Piece p, enum Square sq);
void nnue_sub_piece(struct Board *b, enum Piece p, enum Square sq);
void nnue_move_piece(struct Board *b, enum Piece p, enum Square from, enum Square to);
[[nodiscard]] int evaluate_nnue(const struct Board *b);
