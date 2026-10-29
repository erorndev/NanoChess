#pragma once

#include "board.h"
#include "types.h"

void init_eval(void);
[[nodiscard]] int evaluate(const struct Board *b);
