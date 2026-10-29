#include "movegen.h"
#include "attacks.h"
#include "platform.h"

#include <string.h>

#define FILE_A_BB 0x0101010101010101ULL
#define FILE_H_BB 0x8080808080808080ULL
#define RANK_2_BB 0x000000000000FF00ULL
#define RANK_3_BB 0x0000000000FF0000ULL
#define RANK_6_BB 0x0000FF0000000000ULL
#define RANK_7_BB 0x00FF000000000000ULL

static inline void add_move(struct MoveList *list, enum Square from, enum Square to,
                            uint16_t flags) {
    list->moves[list->count++] = make_move(from, to, flags);
}

static inline void add_promotions(struct MoveList *list, enum Square from, enum Square to,
                                  bool cap) {
    uint16_t base = cap ? MOVE_PROM_CAP_Q : MOVE_PROM_Q;
    add_move(list, from, to, base);
    add_move(list, from, to, base - (1 << 12));
    add_move(list, from, to, base - (2 << 12));
    add_move(list, from, to, base - (3 << 12));
}

static void gen_internal(const struct Board *b, struct MoveList *list, bool caps_only) {
    list->count = 0;
    enum Color us = b->side;
    enum Color them = (enum Color)(us ^ 1);
    bitboard_t empty = ~b->all;
    bitboard_t enemies = b->occ[them];

    if (us == WHITE) {
        bitboard_t pawns = b->pieces[W_PAWN];
        bitboard_t promo = pawns & RANK_7_BB;
        bitboard_t normal = pawns & ~RANK_7_BB;

        if (promo) {
            bitboard_t push = (promo << 8) & empty;
            while (push) {
                int to = lsb64(push);
                add_promotions(list, (enum Square)(to - 8), (enum Square)to, false);
                push &= push - 1;
            }
            bitboard_t cap_l = ((promo & ~FILE_A_BB) << 7) & enemies;
            while (cap_l) {
                int to = lsb64(cap_l);
                add_promotions(list, (enum Square)(to - 7), (enum Square)to, true);
                cap_l &= cap_l - 1;
            }
            bitboard_t cap_r = ((promo & ~FILE_H_BB) << 9) & enemies;
            while (cap_r) {
                int to = lsb64(cap_r);
                add_promotions(list, (enum Square)(to - 9), (enum Square)to, true);
                cap_r &= cap_r - 1;
            }
        }

        bitboard_t cap_l = ((normal & ~FILE_A_BB) << 7) & enemies;
        while (cap_l) {
            int to = lsb64(cap_l);
            add_move(list, (enum Square)(to - 7), (enum Square)to, MOVE_CAPTURE);
            cap_l &= cap_l - 1;
        }
        bitboard_t cap_r = ((normal & ~FILE_H_BB) << 9) & enemies;
        while (cap_r) {
            int to = lsb64(cap_r);
            add_move(list, (enum Square)(to - 9), (enum Square)to, MOVE_CAPTURE);
            cap_r &= cap_r - 1;
        }

        if (!caps_only) {
            bitboard_t single = (normal << 8) & empty;
            bitboard_t dbl = ((single & RANK_3_BB) << 8) & empty;
            while (single) {
                int to = lsb64(single);
                add_move(list, (enum Square)(to - 8), (enum Square)to, MOVE_QUIET);
                single &= single - 1;
            }
            while (dbl) {
                int to = lsb64(dbl);
                add_move(list, (enum Square)(to - 16), (enum Square)to, MOVE_DOUBLE_PAWN);
                dbl &= dbl - 1;
            }
        }
    } else {
        bitboard_t pawns = b->pieces[B_PAWN];
        bitboard_t promo = pawns & RANK_2_BB;
        bitboard_t normal = pawns & ~RANK_2_BB;

        if (promo) {
            bitboard_t push = (promo >> 8) & empty;
            while (push) {
                int to = lsb64(push);
                add_promotions(list, (enum Square)(to + 8), (enum Square)to, false);
                push &= push - 1;
            }
            bitboard_t cap_l = ((promo & ~FILE_A_BB) >> 9) & enemies;
            while (cap_l) {
                int to = lsb64(cap_l);
                add_promotions(list, (enum Square)(to + 9), (enum Square)to, true);
                cap_l &= cap_l - 1;
            }
            bitboard_t cap_r = ((promo & ~FILE_H_BB) >> 7) & enemies;
            while (cap_r) {
                int to = lsb64(cap_r);
                add_promotions(list, (enum Square)(to + 7), (enum Square)to, true);
                cap_r &= cap_r - 1;
            }
        }

        bitboard_t cap_l = ((normal & ~FILE_A_BB) >> 9) & enemies;
        while (cap_l) {
            int to = lsb64(cap_l);
            add_move(list, (enum Square)(to + 9), (enum Square)to, MOVE_CAPTURE);
            cap_l &= cap_l - 1;
        }
        bitboard_t cap_r = ((normal & ~FILE_H_BB) >> 7) & enemies;
        while (cap_r) {
            int to = lsb64(cap_r);
            add_move(list, (enum Square)(to + 7), (enum Square)to, MOVE_CAPTURE);
            cap_r &= cap_r - 1;
        }

        if (!caps_only) {
            bitboard_t single = (normal >> 8) & empty;
            bitboard_t dbl = ((single & RANK_6_BB) >> 8) & empty;
            while (single) {
                int to = lsb64(single);
                add_move(list, (enum Square)(to + 8), (enum Square)to, MOVE_QUIET);
                single &= single - 1;
            }
            while (dbl) {
                int to = lsb64(dbl);
                add_move(list, (enum Square)(to + 16), (enum Square)to, MOVE_DOUBLE_PAWN);
                dbl &= dbl - 1;
            }
        }
    }

    enum Square ep = b->history[b->ply].ep;
    if (ep != SQ_NONE) {
        bitboard_t ep_atk = pawn_attacks[them][ep] & b->pieces[make_piece(us, PAWN)];
        while (ep_atk) {
            int from = lsb64(ep_atk);
            add_move(list, (enum Square)from, ep, MOVE_EN_PASSANT);
            ep_atk &= ep_atk - 1;
        }
    }

    for (enum PieceType pt = KNIGHT; pt <= KING; ++pt) {
        bitboard_t bb = b->pieces[make_piece(us, pt)];
        while (bb) {
            enum Square from = (enum Square)lsb64(bb);
            bitboard_t atk = piece_attacks(pt, from, b->all);

            bitboard_t caps = atk & enemies;
            while (caps) {
                enum Square to = (enum Square)lsb64(caps);
                add_move(list, from, to, MOVE_CAPTURE);
                caps &= caps - 1;
            }

            if (!caps_only) {
                bitboard_t quiets = atk & empty;
                while (quiets) {
                    enum Square to = (enum Square)lsb64(quiets);
                    add_move(list, from, to, MOVE_QUIET);
                    quiets &= quiets - 1;
                }
            }
            bb &= bb - 1;
        }
    }

    if (!caps_only) {
        uint8_t cr = b->history[b->ply].castling;
        if (us == WHITE) {
            if ((cr & CASTLE_WK) && !(b->all & 0x60ULL)) {
                if (!is_square_attacked(b, SQ_E1, BLACK) && !is_square_attacked(b, SQ_F1, BLACK))
                    add_move(list, SQ_E1, SQ_G1, MOVE_CASTLE);
            }
            if ((cr & CASTLE_WQ) && !(b->all & 0x0EULL)) {
                if (!is_square_attacked(b, SQ_E1, BLACK) && !is_square_attacked(b, SQ_D1, BLACK))
                    add_move(list, SQ_E1, SQ_C1, MOVE_CASTLE);
            }
        } else {
            if ((cr & CASTLE_BK) && !(b->all & 0x6000000000000000ULL)) {
                if (!is_square_attacked(b, SQ_E8, WHITE) && !is_square_attacked(b, SQ_F8, WHITE))
                    add_move(list, SQ_E8, SQ_G8, MOVE_CASTLE);
            }
            if ((cr & CASTLE_BQ) && !(b->all & 0x0E00000000000000ULL)) {
                if (!is_square_attacked(b, SQ_E8, WHITE) && !is_square_attacked(b, SQ_D8, WHITE))
                    add_move(list, SQ_E8, SQ_C8, MOVE_CASTLE);
            }
        }
    }
}

