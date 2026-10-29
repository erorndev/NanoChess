#pragma once

#include "board.h"
#include "types.h"

#define INF 32000
#define MATE_VAL 31000
#define MATE_THRESHOLD 30000
#define MAX_SEARCH_DEPTH 64
#define MAX_THREADS 64

extern int option_threads;

struct SearchLimits {
    int depth;
    uint64_t soft_time;
    uint64_t hard_time;
    uint64_t max_nodes;
    bool uci_output;
};

struct SearchResult {
    move_t best_move;
    int score;
    int depth;
    uint64_t nodes;
};

void init_search(void);
void clear_search(void);
void set_threads(int count);

void search_request_stop(void);
void search_clear_stop_request(void);

[[nodiscard]] bool see(const struct Board *b, move_t m, int threshold);
[[nodiscard]] int conthist_entry(enum Piece prev_pc, enum Square prev_to, enum Piece pc,
                                 enum Square to);
[[nodiscard]] uint64_t last_singular_extensions(void);
struct SearchResult search_position(struct Board *b, const struct SearchLimits *limits);
