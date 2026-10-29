#undef NDEBUG
#include "attacks.h"
#include "board.h"
#include "movegen.h"
#include "platform.h"
#include "types.h"
#include "zobrist.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    init_zobrist();
    init_attacks();

    FILE *f = fopen("tests/perftsuite.epd", "r");
    if (!f)
        f = fopen("../tests/perftsuite.epd", "r");
    if (!f) {
        printf("Error: Could not open tests/perftsuite.epd\n");
        return 1;
    }

    char line[1024];
    int tested_pos = 0;
    int tested_depths = 0;
    uint64_t total_nodes = 0;
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
        if (!set_fen(&b, fen)) {
            printf("Warning: failed to parse FEN: %s\n", fen);
            continue;
        }

        tested_pos++;

        char *token = semi;
        while (token && *token == ';') {
            token++;
            while (*token == ' ')
                token++;
            if (*token == 'D' || *token == 'd') {
                token++;
                int depth = (int)strtol(token, &token, 10);
                while (*token == ' ')
                    token++;
                uint64_t expected = (uint64_t)strtoull(token, &token, 10);

                if (expected > 0 && expected <= 10000000ULL) {
                    uint64_t nodes = perft(&b, depth);
                    assert(nodes == expected);
                    tested_depths++;
                    total_nodes += nodes;
                }
            }
            token = strchr(token, ';');
        }
    }

    fclose(f);
    uint64_t dt = get_time_ms() - t0;
    uint64_t nps = dt > 0 ? (total_nodes * 1000ULL) / dt : total_nodes;

    printf("perft: %d positions, %d depths, %llu nodes (%llu ms, %llu nps) - all passed\n",
           tested_pos, tested_depths, (unsigned long long)total_nodes, (unsigned long long)dt,
           (unsigned long long)nps);

    return 0;
}
