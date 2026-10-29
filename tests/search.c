#undef NDEBUG
#include "search.h"
#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "movegen.h"
#include "platform.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                \
            fflush(stdout);                                                                        \
            _exit(1);                                                                              \
        }                                                                                          \
    } while (0)

static void test_see(void) {
    struct Board b;
    // White Queen on d1, Black Pawn on d5 defended by Black Pawn on e6
    set_fen(&b, "4k3/8/4p3/3p4/8/8/8/3QK3 w - - 0 1");
    move_t qxd5 = parse_move(&b, "d1d5");
    CHECK(qxd5 != 0);
    CHECK(!see(&b, qxd5, 0));

    // White Rook on d2, White Queen on d1 (X-ray battery), Black Rook on d5 defended by Pawn e6
    set_fen(&b, "4k3/8/4p3/3r4/8/8/3R4/3QK3 w - - 0 1");
    move_t rxd5 = parse_move(&b, "d2d5");
    CHECK(rxd5 != 0);
    CHECK(see(&b, rxd5, 0));
}

static void test_continuation_history(void) {
    clear_tt();
    clear_search();

    CHECK(conthist_entry(NO_PIECE, SQ_E4, W_KNIGHT, SQ_F3) == 0);
    CHECK(conthist_entry(B_PAWN, SQ_E5, NO_PIECE, SQ_F3) == 0);

    struct Board b;
    set_fen(&b, START_FEN);
    struct SearchLimits lim = {
        .depth = 7,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    (void)search_position(&b, &lim);

    int nonzero_1ply = 0;
    int nonzero_2ply = 0;
    for (int p1 = 0; p1 < NO_PIECE; ++p1) {
        for (int s1 = 0; s1 < 64; ++s1) {
            for (int p2 = 0; p2 < NO_PIECE; ++p2) {
                for (int s2 = 0; s2 < 64; ++s2) {
                    int v = conthist_entry((enum Piece)p1, (enum Square)s1, (enum Piece)p2,
                                           (enum Square)s2);
                    CHECK(v >= -16384 && v <= 16384);
                    if (v != 0) {
                        if (piece_color((enum Piece)p1) != piece_color((enum Piece)p2))
                            nonzero_1ply++;
                        else
                            nonzero_2ply++;
                    }
                }
            }
        }
    }
    CHECK(nonzero_1ply > 0);
    CHECK(nonzero_2ply > 0);

    clear_search();
    for (int p1 = 0; p1 < NO_PIECE; ++p1)
        for (int s1 = 0; s1 < 64; ++s1)
            CHECK(conthist_entry((enum Piece)p1, (enum Square)s1, W_KNIGHT, SQ_F3) == 0);
}

static void test_qsearch_check_and_see(void) {
    struct Board b;
    // 1... Rxb2?? wins an undefended Queen but blunders into 2. Rxd8# inside qsearch
    set_fen(&b, "1r1n2k1/5ppp/8/4p3/8/6PP/1Q3P1K/3R4 b - - 0 1");
    move_t blunder_rxb2 = parse_move(&b, "b8b2");
    CHECK(blunder_rxb2 != 0);

    clear_tt();
    clear_search();
    struct SearchLimits lim = {
        .depth = 1,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    struct SearchResult res = search_position(&b, &lim);
    CHECK(res.best_move != blunder_rxb2);
}

static void test_singular_extension(void) {
    struct Board b;
    set_fen(&b, START_FEN);
    clear_tt();
    clear_search();
    struct SearchLimits shallow = {
        .depth = 6,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    (void)search_position(&b, &shallow);
    CHECK(last_singular_extensions() == 0);

    // 1. cxd5 forces singular recapture 1... exd5 at ply 1 (all alternatives lose a full Rook)
    set_fen(&b, "6k1/5ppp/4p3/3r4/2P5/8/5PPP/3R2K1 w - - 0 1");
    clear_tt();
    clear_search();
    struct SearchLimits deep = {
        .depth = 10,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    struct SearchResult res = search_position(&b, &deep);
    CHECK(res.best_move == parse_move(&b, "c4d5"));
    CHECK(last_singular_extensions() > 0);
}

static void test_singular_root_depth_cap(void) {
    const char *fen = "r1bq1rk1/pppnnppp/4p3/3pP3/1b1P4/2NB3N/PPP2PPP/R1BQK2R w KQ - 3 7";
    struct Board b;
    struct SearchLimits warm = {.depth = 16, .uci_output = false};

    set_fen(&b, fen);
    clear_tt();
    clear_search();
    (void)search_position(&b, &warm);
    (void)do_move(&b, parse_move(&b, "d3h7"));
    (void)do_move(&b, parse_move(&b, "g8h7"));
    struct SearchLimits timed = {.depth = 0, .soft_time = 20, .uci_output = false};
    struct SearchResult r_timed = search_position(&b, &timed);
    uint64_t ext_timed = last_singular_extensions();

    set_fen(&b, fen);
    clear_tt();
    clear_search();
    (void)search_position(&b, &warm);
    (void)do_move(&b, parse_move(&b, "d3h7"));
    (void)do_move(&b, parse_move(&b, "g8h7"));
    struct SearchLimits fixed = {.depth = r_timed.depth, .uci_output = false};
    struct SearchResult r_fixed = search_position(&b, &fixed);
    uint64_t ext_fixed = last_singular_extensions();

    CHECK(r_timed.nodes == r_fixed.nodes);
    CHECK(ext_timed == ext_fixed);
}

static void assert_best_move(const char *fen, int depth, const char *expected_uci) {
    struct Board b;
    CHECK(set_fen(&b, fen));
    clear_tt();
    clear_search();

    struct SearchLimits lim = {
        .depth = depth,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    struct SearchResult res = search_position(&b, &lim);
    char mbuf[6];
    move_to_str(res.best_move, mbuf);
    if (strcmp(mbuf, expected_uci) != 0) {
        printf("FAIL: fen='%s' depth=%d expected=%s got=%s (score=%d)\n", fen, depth, expected_uci,
               mbuf, res.score);
        fflush(stdout);
        _exit(1);
    }
}

static void test_time_manager(void) {
    struct Board b;
    set_fen(&b, START_FEN);

    struct SearchLimits lim_op = allocate_time(&b, 60000, 1000, 0);
    CHECK(lim_op.soft_time > 1500 && lim_op.soft_time < 3000);
    CHECK(lim_op.hard_time >= lim_op.soft_time);
    CHECK(lim_op.hard_time <= 60000 - 15);

    set_fen(&b, "8/4k3/8/8/3P4/8/4K3/8 w - - 0 1");
    struct SearchLimits lim_eg = allocate_time(&b, 60000, 1000, 0);
    CHECK(lim_eg.soft_time > lim_op.soft_time * 15 / 10);
    CHECK(lim_eg.hard_time >= lim_eg.soft_time);
    CHECK(lim_eg.hard_time <= 60000 - 15);

    set_fen(&b, START_FEN);
    struct SearchLimits lim_blitz = allocate_time(&b, 10000, 100, 0);
    CHECK(lim_blitz.soft_time >= 350 && lim_blitz.soft_time <= 500);
    CHECK(lim_blitz.hard_time >= lim_blitz.soft_time);

    struct SearchLimits lim_low = allocate_time(&b, 10, 0, 0);
    CHECK(lim_low.hard_time <= 10);
    CHECK(lim_low.soft_time <= lim_low.hard_time);

    struct SearchLimits lim_1ms = allocate_time(&b, 1, 0, 0);
    CHECK(lim_1ms.hard_time <= 1);
    CHECK(lim_1ms.soft_time <= lim_1ms.hard_time);

    option_move_overhead = 100;
    struct SearchLimits lim_overhead = allocate_time(&b, 60000, 1000, 0);
    CHECK(lim_overhead.hard_time <= 60000 - 100);
    option_move_overhead = 15;

    set_fen(&b, "8/8/8/8/8/q7/8/Kr6 w - - 0 1");
    clear_tt();
    clear_search();
    struct SearchLimits lim_forced = {
        .depth = 0,
        .soft_time = 5000,
        .hard_time = 5000,
        .max_nodes = 0,
        .uci_output = false,
    };
    uint64_t t0 = get_time_ms();
    struct SearchResult res_forced = search_position(&b, &lim_forced);
    uint64_t dt = get_time_ms() - t0;
    CHECK(res_forced.best_move == parse_move(&b, "a1b1"));
    CHECK(res_forced.nodes == 1);
    CHECK(dt < 15);
}

struct StopTestArgs {
    struct Board board;
    struct SearchResult result;
};

static int stop_test_worker(void *arg) {
    struct StopTestArgs *a = (struct StopTestArgs *)arg;
    struct SearchLimits lim = {
        .depth = 0,
        .soft_time = 0,
        .hard_time = 0,
        .max_nodes = 0,
        .uci_output = false,
    };
    a->result = search_position(&a->board, &lim);
    return 0;
}

static void test_stop_request(void) {
    clear_tt();
    clear_search();

    struct StopTestArgs args;
    set_fen(&args.board, START_FEN);
    args.result = (struct SearchResult){0, 0, 0, 0};

    search_clear_stop_request();
    thread_t t;
    uint64_t t0 = get_time_ms();
    CHECK(thread_create(&t, stop_test_worker, &args));
    sleep_ms(50);
    search_request_stop();
    thread_join(t);
    uint64_t dt = get_time_ms() - t0;

    CHECK(args.result.best_move != 0);
    CHECK(args.result.depth >= 1);
    CHECK(dt < 1000);

    search_request_stop();
    args.result = (struct SearchResult){0, 0, 0, 0};
    stop_test_worker(&args);
    CHECK(args.result.best_move != 0);
    search_clear_stop_request();
}

int main(void) {
    init_zobrist();
    init_attacks();
    init_eval();
    init_tt(16);
    init_search();

    test_time_manager();
    test_stop_request();
    test_see();
    test_continuation_history();
    test_qsearch_check_and_see();
    test_singular_extension();
    test_singular_root_depth_cap();

    // Back-rank Mate in 1
    assert_best_move("6k1/5ppp/8/8/8/8/8/3R2K1 w - - 0 1", 4, "d1d8");

    // Scholar's Mate in 1
    assert_best_move("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 4,
                     "h5f7");

    // Morphy's Opera Game Mate in 2 (1. Qb8+ Nxb8 2. Rd8#)
    assert_best_move("4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16", 6, "b3b8");

    // Battery Mate in 1 (Qxg7#)
    assert_best_move("5rk1/6pp/8/5N2/8/8/1Q6/6K1 w - - 0 1", 4, "b2g7");

    // Philidor Smothered Mate in 2 (1. Qg8+ Rxg8 2. Nf7#)
    assert_best_move("5r1k/6pp/7N/8/8/1Q6/6PP/6K1 w - - 0 1", 6, "b3g8");

    // Anastasias Mate in 2 (1. Qxh7+ Kxh7 2. Rh3#)
    assert_best_move("5r1k/pp2N1pp/8/8/8/3QR3/PP3PPP/6K1 w - - 0 1", 6, "d3h7");

    // Tactical Knight Fork winning Queen (1. Nxc7+ Kd8 2. Nxa6)
    assert_best_move("4k2r/ppp2ppp/q7/3N4/8/8/PPP2PPP/R3K2R w KQk - 0 1", 5, "d5c7");

    // Multi-threaded Lazy SMP verification
    set_threads(4);
    CHECK(option_threads == 4);
    assert_best_move("4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16", 6, "b3b8");
    assert_best_move("5r1k/pp2N1pp/8/8/8/3QR3/PP3PPP/6K1 w - - 0 1", 6, "d3h7");

    struct Board b;
    set_fen(&b, START_FEN);
    clear_tt();
    clear_search();
    struct SearchLimits lim = {
        .depth = 0,
        .soft_time = 40,
        .hard_time = 80,
        .max_nodes = 0,
        .uci_output = false,
    };
    struct SearchResult res = search_position(&b, &lim);
    CHECK(res.best_move != 0);
    CHECK(res.nodes > 1000);

    set_threads(1);
    CHECK(option_threads == 1);

    free_tt();
    printf("search: all passed\n");
    return 0;
}
