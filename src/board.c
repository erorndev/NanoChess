#include "board.h"
#include "attacks.h"
#include "nnue.h"
#include "platform.h"
#include "zobrist.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

// clang-format off
static const char piece_chars[13] = "PNBRQKpnbrqk.";

static const uint8_t castling_mask[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};
// clang-format on

void init_board(struct Board *b) {
    memset(b, 0, sizeof(*b));
    for (int i = 0; i < 64; ++i)
        b->mailbox[i] = NO_PIECE;
    b->history[0].ep = SQ_NONE;
}

static uint64_t compute_key(const struct Board *b) {
    uint64_t k = 0;
    for (int p = 0; p < 12; ++p) {
        bitboard_t bb = b->pieces[p];
        while (bb) {
            int sq = lsb64(bb);
            k ^= zobrist.pieces[p][sq];
            bb &= bb - 1;
        }
    }
    if (b->side == BLACK)
        k ^= zobrist.side;
    k ^= zobrist.castling[b->history[b->ply].castling];
    if (b->history[b->ply].ep != SQ_NONE) {
        k ^= zobrist.ep[square_file(b->history[b->ply].ep)];
    }
    return k;
}

static enum Piece char_to_piece(char c) {
    switch (c) {
    case 'P':
        return W_PAWN;
    case 'N':
        return W_KNIGHT;
    case 'B':
        return W_BISHOP;
    case 'R':
        return W_ROOK;
    case 'Q':
        return W_QUEEN;
    case 'K':
        return W_KING;
    case 'p':
        return B_PAWN;
    case 'n':
        return B_KNIGHT;
    case 'b':
        return B_BISHOP;
    case 'r':
        return B_ROOK;
    case 'q':
        return B_QUEEN;
    case 'k':
        return B_KING;
    default:
        return NO_PIECE;
    }
}

bool set_fen(struct Board *b, const char *fen) {
    init_board(b);
    int sq = SQ_A8;

    while (*fen && *fen != ' ') {
        if (*fen == '/') {
            sq -= 16;
        } else if (isdigit((unsigned char)*fen)) {
            sq += *fen - '0';
        } else {
            enum Piece p = char_to_piece(*fen);
            if (p == NO_PIECE)
                return false;
            b->pieces[p] |= 1ULL << sq;
            b->mailbox[sq] = p;
            sq++;
        }
        fen++;
    }

    b->occ[WHITE] = b->pieces[W_PAWN] | b->pieces[W_KNIGHT] | b->pieces[W_BISHOP] |
                    b->pieces[W_ROOK] | b->pieces[W_QUEEN] | b->pieces[W_KING];
    b->occ[BLACK] = b->pieces[B_PAWN] | b->pieces[B_KNIGHT] | b->pieces[B_BISHOP] |
                    b->pieces[B_ROOK] | b->pieces[B_QUEEN] | b->pieces[B_KING];
    b->all = b->occ[WHITE] | b->occ[BLACK];

    if (*fen != ' ')
        return false;
    fen++;
    b->side = (*fen == 'w') ? WHITE : BLACK;
    fen++;

    if (*fen != ' ')
        return false;
    fen++;
    b->history[0].castling = CASTLE_NONE;
    while (*fen && *fen != ' ') {
        if (*fen == 'K')
            b->history[0].castling |= CASTLE_WK;
        else if (*fen == 'Q')
            b->history[0].castling |= CASTLE_WQ;
        else if (*fen == 'k')
            b->history[0].castling |= CASTLE_BK;
        else if (*fen == 'q')
            b->history[0].castling |= CASTLE_BQ;
        fen++;
    }

    if (*fen != ' ')
        return false;
    fen++;
    b->history[0].ep = SQ_NONE;
    if (*fen != '-') {
        enum File f = (enum File)(fen[0] - 'a');
        enum Rank r = (enum Rank)(fen[1] - '1');
        b->history[0].ep = make_square(f, r);
        fen += 2;
    } else {
        fen++;
    }

    int halfmove = 0;
    if (*fen == ' ') {
        halfmove = (int)strtol(fen, NULL, 10);
    }
    b->history[0].halfmove = (uint8_t)halfmove;
    b->history[0].key = compute_key(b);
    nnue_refresh(b);
    return true;
}

