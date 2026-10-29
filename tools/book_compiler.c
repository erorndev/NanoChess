#include "board.h"
#include "movegen.h"
#include "zobrist.h"
#include "attacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "raw_book.h"

#define MAX_ENTRIES 8192

struct CompiledEntry {
    uint64_t key;
    move_t move;
    uint8_t weight;
};

static struct CompiledEntry entries[MAX_ENTRIES];
static int total_entries = 0;

static void add_entry(uint64_t key, move_t m, uint8_t weight) {
    if (!m) return;
    for (int i = 0; i < total_entries; ++i) {
        if (entries[i].key == key && entries[i].move == m) {
            if (weight > entries[i].weight)
                entries[i].weight = weight;
            return;
        }
    }
    if (total_entries < MAX_ENTRIES) {
        entries[total_entries].key = key;
        entries[total_entries].move = m;
        entries[total_entries].weight = weight ? weight : 50;
        total_entries++;
    }
}

static void add_line(const char *line, uint8_t weight) {
    struct Board b;
    set_fen(&b, START_FEN);
    char buf[1024];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, " \t\r\n");
    while (tok) {
        move_t m = parse_move(&b, tok);
        if (!m) break;
        uint64_t key = board_key(&b);
        add_entry(key, m, weight);
        if (!do_move(&b, m)) break;
        tok = strtok(NULL, " \t\r\n");
    }
}

