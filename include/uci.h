#pragma once

#include "board.h"
#include "search.h"

void uci_loop(int argc, char **argv);
struct SearchLimits allocate_time(const struct Board *b, uint64_t time_left, uint64_t inc, int movestogo);