bool is_square_attacked(const struct Board *b, enum Square sq, enum Color by_side) {
    int base = by_side * 6;
    if (pawn_attacks[by_side ^ 1][sq] & b->pieces[base + PAWN])
        return true;
    if (knight_attacks[sq] & b->pieces[base + KNIGHT])
        return true;
    if (king_attacks[sq] & b->pieces[base + KING])
        return true;
    if (bishop_attacks(sq, b->all) & (b->pieces[base + BISHOP] | b->pieces[base + QUEEN]))
        return true;
    if (rook_attacks(sq, b->all) & (b->pieces[base + ROOK] | b->pieces[base + QUEEN]))
        return true;
    return false;
}

bool in_check(const struct Board *b) {
    bitboard_t kbb = b->pieces[make_piece(b->side, KING)];
    return is_square_attacked(b, (enum Square)lsb64(kbb), (enum Color)(b->side ^ 1));
}

bool do_move(struct Board *b, move_t m) {
    enum Color us = b->side;
    enum Color them = (enum Color)(us ^ 1);
    enum Square from = move_from(m);
    enum Square to = move_to(m);
    uint16_t flags = move_flags(m);
    enum Piece piece = b->mailbox[from];
    enum Piece captured = NO_PIECE;

    struct StateInfo prev = b->history[b->ply];
    uint64_t key = prev.key;

    if (prev.ep != SQ_NONE)
        key ^= zobrist.ep[square_file(prev.ep)];

    b->pieces[piece] ^= 1ULL << from;
    b->occ[us] ^= 1ULL << from;
    b->mailbox[from] = NO_PIECE;
    key ^= zobrist.pieces[piece][from];

    if (flags == MOVE_EN_PASSANT) {
        enum Square cap_sq = (enum Square)(us == WHITE ? to - 8 : to + 8);
        captured = make_piece(them, PAWN);
        b->pieces[captured] ^= 1ULL << cap_sq;
        b->occ[them] ^= 1ULL << cap_sq;
        b->mailbox[cap_sq] = NO_PIECE;
        key ^= zobrist.pieces[captured][cap_sq];
        nnue_sub_piece(b, captured, cap_sq);
    } else if (move_is_capture(m)) {
        captured = b->mailbox[to];
        b->pieces[captured] ^= 1ULL << to;
        b->occ[them] ^= 1ULL << to;
        key ^= zobrist.pieces[captured][to];
        nnue_sub_piece(b, captured, to);
    }

    enum Piece placed = move_is_promotion(m) ? make_piece(us, move_promoted_type(m)) : piece;
    b->pieces[placed] |= 1ULL << to;
    b->occ[us] |= 1ULL << to;
    b->mailbox[to] = placed;
    key ^= zobrist.pieces[placed][to];

    if (move_is_promotion(m)) {
        nnue_sub_piece(b, piece, from);
        nnue_add_piece(b, placed, to);
    } else {
        nnue_move_piece(b, piece, from, to);
    }

    if (flags == MOVE_CASTLE) {
        enum Square r_from, r_to;
        if (to == SQ_G1) {
            r_from = SQ_H1;
            r_to = SQ_F1;
        } else if (to == SQ_C1) {
            r_from = SQ_A1;
            r_to = SQ_D1;
        } else if (to == SQ_G8) {
            r_from = SQ_H8;
            r_to = SQ_F8;
        } else {
            r_from = SQ_A8;
            r_to = SQ_D8;
        }
        enum Piece rook = make_piece(us, ROOK);
        b->pieces[rook] ^= (1ULL << r_from) | (1ULL << r_to);
        b->occ[us] ^= (1ULL << r_from) | (1ULL << r_to);
        b->mailbox[r_from] = NO_PIECE;
        b->mailbox[r_to] = rook;
        key ^= zobrist.pieces[rook][r_from] ^ zobrist.pieces[rook][r_to];
        nnue_move_piece(b, rook, r_from, r_to);
    }

    b->all = b->occ[WHITE] | b->occ[BLACK];

    uint8_t new_castling = prev.castling & castling_mask[from] & castling_mask[to];
    key ^= zobrist.castling[prev.castling] ^ zobrist.castling[new_castling];

    enum Square new_ep = SQ_NONE;
    if (flags == MOVE_DOUBLE_PAWN) {
        new_ep = (enum Square)((from + to) >> 1);
        key ^= zobrist.ep[square_file(new_ep)];
    }

    b->side = them;
    key ^= zobrist.side;

    b->ply++;
    b->history[b->ply] = (struct StateInfo){
        .castling = new_castling,
        .ep = new_ep,
        .halfmove = (piece_type(piece) == PAWN || move_is_capture(m)) ? 0 : prev.halfmove + 1,
        .captured = captured,
        .key = key,
    };

    enum Square ksq = (enum Square)lsb64(b->pieces[make_piece(us, KING)]);
    if (is_square_attacked(b, ksq, them)) {
        undo_move(b, m);
        return false;
    }
    return true;
}

