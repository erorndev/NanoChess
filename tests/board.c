#include "attacks.h"
#include "board.h"
#include "book.h"
#include "movegen.h"
#include "platform.h"
#include "types.h"
#include "zobrist.h"

#undef NDEBUG
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static int thread_worker_test(void *arg) {
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add(counter, 1);
    return 0;
}

static void test_threads(void) {
    atomic_int counter = 0;
    thread_t threads[4];
    for (int i = 0; i < 4; ++i) {
        assert(thread_create(&threads[i], thread_worker_test, &counter));
    }
    for (int i = 0; i < 4; ++i) {
        thread_join(threads[i]);
    }
    assert(atomic_load(&counter) == 4);
}

static void test_types(void) {
    move_t m1 = make_move(SQ_E2, SQ_E4, MOVE_DOUBLE_PAWN);
    assert(move_from(m1) == SQ_E2);
    assert(move_to(m1) == SQ_E4);
    assert(move_flags(m1) == MOVE_DOUBLE_PAWN);
    assert(!move_is_capture(m1));
    assert(!move_is_promotion(m1));

    move_t m2 = make_move(SQ_E7, SQ_E8, MOVE_PROM_CAP_Q);
    assert(move_from(m2) == SQ_E7);
    assert(move_to(m2) == SQ_E8);
    assert(move_is_capture(m2));
    assert(move_is_promotion(m2));
    assert(move_promoted_type(m2) == QUEEN);

    assert(make_square(FILE_E, RANK_4) == SQ_E4);
    assert(square_file(SQ_E4) == FILE_E);
    assert(square_rank(SQ_E4) == RANK_4);
    assert(piece_type(W_QUEEN) == QUEEN);
    assert(piece_color(W_QUEEN) == WHITE);
    assert(piece_type(B_ROOK) == ROOK);
    assert(piece_color(B_ROOK) == BLACK);
}

static void test_platform_intrinsics(void) {
    bitboard_t bb = (1ULL << SQ_A1) | (1ULL << SQ_H8) | (1ULL << SQ_E4);
    assert(popcount64(bb) == 3);
    assert(lsb64(bb) == SQ_A1);
    assert(msb64(bb) == SQ_H8);

    bitboard_t single = 1ULL << SQ_D5;
    assert(popcount64(single) == 1);
    assert(lsb64(single) == SQ_D5);
    assert(msb64(single) == SQ_D5);
}

static void test_fen_parsing(void) {
    struct Board b;

    assert(set_fen(&b, START_FEN));
    assert(b.side == WHITE);
    assert(b.history[0].castling == CASTLE_ALL);
    assert(b.history[0].ep == SQ_NONE);
    assert(b.history[0].halfmove == 0);
    assert(popcount64(b.all) == 32);
    assert(popcount64(b.occ[WHITE]) == 16);
    assert(popcount64(b.occ[BLACK]) == 16);
    assert(b.mailbox[SQ_E1] == W_KING);
    assert(b.mailbox[SQ_E8] == B_KING);
    assert(b.mailbox[SQ_D1] == W_QUEEN);
    assert(b.mailbox[SQ_D8] == B_QUEEN);

    const char *kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    assert(set_fen(&b, kiwi));
    assert(b.side == WHITE);
    assert(b.history[0].castling == CASTLE_ALL);
    assert(b.mailbox[SQ_E5] == W_KNIGHT);
    assert(b.mailbox[SQ_B4] == B_PAWN);
    assert(b.mailbox[SQ_H3] == B_PAWN);

    const char *ep_fen = "8/8/8/3Pp3/8/8/8/4K2k w - e6 0 1";
    assert(set_fen(&b, ep_fen));
    assert(b.history[0].ep == SQ_E6);
    assert(b.history[0].castling == CASTLE_NONE);
}

static void test_zobrist_keys(void) {
    struct Board b1, b2;
    set_fen(&b1, START_FEN);
    set_fen(&b2, START_FEN);
    assert(board_key(&b1) == board_key(&b2));

    set_fen(&b2, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    assert(board_key(&b1) != board_key(&b2));

    set_fen(&b2, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w Qkq - 0 1");
    assert(board_key(&b1) != board_key(&b2));
}

static void test_opening_book(void) {
    init_book();
    struct Board b;

    set_fen(&b, START_FEN);
    move_t m1 = probe_book(&b);
    assert(m1 != 0);
    char mbuf[6];
    move_to_str(m1, mbuf);
    assert(strcmp(mbuf, "e2e4") == 0 || strcmp(mbuf, "d2d4") == 0 ||
           strcmp(mbuf, "c2c4") == 0 || strcmp(mbuf, "g1f3") == 0);

    (void)do_move(&b, parse_move(&b, "e2e4"));
    move_t m2 = probe_book(&b);
    assert(m2 != 0);

    (void)do_move(&b, parse_move(&b, "c7c5"));
    (void)do_move(&b, parse_move(&b, "g1f3"));
    move_t m3 = probe_book(&b);
    assert(m3 != 0);

    set_fen(&b, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    assert(probe_book(&b) == 0);

    option_own_book = false;
    set_fen(&b, START_FEN);
    assert(probe_book(&b) == 0);
    option_own_book = true;
}

int main(void) {
    init_zobrist();
    init_attacks();
    test_types();
    test_platform_intrinsics();
    test_fen_parsing();
    test_zobrist_keys();
    test_threads();
    test_opening_book();
    printf("board: all passed\n");
    return 0;
}
