#undef NDEBUG
#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "platform.h"
#include "types.h"
#include "zobrist.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                \
            fflush(stdout);                                                                        \
            _exit(1);                                                                              \
        }                                                                                          \
    } while (0)

static void assert_symmetry(const char *fen_w, const char *fen_b) {
    struct Board bw, bb;
    CHECK(set_fen(&bw, fen_w));
    CHECK(set_fen(&bb, fen_b));
    int ew = evaluate(&bw);
    int eb = evaluate(&bb);
    CHECK(ew == eb);
}

int main(void) {
    init_zobrist();
    init_attacks();
    init_eval();

    struct Board b;
    set_fen(&b, START_FEN);
    int start_eval = evaluate(&b);
    CHECK(start_eval > 0 && start_eval < 30);

    // Symmetry checks (White to move vs color-flipped Black to move)
    assert_symmetry("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");

    assert_symmetry("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                    "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1");

    assert_symmetry("4k3/8/8/3P4/8/8/8/4K3 w - - 0 1", "4k3/8/8/8/3p4/8/8/4K3 b - - 0 1");

    // Insufficient material draws
    set_fen(&b, "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    CHECK(evaluate(&b) == 0);
    set_fen(&b, "4k3/8/8/8/8/5N2/8/4K3 w - - 0 1");
    CHECK(evaluate(&b) == 0);
    set_fen(&b, "4k3/8/3b4/8/8/8/8/4K3 b - - 0 1");
    CHECK(evaluate(&b) == 0);

    // Central knight vs rim knight
    struct Board b_center, b_rim;
    set_fen(&b_center, "4k3/pppppppp/8/8/4N3/8/PPPPPPPP/4K3 w - - 0 1");
    set_fen(&b_rim, "4k3/pppppppp/8/8/8/8/PPPPPPPP/N3K3 w - - 0 1");
    CHECK(evaluate(&b_center) > evaluate(&b_rim) + 50);

    // Passed pawn rank progression
    struct Board b_r6, b_r3;
    set_fen(&b_r6, "4k3/8/3P4/8/8/8/8/4K3 w - - 0 1");
    set_fen(&b_r3, "4k3/8/8/8/8/3P4/8/4K3 w - - 0 1");
    CHECK(evaluate(&b_r6) > evaluate(&b_r3) + 80);

    // Rook on open file vs blocked file
    struct Board b_open, b_closed;
    set_fen(&b_open, "3rk3/ppp1pppp/8/8/8/8/PPP1PPPP/3RK3 w - - 0 1");
    set_fen(&b_closed, "3rk3/ppp1pppp/8/8/8/8/PPPP1PPP/3RK3 w - - 0 1");
    CHECK(evaluate(&b_open) > evaluate(&b_closed));

    // Pawn threat on enemy minor piece
    struct Board b_threat, b_nothreat;
    set_fen(&b_threat, "r3k2r/pppppppp/8/3n4/2P5/8/PP1PPPPP/R3K2R w KQkq - 0 1");
    set_fen(&b_nothreat, "r3k2r/pppppppp/8/3n4/1P6/8/P1PPPPPP/R3K2R w KQkq - 0 1");
    CHECK(evaluate(&b_threat) > evaluate(&b_nothreat) + 15);

    // Protected passed pawn vs isolated passed pawn
    struct Board b_prot_pass, b_iso_pass;
    set_fen(&b_prot_pass, "4k3/8/8/3PP3/2P5/8/8/4K3 w - - 0 1");
    set_fen(&b_iso_pass, "4k3/8/8/3P4/8/2P1P3/8/4K3 w - - 0 1");
    CHECK(evaluate(&b_prot_pass) > evaluate(&b_iso_pass) + 25);

    // KNN vs K dead draw scaling
    struct Board b_knn;
    set_fen(&b_knn, "4k3/8/8/8/8/8/8/3KNN2 w - - 0 1");
    CHECK(evaluate(&b_knn) == 0);

    printf("eval: all passed\n");
    return 0;
}