void undo_move(struct Board *b, move_t m) {
    b->side ^= 1;
    enum Color us = b->side;
    enum Color them = (enum Color)(us ^ 1);
    enum Piece captured = b->history[b->ply].captured;
    b->ply--;

    enum Square from = move_from(m);
    enum Square to = move_to(m);
    uint16_t flags = move_flags(m);

    enum Piece placed = b->mailbox[to];
    enum Piece piece = move_is_promotion(m) ? make_piece(us, PAWN) : placed;

    b->pieces[placed] ^= 1ULL << to;
    b->occ[us] ^= 1ULL << to;

    b->pieces[piece] |= 1ULL << from;
    b->occ[us] |= 1ULL << from;
    b->mailbox[from] = piece;
    b->mailbox[to] = NO_PIECE;

    if (move_is_promotion(m)) {
        nnue_sub_piece(b, placed, to);
        nnue_add_piece(b, piece, from);
    } else {
        nnue_move_piece(b, piece, to, from);
    }

    if (flags == MOVE_EN_PASSANT) {
        enum Square cap_sq = (enum Square)(us == WHITE ? to - 8 : to + 8);
        b->pieces[captured] |= 1ULL << cap_sq;
        b->occ[them] |= 1ULL << cap_sq;
        b->mailbox[cap_sq] = captured;
        nnue_add_piece(b, captured, cap_sq);
    } else if (captured != NO_PIECE) {
        b->pieces[captured] |= 1ULL << to;
        b->occ[them] |= 1ULL << to;
        b->mailbox[to] = captured;
        nnue_add_piece(b, captured, to);
    }

    if (flags == MOVE_CASTLE) {
        enum Square r_from, r_to;
        if (to == SQ_G1) {
            r_from = SQ_H1;
            r_to = SQ_F1;
        } else if (to == SQ_C1) {
            r_from = SQ_A1;
            r_to = SQ_D1;
        } else if (to == SQ_G8) {
            r_from = SQ_H8;
            r_to = SQ_F8;
        } else {
            r_from = SQ_A8;
            r_to = SQ_D8;
        }
        enum Piece rook = make_piece(us, ROOK);
        b->pieces[rook] ^= (1ULL << r_from) | (1ULL << r_to);
        b->occ[us] ^= (1ULL << r_from) | (1ULL << r_to);
        b->mailbox[r_to] = NO_PIECE;
        b->mailbox[r_from] = rook;
        nnue_move_piece(b, rook, r_to, r_from);
    }

    b->all = b->occ[WHITE] | b->occ[BLACK];
}

void do_null_move(struct Board *b) {
    struct StateInfo prev = b->history[b->ply];
    uint64_t key = prev.key ^ zobrist.side;
    if (prev.ep != SQ_NONE)
        key ^= zobrist.ep[square_file(prev.ep)];

    b->side ^= 1;
    b->ply++;
    b->history[b->ply] = (struct StateInfo){
        .castling = prev.castling,
        .ep = SQ_NONE,
        .halfmove = prev.halfmove + 1,
        .captured = NO_PIECE,
        .key = key,
    };
}

void undo_null_move(struct Board *b) {
    b->side ^= 1;
    b->ply--;
}

void print_board(const struct Board *b) {
    printf("\n");
    for (int r = RANK_8; r >= RANK_1; --r) {
        printf(" %d ", r + 1);
        for (int f = FILE_A; f <= FILE_H; ++f) {
            enum Square sq = make_square((enum File)f, (enum Rank)r);
            enum Piece p = b->mailbox[sq];
            printf(" %c", piece_chars[p]);
        }
        printf("\n");
    }
    printf("\n    a b c d e f g h\n\n");
    printf("Side: %s, Key: 0x%016llx\n\n", b->side == WHITE ? "white" : "black",
           (unsigned long long)b->history[b->ply].key);
}
