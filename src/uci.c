#include "uci.h"
#include "attacks.h"
#include "book.h"
#include "evaluate.h"
#include "movegen.h"
#include "platform.h"
#include "search.h"
#include "tt.h"
#include "zobrist.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENGINE_NAME "NanoChess"
#define ENGINE_AUTHOR "Erorn"

static void run_bench(void) {
    const char *bench_fens[] = {
        START_FEN,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r1b1k2r/pp3pbp/1qp1p1p1/2n5/2B5/2N2N2/PPP2PPP/R1BQK2R w KQkq - 0 11",
        "r2q1rk1/1pp1bppp/p1np1nb1/4p3/2B1P1P1/2NP1N1P/PPP2P2/R1BQR1K1 w - - 0 11",
        NULL};

    clear_tt();
    clear_search();
    uint64_t total_nodes = 0;
    uint64_t t0 = get_time_ms();

    for (int i = 0; bench_fens[i] != NULL; ++i) {
        struct Board b;
        set_fen(&b, bench_fens[i]);
        struct SearchLimits lim = {
            .depth = 10, .soft_time = 0, .hard_time = 0, .max_nodes = 0, .uci_output = false};
        struct SearchResult res = search_position(&b, &lim);
        total_nodes += res.nodes;
    }

    uint64_t dt = get_time_ms() - t0;
    uint64_t nps = dt > 0 ? (total_nodes * 1000ULL) / dt : total_nodes;
    printf("%llu nodes %llu nps\n", (unsigned long long)total_nodes, (unsigned long long)nps);
}

static void parse_position(struct Board *b, char *line) {
    char *p = line + 8; // skip "position"
    while (*p == ' ')
        p++;

    if (strncmp(p, "startpos", 8) == 0) {
        set_fen(b, START_FEN);
        p += 8;
    } else if (strncmp(p, "fen", 3) == 0) {
        p += 3;
        while (*p == ' ')
            p++;
        char fen[256];
        int k = 0;
        while (*p && strncmp(p, "moves", 5) != 0 && k < 255) {
            fen[k++] = *p++;
        }
        while (k > 0 && fen[k - 1] == ' ')
            k--;
        fen[k] = '\0';
        set_fen(b, fen);
    }

    char *moves_str = strstr(p, "moves");
    if (moves_str) {
        moves_str += 5;
        while (*moves_str) {
            while (*moves_str == ' ')
                moves_str++;
            if (!*moves_str)
                break;
            move_t m = parse_move(b, moves_str);
            if (m) {
                (void)do_move(b, m);
            }
            while (*moves_str && *moves_str != ' ')
                moves_str++;
        }
    }
}

struct SearchLimits allocate_time(const struct Board *b, uint64_t time_left, uint64_t inc, int movestogo) {
    struct SearchLimits lim = {0};
    if (time_left == 0)
        return lim;

    const uint64_t overhead = 15;
    uint64_t usable = (time_left > overhead) ? (time_left - overhead) : (time_left * 3 / 4);
    if (usable < 1)
        usable = 1;

    int pieces = popcount64(b->all);
    int moves = (movestogo > 0 && movestogo < 50)
                    ? movestogo
                    : (time_left < 30000 ? (16 + (pieces * 16) / 32) : (20 + (pieces * 20) / 32));

    uint64_t base = usable / (uint64_t)moves;
    uint64_t scale_num = 16 + (uint64_t)((32 - pieces) / 2);
    uint64_t optimum = (base * scale_num) / 16 + (inc * 4 / 5);

    if (time_left < 1500) {
        uint64_t cap = usable * 4 / 10 + inc * 7 / 10;
        if (optimum > cap)
            optimum = cap;
    }

    if (optimum > usable * 8 / 10)
        optimum = usable * 8 / 10;
    if (optimum < 1)
        optimum = 1;

    uint64_t hard = (time_left < 1500) ? (usable / 2) : (usable * 3 / 4);
    uint64_t hard_scale = optimum * 5 / 2 + inc * 4 / 5;
    if (hard_scale < hard)
        hard = hard_scale;
    if (hard < optimum)
        hard = optimum;
    if (hard > usable)
        hard = usable;
    if (hard < 1)
        hard = 1;

    lim.soft_time = optimum;
    lim.hard_time = hard;
    return lim;
}

static void parse_go(struct Board *b, char *line) {
    struct SearchLimits lim = {
        .depth = 0, .soft_time = 0, .hard_time = 0, .max_nodes = 0, .uci_output = true};

    uint64_t wtime = 0, btime = 0, winc = 0, binc = 0, movetime = 0;
    int movestogo = 0;

    char *token = strtok(line, " \t\r\n");
    while (token) {
        if (strcmp(token, "depth") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                lim.depth = atoi(token);
        } else if (strcmp(token, "wtime") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                wtime = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "btime") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                btime = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "winc") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                winc = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "binc") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                binc = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "movetime") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                movetime = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "nodes") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                lim.max_nodes = (uint64_t)strtoull(token, NULL, 10);
        } else if (strcmp(token, "movestogo") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token)
                movestogo = atoi(token);
        }
        token = strtok(NULL, " \t\r\n");
    }

    move_t bm = probe_book(b);
    if (bm != 0) {
        char mbuf[6];
        move_to_str(bm, mbuf);
        printf("info depth 1 score cp 20 time 0 pv %s\n", mbuf);
        printf("bestmove %s\n", mbuf);
        fflush(stdout);
        return;
    }

    if (movetime > 0) {
        lim.soft_time = movetime;
        lim.hard_time = movetime;
    } else {
        uint64_t time_left = (b->side == WHITE) ? wtime : btime;
        uint64_t inc = (b->side == WHITE) ? winc : binc;
        struct SearchLimits tlim = allocate_time(b, time_left, inc, movestogo);
        lim.soft_time = tlim.soft_time;
        lim.hard_time = tlim.hard_time;
    }

    struct SearchResult res = search_position(b, &lim);
    char mbuf[6];
    move_to_str(res.best_move, mbuf);
    printf("bestmove %s\n", mbuf);
    fflush(stdout);
}

void uci_loop(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        run_bench();
        return;
    }

    struct Board b;
    set_fen(&b, START_FEN);

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        char *cr = strchr(line, '\r');
        if (cr)
            *cr = '\0';

        if (strcmp(line, "uci") == 0) {
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("option name Hash type spin default 32 min 1 max 1024\n");
            printf("option name Threads type spin default 1 min 1 max %d\n", MAX_THREADS);
            printf("option name OwnBook type check default true\n");
            printf("uciok\n");
            fflush(stdout);
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        } else if (strcmp(line, "ucinewgame") == 0) {
            clear_tt();
            clear_search();
            set_fen(&b, START_FEN);
        } else if (strncmp(line, "position", 8) == 0) {
            parse_position(&b, line);
        } else if (strncmp(line, "go", 2) == 0) {
            parse_go(&b, line);
        } else if (strncmp(line, "setoption", 9) == 0) {
            char *name = strstr(line, "name Hash");
            char *val = strstr(line, "value ");
            if (name && val) {
                int mb = atoi(val + 6);
                init_tt(mb);
            }
            char *threads_name = strstr(line, "name Threads");
            if (threads_name && val) {
                int n = atoi(val + 6);
                set_threads(n);
            }
            char *book_name = strstr(line, "name OwnBook");
            if (book_name && val) {
                if (strstr(val, "true"))
                    option_own_book = true;
                else if (strstr(val, "false"))
                    option_own_book = false;
            }
        } else if (strcmp(line, "bench") == 0) {
            run_bench();
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}