static const char *gm_lines[] = {
    // --- 1. e4 e5 (Open Games) ---
    // Ruy Lopez: Berlin Defense
    "e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6 b5c6 d7c6 d4e5 d6f5 d1d8 e8d8 b1c3 d8e8 h2h3 h7h5",
    "e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6 b5c6 d7c6 d4e5 d6f5 d1d8 e8d8 f1d1 e8e8 b1c3 c8e6",
    // Ruy Lopez: Closed (Main line, Breyer, Chigorin)
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8 h2h3 c6b8 d2d4 b8d7",
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8 h2h3 c6a5 b3c2 c7c5 d2d4 d8c7",
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8 h2h3 h7h6 d2d4 f8e8",
    // Ruy Lopez: Marshall Attack
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 e8g8 c2c3 d7d5 e4d5 f6d5 f3e5 c6e5 e1e5 c7c6 d2d4 e7d6 e5e1 d8h4 g2g3 h4h3",
    // Ruy Lopez: Open Variation
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f6e4 d2d4 b7b5 a4b3 d7d5 d4e5 c8e6 c2c3 f8e7",
    // Ruy Lopez: Exchange Variation
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5c6 d7c6 e1g1 f7f6 d2d4 e5d4 f3d4 c6c5 d4b3 d8d1 f1d1 c8d7",
    // Italian Game: Giuoco Piano & Pianissimo
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 a7a6 e1g1 d7d6 c4b3 c5a7 b1d2 e8g8 h2h3 c8e6",
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 d7d6 e1g1 e8g8 f1e1 a7a6 a2a4 c5a7 h2h3 c6e7 b1d2 e7g6",
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 c3d4 c5b4 c1d2 b4d2 b1d2 d7d5 e4d5 f6d5",
    // Italian: Two Knights Defense
    "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8c5 c2c3 d7d6 e1g1 e8g8 b1d2 a7a6 a2a4 c5a7",
    "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 f3g5 d7d5 e4d5 c6a5 c4b5 c7c6 d5c6 b7c6 b5e2 h7h6 g5f3 e5e4 f3e5 f8d6",
    // Scotch Game
    "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 b1c3 f8b4 d4c6 b7c6 f1d3 d7d5 e4d5 c6d5 e1g1 e8g8",
    "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 f8c5 c1e3 d8f6 c2c3 g8e7 f1c4 e8g8 e1g1 c5b6",
    // Petroff Defense
    "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3 f8e7 e1g1 b8c6 c2c4 c6b4",
    "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3 b8c6 e1g1 f8e7 c2c4 e4f6 b1c3 c8g4",
    // Four Knights Game
    "e2e4 e7e5 g1f3 b8c6 b1c3 g8f6 f1b5 f8b4 e1g1 e8g8 d2d3 d7d6 c1g5 b4c3 b2c3 d8e7",

    // --- 1. e4 c5 (Sicilian Defense) ---
    // Najdorf 6.Be3 (English Attack)
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6 f2f3 b8d7 d1d2 b7b5 e1c1 f8e7 g2g4 b5b4 c3d5 e6d5 e4d5 d7b6",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e6 f2f3 b7b5 g2g4 h7h6 d1d2 b8d7 e1c1 c8b7 h2h4 b5b4 c3a4 d6d5",
    // Najdorf 6.Bg5
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1g5 e7e6 f2f4 d8b6 d1d2 b6b2 a1b1 b2a3 f4f5 b8c6 f5e6 f7e6 d4c6 b7c6 e4e5 d6e5 g5f6 g7f6 c3e4 f8e7 f1e2 h7h5 b1b3 a3a4 c2c4",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1g5 e7e6 f2f4 f8e7 d1f3 d8c7 e1c1 b8d7 g2g4 b7b5 g5f6 d7f6 g4g5 f6d7 f4f5 e8g8",
    // Najdorf 6.h3 (Adams Attack)
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 h2h3 e7e5 d4e2 h7h5 g2g3 c8e6 f1g2 b8d7 a2a4 a8c8 e1g1 f8e7 c1e3",
    // Dragon (Yugoslav Attack 9.Bc4 & 9.0-0-0)
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6 c1e3 f8g7 f2f3 b8c6 d1d2 e8g8 f1c4 c8d7 e1c1 a8c8 c4b3 c6e5 h2h4 h7h5 c1b1 f8e8",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6 c1e3 f8g7 f2f3 e8g8 d1d2 b8c6 e1c1 d6d5 e4d5 f6d5 d4c6 b7c6 e3d4 e7e5 d4c5 c8e6 c3e4 f8e8",
    // Classical Sicilian (Richter-Rauzer)
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 b8c6 c1g5 e7e6 d1d2 a7a6 e1c1 c8d7 f2f4 b7b5 g5f6 g7f6 c1b1 d8b6 d4c6 d7c6 f4f5 b5b4",
    // Sveshnikov Variation
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5 d7d6 c1g5 a7a6 b5a3 b7b5 c3d5 f8e7 g5f6 e7f6 c2c3 e8g8 a3c2 a8b8 g2g3 f6g5 f1g2 c8e6 e1g1 c6e7",
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5 d7d6 c1g5 a7a6 b5a3 b7b5 c3d5 f8e7 g5f6 e7f6 c2c3 c6e7 d5f6 g7f6 c2c4 c8b7",
    // Taimanov / Kan
    "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d8c7 c1e3 a7a6 d1f3 g8f6 e1c1 c6e5 f3g3 b7b5 f2f4 e5g6 e4e5 b5b4 c3b5 a6b5 e5f6 a8a2",
    "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 a7a6 b1c3 d8c7 f1d3 g8f6 e1g1 f8c5 d4b3 c5e7 f2f4 d7d6 g1h1 b8d7 d1e2 b7b5 a2a4 b5b4 c3d1 c8b7",
    // Alapin Variation (2.c3)
    "e2e4 c7c5 c2c3 d7d5 e4d5 d8d5 d2d4 g8f6 g1f3 c8g4 f1e2 e7e6 e1g1 b8c6 c1e3 c5d4 c3d4 f8e7 b1c3 d5d6 h2h3 g4h5 d1b3 e8g8 f1d1 f8d8 a2a3 a7a6 a1c1 b7b5",
    "e2e4 c7c5 c2c3 g8f6 e4e5 f6d5 d2d4 c5d4 g1f3 b8c6 f1c4 d5b6 c4b3 d7d5 e5d6 d8d6 e1g1 c8e6 b1a3 e6b3 a2b3 d4c3 d1e2 e7e6 b2c3 f8e7 a3b5 d6b8",
    // Rossolimo (2...Nc6 3.Bb5)
    "e2e4 c7c5 g1f3 b8c6 f1b5 g7g6 b5c6 d7c6 d2d3 f8g7 h2h3 g8f6 b1c3 e8g8 c1e3 b7b6 d1d2 e7e5 e1g1 d8d6 a2a3 f8d8",
    "e2e4 c7c5 g1f3 b8c6 f1b5 e7e6 e1g1 g8e7 f1e1 a7a6 b5f1 d7d5 e4d5 e7d5 d2d4 g8f6 c2c4 d5b6 d4d5 f8e7 b1c3 e8g8 c1f4 e6d5 c4d5 e7d6",

    // --- 1. e4 e6 (French Defense) ---
    // Winawer Variation
    "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3 g8e7 d1g4 e8g8 f1d3 b8c6 g4h5 e7g6 g1f3 d8c7 c1e3 c5c4 d3g6 f7g6 h5g4 c8d7 h2h4 f8f5",
    // Classical / Steinitz
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6 e4e5 f6d7 f2f4 c7c5 g1f3 b8c6 c1e3 c5d4 f3d4 f8c5 d1d2 e8g8 e1c1 a7a6 d2f2 c5d4 e3d4 b7b5 f1d3 b5b4 c3e2 a6a5",
    // Tarrasch Variation (3.Nd2)
    "e2e4 e7e6 d2d4 d7d5 b1d2 c7c5 e4d5 d8d5 g1f3 c5d4 f1c4 d5d6 e1g1 g8f6 d2b3 b8c6 b3d4 c6d4 f3d4 a7a6 f1e1 d6c7 c4b3 f8d6 d4f5 d6h2 g1h1 e8g8 f5g7 f8d8 d1f3 g8g7 c1h6 g7g6 c2c3",
    // Advance Variation
    "e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6 a2a3 c5c4 b1d2 c6a5 f1e2 c8d7 e1g1 g8e7 f1e1 h7h6 d2f1 a5b3 a1b1 b3c1 d1c1 e8c8",

    // --- 1. e4 c6 (Caro-Kann Defense) ---
    // Advance Variation (Short & Shirov)
    "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 c6c5 c1e3 b8c6 e1g1 c5d4 f3d4 g8e7 b1d2 c6e5 c2c4 e5c6 d1a4 d8d7 c4d5 e7d5 d4f5 e6f5 d2c4 f8e7 f1d1",
    "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 b8d7 e1g1 h7h6 b1d2 g8e7 d2b3 g7g5 c1d2 f8g7 a2a4 e8g8 a4a5 a8c8 d2b4 f8e8",
    // Classical Variation (4...Bf5)
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7 h4h5 g6h7 f1d3 h7d3 d1d3 e7e6 c1d2 g8f6 e1c1 f8e7 c1b1 e8g8 g3e4 c6c5 e4f6 d7f6",
    // Tartakower / Korchnoi (4...Nf6)
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 g8f6 e4f6 e7f6 c2c3 f8d6 f1d3 e8g8 d1c2 f8e8 g1e2 h7h5 e1g1 b8d7 c1d2 d7f8 a1e1 c8e6 c3c4 d6c7",
    // Panov-Botvinnik Attack
    "e2e4 c7c6 d2d4 d7d5 e4d5 c6d5 c2c4 g8f6 b1c3 e7e6 g1f3 f8b4 c4d5 f6d5 c1d2 b8c6 f1d3 e8g8 e1g1 b4e7 a2a3 d5f6 d2g5 b7b6 f1e1 c8b7 d3c2 f8e8 d1d3 g7g6 a1d1",

    // --- 1. d4 d5 (Closed Games & Queen's Gambit) ---
    // QGD: Orthodox / Tartakower / Lasker
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 h7h6 g5h4 e8g8 g1f3 b7b6 c4d5 f6d5 h4e7 d8e7 c3d5 e6d5 a1c1 c8e6 d1a4 c7c5 a4a3 f8c8 f1b5 a7a6 d4c5 b6c5 e1g1 a8a7 b5e2 c5c4 a3e7 a7e7",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 b8d7 a1c1 c7c6 f1d3 d5c4 d3c4 f6d5 g5e7 d8e7 e1g1 d5c3 c1c3 e6e5 d1c2 e5d4 e3d4 d7f6 f1e1 e7d6",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 h7h6 g5h4 e8g8 g1f3 f6e4 h4e7 d8e7 c4d5 e4c3 b2c3 e6d5 d1b3 c7c6 c3c4 d5c4 f1c4 b8d7 e1g1 b7b5 c4e2 a7a6 a2a4 c8b7",
    // QGD: Exchange Variation
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c4d5 e6d5 c1g5 c7c6 e2e3 c8f5 d1f3 f5g6 g5f6 d8f6 f3f6 g7f6 g1f3 b8d7 f3h4 f8e7 g2g3 d7b6 e1c1 a7a5 c1d2 a5a4 a2a3 e8d8 f1d3 b6c8 d2c2 c8d6",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c4d5 e6d5 c1g5 f8e7 e2e3 e8g8 f1d3 b8d7 d1c2 f8e8 g1f3 c7c6 e1g1 d7f8 h2h3 g7g6 a1b1 f8e6 g5h4 e6g7 b2b4 a7a6 a2a4 c8f5",
    // Slav Defense: Chebanenko (4...a6)
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 a7a6 e2e3 b7b5 b2b3 c8g4 f1e2 e7e6 e1g1 b8d7 c1b2 f8d6 f3e5 g4e2 c3e2 d8c7 c4d5 c6d5 a1c1 c7b8 e2f4 e8g8 e5d7 f6d7 f4d3 d6h2 g1h1 h2d6",
    // Slav Defense: Main Line (4...dxc4 5.a4)
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5 f3e5 e7e6 f2f3 f8b4 e2e4 f5e4 f3e4 f6e4 c1d2 d8d4 c3e4 d4e4 d1e2 b4d2 e1d2 e4d5 d2c2 b8a6 e5c4 e8g8",
    // Semi-Slav: Meran Variation
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 e2e3 b8d7 f1d3 d5c4 d3c4 b7b5 c4d3 a7a6 e3e4 c6c5 e4e5 c5d4 c3b5 a6b5 e5f6 g7f6 e1g1 d8b6 d1e2 c8b7 f1d1 f8c5 d3b5 h8g8 c1f4 e8e7 f4g3 a8d8 a2a4",
    // Semi-Slav: Moscow / Botvinnik
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 c1g5 h7h6 g5h4 d5c4 e2e4 g7g5 h4g3 b7b5 f1e2 c8b7 e1g1 b8d7 f3e5 f8g7 e5d7 f6d7 g3d6 a7a6 e4e5 c6c5 a2a4 c5d4 d1d4 d8b6 d4b6 d7b6 a4b5",
    // Queen's Gambit Accepted
    "d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5 e1g1 a7a6 d4c5 d8d1 f1d1 f8c5 b1d2 b8d7 c4e2 b7b6 d2c4 c8b7 b2b3 e8e7 c1b2 h8d8 a1c1 a8c8 f3d4",

    // --- 1. d4 Nf6 (Indian Defenses) ---
    // King's Indian: Classical / Mar del Plata
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5 e1g1 b8c6 d4d5 c6e7 f3e1 f6d7 c1e3 f7f5 f2f3 f5f4 e3f2 g6g5 c4c5 d7c5 b2b4 c5d7 a2a4 e7g6 a4a5 f8f7 c3b5 a7a6 b5a7 d7f6 a7c8 a8c8",
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5 e1g1 b8c6 d4d5 c6e7 b2b4 f6h5 f1e1 f7f5 f3g5 h5f6 e2f3 c7c6 b4b5 c6d5 c4d5 h7h6 g5e6 c8e6 d5e6 a8c8 c1b2",
    // King's Indian: Saemisch Variation
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 f2f3 e8g8 c1e3 b8c6 g1e2 a7a6 d1d2 a8b8 e2c1 e7e5 d4d5 c6d4 c1b3 d4b3 a2b3 c7c5 d5c6 b7c6 e1c1 b8b3 d2d6 c8d7 c1c2 b3b7 b2b4",
    // Grunfeld: Exchange Variation
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7 g1f3 c7c5 a1b1 e8g8 f1e2 b8c6 d4d5 c6e5 f3e5 g7e5 d1d2 e7e6 f2f4 e5c7 e1g1 e6d5 e4d5 c7a5 b1b5 b7b6 d5d6 c8d7 b5b1 f8e8",
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7 f1c4 c7c5 g1e2 e8g8 e1g1 b8c6 c1e3 c8g4 f2f3 c6a5 c4d3 c5d4 c3d4 g4e6 d4d5 g7a1 d1a1 f7f6 e3h6 f8e8 a1d4 e6f7",
    // Grunfeld: Russian System (4.Nf3 Bg7 5.Qb3)
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 g1f3 f8g7 d1b3 d5c4 b3c4 e8g8 e2e4 a7a6 f1e2 b7b5 c4b3 c7c5 d4c5 c8b7 e4e5 f6d7 c1e3 e7e6 e1g1 d8c7 a1c1 d7e5 f3e5 g7e5 f2f4 e5g7 a2a4 b5a4 c3a4",
    // Nimzo-Indian: Rubinstein (4.e3)
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3 c7c5 e1g1 d5c4 d3c4 b8d7 a2a3 c5d4 e3d4 b4c3 b2c3 d8c7 d1e2 b7b6 c1d2 c8b7 c4d3 a8c8 a3a4 f8e8 f1e1 c7c6 a4a5",
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 c7c5 g1e2 c5d4 e3d4 d7d5 a2a3 b4e7 c4c5 b7b6 b2b4 e8g8 g2g3 a7a5 c1b2 c8b7 f1g2 a5b4 a3b4 a8a1 d1a1 b8c6 a1a4 d8a8 e1g1 a8a4",
    // Nimzo-Indian: Classical (4.Qc2)
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 d1c2 e8g8 a2a3 b4c3 c2c3 b7b6 c1g5 c8b7 f2f3 d7d5 e2e3 b8d7 c4d5 f6d5 g5d8 d5c3 d8h4 c3d5 h4f2 c7c5 e3e4 d5e7 g1e2 a8c8 a1d1 f8d8 e2c3 a7a6 f1e2",
    // Catalan: Open & Closed
    "d2d4 g8f6 c2c4 e7e6 g1f3 d7d5 g2g3 f8e7 f1g2 e8g8 e1g1 d5c4 d1c2 a7a6 a2a4 c8d7 c2c4 d7c6 c1g5 c6d5 c4d3 c7c5 b1c3 d5c6 a1d1 b8d7 e2e4 c5d4 f3d4 d7e5 d3e2 d8b6 d4c6 e5c6 e4e5 f6d5",
    "d2d4 g8f6 c2c4 e7e6 g1f3 d7d5 g2g3 f8e7 f1g2 e8g8 e1g1 b8d7 d1c2 c7c6 b1d2 b7b6 e2e4 c8b7 e4e5 f6e8 c4d5 c6d5 f1e1 a8c8 c2d1 e8c7 d2f1 b6b5 h2h4 b5b4 c1g5 d7b6 f1e3 b6c4 b2b3",

    // --- 1. c4 & 1. Nf3 (English & Reti) ---
    // English: Four Knights
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 f8b4 f1g2 e8g8 e1g1 e5e4 f3g5 b4c3 b2c3 f8e8 f2f3 e4f3 g5f3 d7d5 c4d5 d8d5 d2d3 d5c5 g1h1 c8g4 a1b1 a8b8 e2e4 c5c3 c1b2 c3a5 d1c1 g4f3 g2f3 c6e5 f3e2 b8d8 d3d4 e5g6",
    // English: Symmetrical
    "c2c4 c7c5 g1f3 g8f6 d2d4 c5d4 f3d4 e7e6 g2g3 d7d5 f1g2 e6e5 d4f3 d5d4 e1g1 b8c6 e2e3 f8e7 e3d4 e5d4 c1f4 e8g8 f3e5 c6e5 f4e5 e7c5 b1d2 f8e8 f1e1 c8g4 e5f6 g4d1 e1e8 d8e8 a1d1 g7f6 d2e4 c5e7",
    // Reti Opening (1.Nf3 d5 2.c4)
    "g1f3 d7d5 c2c4 d5d4 g2g3 c7c5 f1g2 b8c6 e1g1 e7e5 d2d3 g8f6 e2e3 f8e7 e3d4 c5d4 f1e1 f6d7 a2a3 a7a5 b1d2 e8g8 d2e4 h7h6 g3g4 d7c5 e4c5 e7c5 f3e5 c6e5 e1e5 c5d6 e5d5 d8c7 h2h3 c8e6",
    "g1f3 g8f6 c2c4 g7g6 b2b4 f8g7 c1b2 e8g8 g2g3 d7d6 f1g2 e7e5 e1g1 f8e8 d2d3 c7c6 b1d2 a7a5 a2a3 b8a6 b2c3 a5b4 a3b4 c8g4 h2h3 g4d7 d1b3 h7h6 f1b1 d7e6 b3b2 f6d7 d2b3 d8c7",
    NULL
};

