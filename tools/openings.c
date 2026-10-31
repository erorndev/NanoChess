#include "attacks.h"
#include "board.h"
#include "book.h"
#include "movegen.h"
#include "platform.h"
#include "time.h"
#include "zobrist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char piece_chars[13] = "PNBRQKpnbrqk.";

static void board_to_fen(const struct Board *b, char *out) {
    char *p = out;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            enum Piece pc = b->mailbox[r * 8 + f];
            if (pc == NO_PIECE) {
                empty++;
                continue;
            }
            if (empty) {
                *p++ = (char)('0' + empty);
                empty = 0;
            }
            *p++ = piece_chars[pc];
        }
        if (empty)
            *p++ = (char)('0' + empty);
        if (r > 0)
            *p++ = '/';
    }
    *p++ = ' ';
    *p++ = b->side == WHITE ? 'w' : 'b';
    *p++ = ' ';

    uint8_t cr = b->history[b->ply].castling;
    if (!cr) {
        *p++ = '-';
    } else {
        if (cr & CASTLE_WK) *p++ = 'K';
        if (cr & CASTLE_WQ) *p++ = 'Q';
        if (cr & CASTLE_BK) *p++ = 'k';
        if (cr & CASTLE_BQ) *p++ = 'q';
    }
    *p++ = ' ';

    enum Square ep = b->history[b->ply].ep;
    if (ep == SQ_NONE) {
        *p++ = '-';
    } else {
        *p++ = (char)('a' + (ep & 7));
        *p++ = (char)('1' + (ep >> 3));
    }
    sprintf(p, " %d %d", b->history[b->ply].halfmove, b->ply / 2 + 1);
}

int main(int argc, char **argv) {
    int count = argc > 1 ? atoi(argv[1]) : 500;
    int min_plies = argc > 2 ? atoi(argv[2]) : 6;
    int max_plies = argc > 3 ? atoi(argv[3]) : 10;

    init_zobrist();
    init_attacks();
    init_book();
    option_own_book = true;

    uint64_t *seen = calloc((size_t)count, sizeof(uint64_t));
    int seen_count = 0;
    int attempts = 0;
    char fen[128];

    while (seen_count < count && attempts < count * 50) {
        attempts++;
        struct Board b;
        set_fen(&b, START_FEN);

        int target = min_plies + (int)(get_time_ms() + (uint64_t)attempts * 7919) %
                                     (uint64_t)(max_plies - min_plies + 1);
        int played = 0;
        for (; played < target; ++played) {
            move_t m = probe_book(&b);
            if (!m || !do_move(&b, m))
                break;
        }
        if (played < min_plies)
            continue;

        uint64_t key = board_key(&b);
        bool dup = false;
        for (int i = 0; i < seen_count; ++i) {
            if (seen[i] == key) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;

        seen[seen_count++] = key;
        board_to_fen(&b, fen);
        printf("%s\n", fen);
    }

    free(seen);
    fprintf(stderr, "generated %d positions in %d attempts\n", seen_count, attempts);
    return 0;
}