void generate_moves(const struct Board *b, struct MoveList *list) {
    gen_internal(b, list, false);
}

void generate_captures(const struct Board *b, struct MoveList *list) {
    gen_internal(b, list, true);
}

void generate_legal(struct Board *b, struct MoveList *list) {
    struct MoveList pseudo;
    generate_moves(b, &pseudo);
    list->count = 0;
    for (int i = 0; i < pseudo.count; ++i) {
        move_t m = pseudo.moves[i];
        if (do_move(b, m)) {
            list->moves[list->count++] = m;
            undo_move(b, m);
        }
    }
}

void move_to_str(move_t m, char *buf) {
    enum Square from = move_from(m);
    enum Square to = move_to(m);
    buf[0] = (char)('a' + square_file(from));
    buf[1] = (char)('1' + square_rank(from));
    buf[2] = (char)('a' + square_file(to));
    buf[3] = (char)('1' + square_rank(to));
    if (move_is_promotion(m)) {
        const char prom_chars[6] = {0, 'n', 'b', 'r', 'q', 0};
        buf[4] = prom_chars[move_promoted_type(m)];
        buf[5] = '\0';
    } else {
        buf[4] = '\0';
    }
}

move_t parse_move(struct Board *b, const char *str) {
    struct MoveList list;
    generate_legal(b, &list);
    char buf[6];
    for (int i = 0; i < list.count; ++i) {
        move_to_str(list.moves[i], buf);
        int len = (int)strlen(buf);
        if (strncmp(str, buf, len) == 0 &&
            (str[len] == '\0' || str[len] == ' ' || str[len] == '\n'))
            return list.moves[i];
    }
    return 0;
}

uint64_t perft(struct Board *b, int depth) {
    if (depth == 0)
        return 1ULL;

    struct MoveList list;
    generate_moves(b, &list);
    uint64_t nodes = 0;

    for (int i = 0; i < list.count; ++i) {
        move_t m = list.moves[i];
        if (!do_move(b, m))
            continue;
        nodes += (depth == 1) ? 1ULL : perft(b, depth - 1);
        undo_move(b, m);
    }
    return nodes;
}
