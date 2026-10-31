#undef NDEBUG
#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "movegen.h"
#include "platform.h"
#include "search.h"
#include "time.h"
#include "tt.h"
#include "types.h"
#include "zobrist.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clean_move_str(char *dest, const char *src) {
    while (*src && !isalpha((unsigned char)*src))
        src++;
    int i = 0;
    while (*src && (isalnum((unsigned char)*src) || *src == '=')) {
        if (*src != '+' && *src != '#' && *src != 'x') {
            dest[i++] = (char)*src;
        }
        src++;
    }
    dest[i] = '\0';
}

static move_t parse_san_approx(struct Board *b, const char *san) {
    char cleaned[32];
    clean_move_str(cleaned, san);

    struct MoveList list;
    generate_legal(b, &list);

    // If already in coordinate notation like "e2e4" or "b3b8"
    if (strlen(cleaned) >= 4 && islower((unsigned char)cleaned[0]) &&
        isdigit((unsigned char)cleaned[1]) && islower((unsigned char)cleaned[2]) &&
        isdigit((unsigned char)cleaned[3])) {
        for (int i = 0; i < list.count; ++i) {
            char uci[6];
            move_to_str(list.moves[i], uci);
            if (strncmp(cleaned, uci, 4) == 0)
                return list.moves[i];
        }
    }

    // SAN notation like "Qb3", "Nf7", "exf6", "Bxh7", "d5"
    enum PieceType pt = PAWN;
    const char *p = cleaned;
    if (isupper((unsigned char)*p)) {
        if (*p == 'N')
            pt = KNIGHT;
        else if (*p == 'B')
            pt = BISHOP;
        else if (*p == 'R')
            pt = ROOK;
        else if (*p == 'Q')
            pt = QUEEN;
        else if (*p == 'K')
            pt = KING;
        p++;
    }

    int to_f = -1, to_r = -1;
    int from_f = -1, from_r = -1;

    int len = (int)strlen(p);
    if (len >= 2) {
        to_f = p[len - 2] - 'a';
        to_r = p[len - 1] - '1';
        if (len >= 3) {
            if (isalpha((unsigned char)p[0]))
                from_f = p[0] - 'a';
            else if (isdigit((unsigned char)p[0]))
                from_r = p[0] - '1';
        }
    }

    if (to_f < 0 || to_f > 7 || to_r < 0 || to_r > 7)
        return 0;

    enum Square to_sq = make_square((enum File)to_f, (enum Rank)to_r);

    for (int i = 0; i < list.count; ++i) {
        move_t m = list.moves[i];
        if (move_to(m) != to_sq)
            continue;
        enum Piece pc = b->mailbox[move_from(m)];
        if (piece_type(pc) != pt)
            continue;
        if (from_f != -1 && square_file(move_from(m)) != from_f)
            continue;
        if (from_r != -1 && square_rank(move_from(m)) != from_r)
            continue;
        return m;
    }

    return 0;
}

int main(void) {
    init_zobrist();
    init_attacks();
    init_eval();
    init_tt(32);
    init_search();

    FILE *f = fopen("tests/kaufman.epd", "r");
    if (!f)
        f = fopen("../tests/kaufman.epd", "r");
    if (!f) {
        printf("Error: Could not open tests/kaufman.epd\n");
        return 1;
    }

    char line[1024];
    int total_tested = 0;
    int matched = 0;
    uint64_t t0 = get_time_ms();

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strlen(line) < 5)
            continue;

        char *semi = strchr(line, ';');
        if (!semi)
            continue;

        char fen[256];
        int fen_len = (int)(semi - line);
        while (fen_len > 0 && line[fen_len - 1] == ' ')
            fen_len--;
        strncpy(fen, line, fen_len);
        fen[fen_len] = '\0';

        struct Board b;
        if (!set_fen(&b, fen))
            continue;

        char *bm_tag = strstr(semi, "bm ");
        if (!bm_tag)
            continue;

        bm_tag += 3;
        while (*bm_tag == ' ')
            bm_tag++;

        char bm_san[32];
        int k = 0;
        while (bm_tag[k] && bm_tag[k] != ';' && bm_tag[k] != '\n' && bm_tag[k] != '\r') {
            bm_san[k] = bm_tag[k];
            k++;
        }
        bm_san[k] = '\0';

        move_t expected_move = parse_san_approx(&b, bm_san);
        if (expected_move == 0)
            continue;

        total_tested++;
        clear_tt();
        clear_search();

        struct SearchLimits lim = {
            .depth = 14,
            .soft_time = 0,
            .hard_time = 0,
            .max_nodes = 0,
            .uci_output = false,
        };
        struct SearchResult res = search_position(&b, &lim);

        char expected_uci[6], actual_uci[6];
        move_to_str(expected_move, expected_uci);
        move_to_str(res.best_move, actual_uci);

        bool hit = (res.best_move == expected_move);
        if (hit)
            matched++;

        printf("Kaufman #%2d: expected=%s (%s), got=%s [%s]\n", total_tested, expected_uci, bm_san,
               actual_uci, hit ? "PASS" : "FAIL");
    }

    fclose(f);
    free_tt();

    uint64_t dt = get_time_ms() - t0;
    printf("\nkaufman: %d/%d passed (%.1f%%) in %llu ms\n", matched, total_tested,
           (total_tested > 0 ? (100.0 * matched) / total_tested : 0.0), (unsigned long long)dt);

    return 0;
}