static int cmp_compiled(const void *a, const void *b) {
    const struct CompiledEntry *ea = (const struct CompiledEntry *)a;
    const struct CompiledEntry *eb = (const struct CompiledEntry *)b;
    if (ea->key < eb->key) return -1;
    if (ea->key > eb->key) return 1;
    if (ea->move < eb->move) return -1;
    if (ea->move > eb->move) return 1;
    return 0;
}

int main(void) {
    init_zobrist();
    init_attacks();

    // 1. Ingest existing raw_book
    struct Board b;
    for (size_t i = 0; i < RAW_BOOK_SIZE; ++i) {
        if (!set_fen(&b, raw_book[i].fen))
            continue;
        uint64_t key = board_key(&b);
        for (int m = 0; m < raw_book[i].count; ++m) {
            move_t parsed = parse_move(&b, raw_book[i].moves[m]);
            if (parsed)
                add_entry(key, parsed, raw_book[i].weights[m]);
        }
    }

    // 2. Ingest GM Repertoire lines
    for (int i = 0; gm_lines[i] != NULL; ++i) {
        add_line(gm_lines[i], 80);
    }

    // 3. Sort entries
    qsort(entries, total_entries, sizeof(struct CompiledEntry), cmp_compiled);

    // 4. Output the C source table
    printf("// Generated compact binary book table\n");
    printf("// Total unique (key, move) entries: %d\n\n", total_entries);
    printf("#include \"book.h\"\n");
    printf("#include \"platform.h\"\n");
    printf("#include \"zobrist.h\"\n\n");
    printf("#include <stdlib.h>\n\n");
    printf("bool option_own_book = true;\n\n");
    printf("// Sorted by key. Parallel arrays avoid struct padding (11 bytes/entry vs 16).\n");
    printf("#define BOOK_SIZE %d\n\n", total_entries);
    printf("// clang-format off\n");
    printf("static const uint64_t book_keys[BOOK_SIZE] = {\n");
    for (int i = 0; i < total_entries; ++i) {
        printf("%s0x%016llxULL,%s", (i % 4 == 0) ? "    " : " ",
               (unsigned long long)entries[i].key,
               (i % 4 == 3 || i == total_entries - 1) ? "\n" : "");
    }
    printf("};\n\n");
    printf("static const uint16_t book_moves[BOOK_SIZE] = {\n");
    for (int i = 0; i < total_entries; ++i) {
        printf("%s0x%04x,%s", (i % 10 == 0) ? "    " : " ",
               (unsigned int)entries[i].move,
               (i % 10 == 9 || i == total_entries - 1) ? "\n" : "");
    }
    printf("};\n\n");
    printf("static const uint8_t book_weights[BOOK_SIZE] = {\n");
    for (int i = 0; i < total_entries; ++i) {
        printf("%s%3u,%s", (i % 16 == 0) ? "    " : " ",
               (unsigned int)entries[i].weight,
               (i % 16 == 15 || i == total_entries - 1) ? "\n" : "");
    }
    printf("};\n");
    printf("// clang-format on\n\n");
    printf("static uint64_t book_prng_state = 0x853c49e6748fea9bULL;\n\n");
    printf("static inline uint32_t book_rand(void) {\n");
    printf("    book_prng_state ^= book_prng_state >> 12;\n");
    printf("    book_prng_state ^= book_prng_state << 25;\n");
    printf("    book_prng_state ^= book_prng_state >> 27;\n");
    printf("    return (uint32_t)((book_prng_state * 0x2545F4914F6CDD1DULL) >> 32);\n");
    printf("}\n\n");
    printf("static int cmp_book_key(const void *a, const void *b) {\n");
    printf("    uint64_t ka = *(const uint64_t *)a;\n");
    printf("    uint64_t kb = *(const uint64_t *)b;\n");
    printf("    if (ka < kb) return -1;\n");
    printf("    if (ka > kb) return 1;\n");
    printf("    return 0;\n");
    printf("}\n\n");
    printf("void init_book(void) {\n");
    printf("    book_prng_state ^= get_time_ms();\n");
    printf("}\n\n");
    printf("move_t probe_book(const struct Board *b) {\n");
    printf("    if (!option_own_book || BOOK_SIZE == 0) return 0;\n");
    printf("    uint64_t key = board_key(b);\n");
    printf("    const uint64_t *match = (const uint64_t *)bsearch(\n");
    printf("        &key, book_keys, BOOK_SIZE, sizeof(uint64_t), cmp_book_key);\n");
    printf("    if (!match) return 0;\n\n");
    printf("    int first = (int)(match - book_keys);\n");
    printf("    while (first > 0 && book_keys[first - 1] == key)\n");
    printf("        first--;\n\n");
    printf("    int last = first;\n");
    printf("    int total_weight = 0;\n");
    printf("    while (last < BOOK_SIZE && book_keys[last] == key) {\n");
    printf("        total_weight += book_weights[last];\n");
    printf("        last++;\n");
    printf("    }\n\n");
    printf("    if (total_weight <= 0) return book_moves[first];\n\n");
    printf("    int r = (int)(book_rand() %% (uint32_t)total_weight);\n");
    printf("    int acc = 0;\n");
    printf("    for (int i = first; i < last; ++i) {\n");
    printf("        acc += book_weights[i];\n");
    printf("        if (r < acc) return book_moves[i];\n");
    printf("    }\n");
    printf("    return book_moves[first];\n");
    printf("}\n");
    return 0;
}
