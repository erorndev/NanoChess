#pragma once

#include "board.h"
#include "types.h"

extern bool option_own_book;

void init_book(void);
[[nodiscard]] move_t probe_book(const struct Board *b);
