#pragma once

#include "board.h"
#include "search.h"
#include "types.h"

extern int option_move_overhead;

[[nodiscard]] uint64_t get_time_ms(void);
void sleep_ms(unsigned ms);
struct SearchLimits allocate_time(const struct Board *b, uint64_t time_left, uint64_t inc, int movestogo);
