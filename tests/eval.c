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

    assert_symmetry("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");

    assert_symmetry("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                    "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1");

    assert_symmetry("4k3/8/8/3P4/8/8/8/4K3 w - - 0 1", "4k3/8/8/8/3p4/8/8/4K3 b - - 0 1");

    set_fen(&b, "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    CHECK(evaluate(&b) == 0);
    set_fen(&b, "4k3/8/8/8/8/5N2/8/4K3 w - - 0 1");
    CHECK(evaluate(&b) == 0);
    set_fen(&b, "4k3/8/3b4/8/8/8/8/4K3 b - - 0 1");
    CHECK(evaluate(&b) == 0);

    struct Board b_center, b_rim;
    set_fen(&b_center, "4k3/pppppppp/8/8/4N3/8/PPPPPPPP/4K3 w - - 0 1");
    set_fen(&b_rim, "4k3/pppppppp/8/8/8/8/PPPPPPPP/N3K3 w - - 0 1");
    CHECK(evaluate(&b_center) > evaluate(&b_rim) + 50);

    struct Board b_r6, b_r3;
    set_fen(&b_r6, "4k3/8/3P4/8/8/8/8/4K3 w - - 0 1");
    set_fen(&b_r3, "4k3/8/8/8/8/3P4/8/4K3 w - - 0 1");
    CHECK(evaluate(&b_r6) > evaluate(&b_r3) + 80);

    struct Board b_open, b_closed;
    set_fen(&b_open, "3rk3/ppp1pppp/8/8/8/8/PPP1PPPP/3RK3 w - - 0 1");
    set_fen(&b_closed, "3rk3/ppp1pppp/8/8/8/8/PPPP1PPP/3RK3 w - - 0 1");
    CHECK(evaluate(&b_open) > evaluate(&b_closed));

    struct Board b_threat, b_nothreat;
    set_fen(&b_threat, "r3k2r/pppppppp/8/3n4/2P5/8/PP1PPPPP/R3K2R w KQkq - 0 1");
    set_fen(&b_nothreat, "r3k2r/pppppppp/8/3n4/1P6/8/P1PPPPPP/R3K2R w KQkq - 0 1");
    CHECK(evaluate(&b_threat) > evaluate(&b_nothreat) + 15);

    struct Board b_prot_pass, b_iso_pass;
    set_fen(&b_prot_pass, "4k3/8/8/3PP3/2P5/8/8/4K3 w - - 0 1");
    set_fen(&b_iso_pass, "4k3/8/8/3P4/8/2P1P3/8/4K3 w - - 0 1");
    CHECK(evaluate(&b_prot_pass) > evaluate(&b_iso_pass) + 25);

    struct Board b_knn;
    set_fen(&b_knn, "4k3/8/8/8/8/8/8/3KNN2 w - - 0 1");
    CHECK(evaluate(&b_knn) == 0);

    struct Board b_kr_kn, b_kr_k;
    set_fen(&b_kr_kn, "4k3/8/8/8/8/5n2/8/R3K3 w - - 0 1");
    set_fen(&b_kr_k, "4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    CHECK(evaluate(&b_kr_kn) < 100);
    CHECK(evaluate(&b_kr_k) > 250);

    struct Board b_wb, b_rb;
    set_fen(&b_wb, "k7/8/8/8/8/P7/8/2B1K3 w - - 0 1"); 
    set_fen(&b_rb, "k7/8/8/8/8/P7/8/1B2K3 w - - 0 1"); 
    CHECK(evaluate(&b_wb) < 50);
    CHECK(evaluate(&b_rb) > evaluate(&b_wb) + 150);

    struct Board b_ocb, b_scb;
    set_fen(&b_ocb, "4k3/8/8/3P4/8/2B5/4b3/4K3 w - - 0 1"); 
    set_fen(&b_scb, "4k3/8/8/3P4/8/2B5/5b2/4K3 w - - 0 1"); 
    CHECK(evaluate(&b_ocb) < evaluate(&b_scb));

    struct Board b_kr_krp, b_kpk;
    set_fen(&b_kr_krp, "4k3/8/8/8/3P4/8/3r4/4K2R w - - 0 1");
    CHECK(evaluate(&b_kr_krp) > 0 && evaluate(&b_kr_krp) < 150);
    set_fen(&b_kpk, "4k3/8/4K3/4P3/8/8/8/8 w - - 0 1");
    CHECK(evaluate(&b_kpk) > 150);

    struct Board b_shuf0, b_shuf60;
    set_fen(&b_shuf0, "4k3/8/8/8/8/2B5/3r4/R3K3 w - - 0 1");
    set_fen(&b_shuf60, "4k3/8/8/8/8/2B5/3r4/R3K3 w - - 60 1");
    CHECK(evaluate(&b_shuf60) < evaluate(&b_shuf0));

    struct Board b_free_n, b_trap_n;
    set_fen(&b_free_n, "4k3/8/8/8/N7/2P5/1P6/4K3 w - - 0 1");
    set_fen(&b_trap_n, "4k3/2p5/1p6/8/N7/2P5/1P6/4K3 w - - 0 1");
    CHECK(evaluate(&b_free_n) > evaluate(&b_trap_n));

    struct Board b_free_b, b_trap_b;
    set_fen(&b_free_b, "4k3/B7/8/8/8/8/PP6/4K3 w - - 0 1");
    set_fen(&b_trap_b, "4k3/B1p5/1p6/8/8/8/PP6/4K3 w - - 0 1");
    CHECK(evaluate(&b_free_b) > evaluate(&b_trap_b));

    struct Board b_free_r, b_trap_r;
    set_fen(&b_free_r, "4k3/8/8/8/8/8/7P/4KR2 w - - 0 1");
    set_fen(&b_trap_r, "4k3/8/8/8/8/8/7P/5K1R w - - 0 1");
    CHECK(evaluate(&b_free_r) > evaluate(&b_trap_r));

    struct Board b_hanging, b_defended;
    set_fen(&b_hanging, "4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1");
    set_fen(&b_defended, "4k3/8/2p5/3n4/8/8/8/3RK3 w - - 0 1");
    CHECK(evaluate(&b_hanging) > evaluate(&b_defended));

    struct Board b_pawn_rook, b_nopawn_rook;
    set_fen(&b_pawn_rook, "4k3/8/8/3r4/2P5/8/8/4K3 w - - 0 1");
    set_fen(&b_nopawn_rook, "4k3/8/8/3r4/1P6/8/8/4K3 w - - 0 1");
    CHECK(evaluate(&b_pawn_rook) > evaluate(&b_nopawn_rook));

    printf("eval: all passed\n");
    return 0;
}
