#pragma once

#include "board.h"
#include "types.h"

struct MoveList {
    move_t moves[256];
    int scores[256];
    int count;
};

void generate_moves(const struct Board *b, struct MoveList *list);
void generate_captures(const struct Board *b, struct MoveList *list);
void generate_legal(struct Board *b, struct MoveList *list);

void move_to_str(move_t m, char *buf);
[[nodiscard]] move_t parse_move(struct Board *b, const char *str);

[[nodiscard]] uint64_t perft(struct Board *b, int depth);
