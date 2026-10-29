#include "book.h"
#include "movegen.h"
#include "platform.h"
#include "zobrist.h"

#include <stdlib.h>
#include <string.h>

bool option_own_book = true;

struct RawBookEntry {
    const char *fen;
    const char *moves[5];
    uint8_t weights[5];
    int count;
};

struct BookKeyEntry {
    uint64_t key;
    move_t move;
    uint8_t weight;
};

// clang-format off
static const struct RawBookEntry raw_book[] = {
    // Initial Position
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", { "e2e4", "d2d4", "c2c4", "g1f3" }, { 45, 35, 10, 10 }, 4 },
    // 1. e4
    { "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", { "c7c5", "e7e5", "e7e6", "c7c6", "d7d5" }, { 45, 30, 12, 10, 3 }, 5 },
    // Sicilian Defense
    { "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2", { "g1f3", "c2c3", "b1c3", "f2f4" }, { 70, 15, 10, 5 }, 4 },
    // Open Sicilian 2...
    { "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", { "d7d6", "b8c6", "e7e6", "g7g6" }, { 45, 25, 25, 5 }, 4 },
    // Sicilian 2...d6
    { "rnbqkbnr/pp2pppp/3p4/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3", { "d2d4", "f1b5", "c2c3" }, { 80, 15, 5 }, 3 },
    // Open Sicilian 3.d4 cxd4
    { "rnbqkbnr/pp2pppp/3p4/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq d3 0 3", { "c5d4" }, { 100 }, 1 },
    // Open Sicilian 4.Nxd4
    { "rnbqkbnr/pp2pppp/3p4/8/3pP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 4", { "f3d4" }, { 100 }, 1 },
    // Open Sicilian 4...Nf6
    { "rnbqkbnr/pp2pppp/3p4/8/3NP3/8/PPP2PPP/RNBQKB1R b KQkq - 0 4", { "g8f6" }, { 100 }, 1 },
    // Open Sicilian 5.Nc3
    { "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/8/PPP2PPP/RNBQKB1R w KQkq - 1 5", { "b1c3" }, { 100 }, 1 },
    // Sicilian 5.Nc3 choices
    { "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R b KQkq - 2 5", { "a7a6", "g7g6", "e7e6", "b8c6" }, { 50, 25, 15, 10 }, 4 },
    // Najdorf 6th move
    { "rnbqkb1r/1p2pppp/p2p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 6", { "c1e3", "c1g5", "f1c4", "f1e2" }, { 40, 30, 15, 15 }, 4 },
    // Najdorf English Attack 6...
    { "rnbqkb1r/1p2pppp/p2p1n2/8/3NP3/2N1B3/PPP2PPP/R2QKB1R b KQkq - 1 6", { "e7e5", "e7e6", "g8g4" }, { 60, 30, 10 }, 3 },
    // Najdorf English Attack 7.Nb3
    { "rnbqkb1r/1p3ppp/p2p1n2/4p3/3NP3/2N1B3/PPP2PPP/R2QKB1R w KQkq e6 0 7", { "d4b3", "d4f3" }, { 85, 15 }, 2 },
    // Najdorf English Attack 7...Be6
    { "rnbqkb1r/1p3ppp/p2p1n2/4p3/4P3/1NN1B3/PPP2PPP/R2QKB1R b KQkq - 1 7", { "c8e6", "f8e7" }, { 60, 40 }, 2 },
    // Dragon 6th move
    { "rnbqkb1r/pp2pp1p/3p1np1/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 6", { "c1e3", "f1e2", "f1c4" }, { 60, 20, 20 }, 3 },
    // Dragon 6...Bg7
    { "rnbqkb1r/pp2pp1p/3p1np1/8/3NP3/2N1B3/PPP2PPP/R2QKB1R b KQkq - 1 6", { "f8g7" }, { 100 }, 1 },
    // Dragon Yugoslav Setup
    { "rnbqk2r/pp2ppbp/3p1np1/8/3NP3/2N1B3/PPP2PPP/R2QKB1R w KQkq - 2 7", { "f2f3", "f1c4" }, { 70, 30 }, 2 },
    // Classical Sicilian 6.Bg5
    { "r1bqkb1r/pp2pppp/2np1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 3 6", { "c1g5", "f1c4", "c1e3" }, { 55, 30, 15 }, 3 },
    // Sicilian 2...e6
    { "rnbqkbnr/pp1p1ppp/4p3/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3", { "d2d4", "c2c3", "b1c3" }, { 75, 15, 10 }, 3 },
    // Sicilian 2...e6 3.d4 cxd4
    { "rnbqkbnr/pp1p1ppp/4p3/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq d3 0 3", { "c5d4" }, { 100 }, 1 },
    // Sicilian 2...e6 4.Nxd4
    { "rnbqkbnr/pp1p1ppp/4p3/8/3pP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 4", { "f3d4" }, { 100 }, 1 },
    // Kan / Taimanov / Four Knights
    { "rnbqkbnr/pp1p1ppp/4p3/8/3NP3/8/PPP2PPP/RNBQKB1R b KQkq - 0 4", { "a7a6", "b8c6", "g8f6" }, { 45, 40, 15 }, 3 },
    // Kan 5th move
    { "rnbqkbnr/1p1p1ppp/p3p3/8/3NP3/8/PPP2PPP/RNBQKB1R w KQkq - 0 5", { "f1d3", "c2c4", "b1c3" }, { 50, 30, 20 }, 3 },
    // Sicilian 2...Nc6
    { "r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", { "d2d4", "f1b5", "b1c3" }, { 65, 25, 10 }, 3 },
    // Rossolimo 3...
    { "r1bqkbnr/pp1ppppp/2n5/1Bp5/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", { "g7g6", "e7e6", "d7d6" }, { 50, 30, 20 }, 3 },
    // Alapin responses
    { "rnbqkbnr/pp1ppppp/8/2p5/4P3/2P5/PP1P1PPP/RNBQKBNR b KQkq - 0 2", { "d7d5", "g8f6" }, { 55, 45 }, 2 },
    // Alapin 3.exd5
    { "rnbqkbnr/pp2pppp/8/2pp4/4P3/2P5/PP1P1PPP/RNBQKBNR w KQkq d6 0 3", { "e4d5" }, { 100 }, 1 },
    // Alapin 3...Qxd5
    { "rnbqkbnr/pp2pppp/8/2pP4/8/2P5/PP1P1PPP/RNBQKBNR b KQkq - 0 3", { "d8d5" }, { 100 }, 1 },
    // Alapin 4.d4
    { "rnb1kbnr/pp2pppp/8/2pq4/8/2P5/PP1P1PPP/RNBQKBNR w KQkq - 0 4", { "d2d4", "g1f3" }, { 90, 10 }, 2 },
    // Alapin 2...Nf6 3.e5
    { "rnbqkb1r/pp1ppppp/5n2/2p5/4P3/2P5/PP1P1PPP/RNBQKBNR w KQkq - 1 3", { "e4e5" }, { 100 }, 1 },
    // Alapin 3...Nd5
    { "rnbqkb1r/pp1ppppp/5n2/2p1P3/8/2P5/PP1P1PPP/RNBQKBNR b KQkq - 0 3", { "f6d5" }, { 100 }, 1 },
    // Alapin 4.d4
    { "rnbqkb1r/pp1ppppp/8/2pnP3/8/2P5/PP1P1PPP/RNBQKBNR w KQkq - 1 4", { "d2d4", "g1f3" }, { 80, 20 }, 2 },
    // 1.e4 e5
    { "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", { "g1f3", "f1c4", "b1c3", "f2f4" }, { 75, 15, 5, 5 }, 4 },
    // 1.e4 e5 2.Nf3
    { "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", { "b8c6", "g8f6", "d7d6" }, { 75, 20, 5 }, 3 },
    // Petroff Defense 3rd move
    { "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", { "f3e5", "d2d4" }, { 75, 25 }, 2 },
    // Petroff 3...d6
    { "rnbqkb1r/pppp1ppp/5n2/4N3/4P3/8/PPPP1PPP/RNBQKB1R b KQkq - 0 3", { "d7d6", "f6e4" }, { 95, 5 }, 2 },
    // Petroff 4.Nf3
    { "rnbqkb1r/ppp2ppp/3p1n2/4N3/4P3/8/PPPP1PPP/RNBQKB1R w KQkq - 0 4", { "e5f3", "e5c4" }, { 95, 5 }, 2 },
    // Petroff 4...Nxe4
    { "rnbqkb1r/ppp2ppp/3p1n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 4", { "f6e4" }, { 100 }, 1 },
    // Petroff 5.d4
    { "rnbqkb1r/ppp2ppp/3p4/8/4n3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 5", { "d2d4", "d1e2" }, { 80, 20 }, 2 },
    // Open Game 3rd move
    { "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", { "f1b5", "f1c4", "d2d4", "b1c3" }, { 55, 30, 10, 5 }, 4 },
    // Ruy Lopez 3...
    { "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", { "a7a6", "g8f6", "d7d6" }, { 70, 25, 5 }, 3 },
    // Ruy Lopez 4th move
    { "r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4", { "b5a4", "b5c6" }, { 85, 15 }, 2 },
    // Ruy Lopez 4...Nf6
    { "r1bqkbnr/1ppp1ppp/p1n5/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 1 4", { "g8f6", "d7d6" }, { 90, 10 }, 2 },
    // Ruy Lopez 5.O-O
    { "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 5", { "e1g1", "d2d3" }, { 85, 15 }, 2 },
    // Ruy Lopez Main/Open
    { "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1NK1 b kq - 3 5", { "f8e7", "b7b5", "f6e4" }, { 65, 25, 10 }, 3 },
    // Ruy Lopez 6.Re1
    { "r1bqk2r/1pppbppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1NK1 w kq - 4 6", { "f1e1", "d2d3" }, { 90, 10 }, 2 },
    // Ruy Lopez 6...b5
    { "r1bqk2r/1pppbppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQN1K1 b kq - 5 6", { "b7b5" }, { 100 }, 1 },
    // Ruy Lopez 7.Bb3
    { "r1bqk2r/2ppbppp/p1n2n2/1p2p3/B3P3/5N2/PPPP1PPP/RNBQN1K1 w kq b6 0 7", { "a4b3" }, { 100 }, 1 },
    // Ruy Lopez 7...d6/O-O
    { "r1bqk2r/2ppbppp/p1n2n2/1p2p3/4P3/1B3N2/PPPP1PPP/RNBQN1K1 b kq - 1 7", { "d7d6", "e1g1" }, { 75, 25 }, 2 },
    // Berlin Defense 4th move
    { "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", { "e1g1", "d2d3" }, { 75, 25 }, 2 },
    // Berlin 4...Nxe4
    { "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQ1NK1 b kq - 5 4", { "f6e4", "f8c5" }, { 80, 20 }, 2 },
    // Berlin 5.d4
    { "r1bqkb1r/pppp1ppp/2n5/1B2p3/4n3/5N2/PPPP1PPP/RNBQ1NK1 w kq - 0 5", { "d2d4", "f1e1" }, { 80, 20 }, 2 },
    // Italian Game 3...
    { "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", { "f8c5", "g8f6" }, { 55, 45 }, 2 },
    // Giuoco Piano 4th move
    { "r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", { "c2c3", "d2d3", "b2b4" }, { 50, 35, 15 }, 3 },
    // Giuoco Piano 4...Nf6
    { "r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/2P2N2/PP1P1PPP/RNBQK2R b KQkq - 0 4", { "g8f6", "d8e7" }, { 90, 10 }, 2 },
    // Giuoco Piano 5.d3/d4
    { "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2P2N2/PP1P1PPP/RNBQK2R w KQkq - 1 5", { "d2d3", "d2d4" }, { 60, 40 }, 2 },
    // Two Knights Defense
    { "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", { "d2d3", "f3g5" }, { 60, 40 }, 2 },
    // Two Knights 4...d5
    { "r1bqkb1r/pppp1ppp/2n2n2/4p1N1/2B1P3/8/PPPP1PPP/RNBQK2R b KQkq - 5 4", { "d7d5" }, { 100 }, 1 },
    // Two Knights 5.exd5
    { "r1bqkb1r/ppp2ppp/2n2n2/3pp1N1/2B1P3/8/PPPP1PPP/RNBQK2R w KQkq d6 0 5", { "e4d5" }, { 100 }, 1 },
    // Two Knights 5...Na5
    { "r1bqkb1r/ppp2ppp/2n2n2/3Pp1N1/2B5/8/PPPP1PPP/RNBQK2R b KQkq - 0 5", { "c6a5", "c6d4" }, { 85, 15 }, 2 },
    // Scotch 3...exd4
    { "r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq d3 0 3", { "e5d4" }, { 100 }, 1 },
    // Scotch 4.Nxd4
    { "r1bqkbnr/pppp1ppp/2n5/8/3pP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 4", { "f3d4" }, { 100 }, 1 },
    // Scotch 4...
    { "r1bqkbnr/pppp1ppp/2n5/8/3NP3/8/PPP2PPP/RNBQKB1R b KQkq - 0 4", { "g8f6", "f8c5", "d8h4" }, { 50, 40, 10 }, 3 },
    // French Defense 2.d4
    { "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", { "d2d4", "d2d3" }, { 95, 5 }, 2 },
    // French Defense 2...d5
    { "rnbqkbnr/pppp1ppp/4p3/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 2", { "d7d5" }, { 100 }, 1 },
    // French Defense 3rd move
    { "rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq d6 0 3", { "b1c3", "b1d2", "e4e5", "e4d5" }, { 50, 25, 20, 5 }, 4 },
    // French 3.Nc3 responses
    { "rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", { "g8f6", "f8b4", "d5e4" }, { 45, 45, 10 }, 3 },
    // Winawer 4.e5
    { "rnbqk1nr/ppp2ppp/4p3/3p4/1b1PP3/2N5/PPP2PPP/R1BQKBNR w KQkq - 2 4", { "e4e5", "a2a3" }, { 90, 10 }, 2 },
    // Winawer 4...c5
    { "rnbqk1nr/ppp2ppp/4p3/3pP3/1b1P4/2N5/PPP2PPP/R1BQKBNR b KQkq - 0 4", { "c7c5" }, { 100 }, 1 },
    // Winawer 5.a3
    { "rnbqk1nr/pp3ppp/4p3/2ppP3/1b1P4/2N5/PPP2PPP/R1BQKBNR w KQkq c6 0 5", { "a2a3", "c1d2" }, { 80, 20 }, 2 },
    // Winawer 5...Bxc3+
    { "rnbqk1nr/pp3ppp/4p3/2ppP3/1b1P4/P1N5/1PP2PPP/R1BQKBNR b KQkq - 0 5", { "b4c3" }, { 100 }, 1 },
    // Winawer 6.bxc3
    { "rnbqk1nr/pp3ppp/4p3/2ppP3/3P4/P1b5/1PP2PPP/R1BQKBNR w KQkq - 0 6", { "b2c3" }, { 100 }, 1 },
    // Winawer 6...Ne7
    { "rnbqk1nr/pp3ppp/4p3/2ppP3/3P4/P1P5/2P2PPP/R1BQKBNR b KQkq - 0 6", { "g8e7", "d8c7" }, { 90, 10 }, 2 },
    // French Tarrasch 3...
    { "rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPPN1PPP/R1BQKBNR b KQkq - 1 3", { "c7c5", "g8f6", "d5e4" }, { 55, 40, 5 }, 3 },
    // French Advance 3...c5
    { "rnbqkbnr/ppp2ppp/4p3/3pP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3", { "c7c5" }, { 100 }, 1 },
    // French Advance 4.c3
    { "rnbqkbnr/pp3ppp/4p3/2ppP3/3P4/8/PPP2PPP/RNBQKBNR w KQkq c6 0 4", { "c2c3", "g1f3" }, { 90, 10 }, 2 },
    // French Advance 4...Nc6
    { "rnbqkbnr/pp3ppp/4p3/2ppP3/3P4/2P5/PP3PPP/RNBQKBNR b KQkq - 0 4", { "b8c6", "d8b6" }, { 90, 10 }, 2 },
    // French Advance 5.Nf3
    { "r1bqkbnr/pp3ppp/2n1p3/2ppP3/3P4/2P5/PP3PPP/RNBQKBNR w KQkq - 1 5", { "g1f3" }, { 100 }, 1 },
    // French Advance 5...Qb6
    { "r1bqkbnr/pp3ppp/2n1p3/2ppP3/3P4/2P2N2/PP3PPP/RNBQKB1R b KQkq - 2 5", { "d8b6", "c8d7" }, { 75, 25 }, 2 },
    // Caro-Kann 2.d4
    { "rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", { "d2d4", "b1c3", "c2c4" }, { 90, 5, 5 }, 3 },
    // Caro-Kann 2...d5
    { "rnbqkbnr/pp1ppppp/2p5/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 2", { "d7d5" }, { 100 }, 1 },
    // Caro-Kann 3rd move
    { "rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq d6 0 3", { "b1c3", "e4e5", "e4d5" }, { 50, 35, 15 }, 3 },
    // Caro-Kann 3...dxe4
    { "rnbqkbnr/pp2pppp/2p5/3p4/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 1 3", { "d5e4" }, { 100 }, 1 },
    // Caro-Kann 4.Nxe4
    { "rnbqkbnr/pp2pppp/2p5/8/3Pp3/2N5/PPP2PPP/R1BQKBNR w KQkq - 0 4", { "c3e4" }, { 100 }, 1 },
    // Caro-Kann 4...
    { "rnbqkbnr/pp2pppp/2p5/8/3PN3/8/PPP2PPP/R1BQKBNR b KQkq - 0 4", { "c8f5", "b8d7", "g8f6" }, { 55, 30, 15 }, 3 },
    // Caro-Kann Classical 5.Ng3
    { "rn1qkbnr/pp2pppp/2p5/5b2/3PN3/8/PPP2PPP/R1BQKBNR w KQkq - 1 5", { "e4g3" }, { 100 }, 1 },
    // Caro-Kann Classical 5...Bg6
    { "rn1qkbnr/pp2pppp/2p5/5b2/3P4/6N1/PPP2PPP/R1BQKBNR b KQkq - 2 5", { "f5g6" }, { 100 }, 1 },
    // Caro-Kann Classical 6.h4
    { "rn1qkbnr/pp2pppp/2p3b1/8/3P4/6N1/PPP2PPP/R1BQKBNR w KQkq - 3 6", { "h2h4", "g1f3" }, { 80, 20 }, 2 },
    // Caro-Kann Classical 6...h6
    { "rn1qkbnr/pp2pppp/2p3b1/8/3P3P/6N1/PPP2PP1/R1BQKBNR b KQkq h3 0 6", { "h7h6" }, { 100 }, 1 },
    // Caro-Kann Classical 7.Nf3
    { "rn1qkbnr/pp2ppp1/2p3bp/8/3P3P/6N1/PPP2PP1/R1BQKBNR w KQkq - 0 7", { "g1f3" }, { 100 }, 1 },
    // Caro Advance 3...
    { "rnbqkbnr/pp2pppp/2p5/3pP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3", { "c8f5", "c6c5" }, { 85, 15 }, 2 },
    // Caro Advance 4th move
    { "rn1qkbnr/pp2pppp/2p5/3pPb2/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 1 4", { "g1f3", "b1c3", "f1d3" }, { 50, 30, 20 }, 3 },
    // Scandinavian 2.exd5
    { "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", { "e4d5" }, { 100 }, 1 },
    // Scandinavian 2...
    { "rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2", { "d8d5", "g8f6" }, { 70, 30 }, 2 },
    // Scandinavian 3.Nc3
    { "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3", { "b1c3" }, { 100 }, 1 },
    // Scandinavian 3...
    { "rnb1kbnr/ppp1pppp/8/3q4/8/2N5/PPPP1PPP/R1BQKBNR b KQkq - 1 3", { "d5a5", "d5d6", "d5d8" }, { 75, 15, 10 }, 3 },
    // Scandinavian 4.d4
    { "rnb1kbnr/ppp1pppp/8/q7/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4", { "d2d4", "g1f3" }, { 80, 20 }, 2 },
    // Pirc 2.d4
    { "rnbqkbnr/ppp1pppp/3p4/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", { "d2d4", "g1f3" }, { 85, 15 }, 2 },
    // Pirc 2...Nf6
    { "rnbqkbnr/ppp1pppp/3p4/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 2", { "g8f6", "g7g6" }, { 85, 15 }, 2 },
    // Pirc 3.Nc3
    { "rnbqkb1r/ppp1pppp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 1 3", { "b1c3" }, { 100 }, 1 },
    // Pirc 3...g6
    { "rnbqkb1r/ppp1pppp/3p1n2/8/3PP3/2N5/PPP2PPP/R1BQKBNR b KQkq - 2 3", { "g7g6" }, { 100 }, 1 },
    // Pirc 4th move
    { "rnbqkb1r/ppp1pp1p/3p1np1/8/3PP3/2N5/PPP2PPP/R1BQKBNR w KQkq - 0 4", { "g1f3", "f2f4", "c1e3" }, { 50, 30, 20 }, 3 },
    // Alekhine Defense 2.e5
    { "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", { "e4e5" }, { 100 }, 1 },
    // Alekhine 2...Nd5
    { "rnbqkb1r/pppppppp/5n2/4P3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2", { "f6d5" }, { 100 }, 1 },
    // Alekhine 3.d4
    { "rnbqkb1r/pppppppp/8/3nP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 1 3", { "d2d4", "c2c4" }, { 80, 20 }, 2 },
    // Alekhine 3...d6
    { "rnbqkb1r/pppppppp/8/3nP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq d3 0 3", { "d7d6" }, { 100 }, 1 },
    // 1. d4
    { "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1", { "g8f6", "d7d5", "e7e6", "f7f5" }, { 60, 30, 5, 5 }, 4 },
    // 1. d4 d5
    { "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq d6 0 2", { "c2c4", "g1f3", "c1f4" }, { 75, 15, 10 }, 3 },
    // London System 2...
    { "rnbqkbnr/ppp1pppp/8/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR b KQkq - 1 2", { "g8f6", "c7c5" }, { 70, 30 }, 2 },
    // London System 3.e3
    { "rnbqkb1r/ppp1pppp/5n2/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR w KQkq - 2 3", { "e2e3", "g1f3" }, { 80, 20 }, 2 },
    // Queen's Gambit 2...
    { "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2", { "e7e6", "c7c6", "d5c4" }, { 50, 35, 15 }, 3 },
    // QGD 3rd move
    { "rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", { "b1c3", "g1f3", "c4d5" }, { 60, 30, 10 }, 3 },
    // QGD 3...Nf6
    { "rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", { "g8f6", "c7c6", "f8e7" }, { 75, 15, 10 }, 3 },
    // QGD 4th move
    { "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", { "c1g5", "g1f3", "c4d5" }, { 45, 40, 15 }, 3 },
    // QGD 4...Be7
    { "rnbqkb1r/ppp2ppp/4pn2/3p2B1/2PP4/2N5/PP2PPPP/R2QKBNR b KQkq - 3 4", { "f8e7", "b8d7", "c7c6" }, { 70, 20, 10 }, 3 },
    // QGD 5.e3
    { "rnbqk2r/ppp1bppp/4pn2/3p2B1/2PP4/2N5/PP2PPPP/R2QKBNR w KQkq - 4 5", { "e2e3", "g1f3" }, { 90, 10 }, 2 },
    // QGD 5...O-O
    { "rnbqk2r/ppp1bppp/4pn2/3p2B1/2PP4/2N1P3/PP3PPP/R2QKBNR b KQkq - 0 5", { "e8g8", "h7h6" }, { 90, 10 }, 2 },
    // QGD 6.Nf3
    { "rnbq1nk1/ppp1bppp/4pn2/3p2B1/2PP4/2N1P3/PP3PPP/R2QKBNR w KQ - 1 6", { "g1f3", "a1c1" }, { 90, 10 }, 2 },
    // Slav 3rd move
    { "rnbqkbnr/pp2pppp/2p5/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", { "g1f3", "b1c3", "c4d5" }, { 60, 30, 10 }, 3 },
    // Slav 3...Nf6
    { "rnbqkbnr/pp2pppp/2p5/3p4/2PP4/5N2/PP2PPPP/RNBQKB1R b KQkq - 1 3", { "g8f6" }, { 100 }, 1 },
    // Slav 4th move
    { "rnbqkb1r/pp2pppp/2p2n2/3p4/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 2 4", { "b1c3", "e2e3" }, { 75, 25 }, 2 },
    // Slav 4...
    { "rnbqkb1r/pp2pppp/2p2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R b KQkq - 3 4", { "d5c4", "e7e6", "a7a6" }, { 50, 40, 10 }, 3 },
    // Slav 5.a4
    { "rnbqkb1r/pp2pppp/2p2n2/8/2pP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 0 5", { "a2a4", "e2e3" }, { 85, 15 }, 2 },
    // Slav 5...Bf5
    { "rnbqkb1r/pp2pppp/2p2n2/8/P1pP4/2N2N2/1P2PPPP/R1BQKB1R b KQkq a3 0 5", { "c8f5", "e7e6" }, { 85, 15 }, 2 },
    // Semi-Slav 5th move
    { "rnbqkb1r/pp3ppp/2p1pn2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 0 5", { "e2e3", "c1g5" }, { 60, 40 }, 2 },
    // QGA 3rd move
    { "rnbqkbnr/ppp1pppp/8/8/2pP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", { "g1f3", "e2e4", "e2e3" }, { 65, 20, 15 }, 3 },
    // QGA 3...Nf6
    { "rnbqkbnr/ppp1pppp/8/8/2pP4/5N2/PP2PPPP/RNBQKB1R b KQkq - 1 3", { "g8f6", "a7a6" }, { 85, 15 }, 2 },
    // QGA 4.e3
    { "rnbqkb1r/ppp1pppp/5n2/8/2pP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 2 4", { "e2e3", "e2e4" }, { 75, 25 }, 2 },
    // 1. d4 Nf6
    { "rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2", { "c2c4", "g1f3", "c1g5" }, { 80, 15, 5 }, 3 },
    // 1. d4 Nf6 2. c4
    { "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2", { "e7e6", "g7g6", "c7c5" }, { 45, 40, 15 }, 3 },
    // 2...e6 3rd move
    { "rnbqkb1r/pppp1ppp/4pn2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", { "b1c3", "g1f3", "g2g3" }, { 55, 30, 15 }, 3 },
    // Nimzo-Indian 3...Bb4
    { "rnbqkb1r/pppp1ppp/4pn2/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", { "f8b4", "d7d5" }, { 80, 20 }, 2 },
    // Nimzo-Indian 4th move
    { "rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", { "e2e3", "d1c2", "g1f3" }, { 55, 35, 10 }, 3 },
    // Nimzo Rubinstein 4...
    { "rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N1P3/PP3PPP/R1BQKBNR b KQkq - 0 4", { "e8g8", "c7c5", "b7b6" }, { 70, 20, 10 }, 3 },
    // Nimzo Classical 4...
    { "rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PPQ1PPPP/R1B1KBNR b KQkq - 3 4", { "e8g8", "c7c5", "d7d5" }, { 60, 30, 10 }, 3 },
    // Catalan 3...d5
    { "rnbqkb1r/pppp1ppp/4pn2/8/2PP4/6P1/PP2PP1P/RNBQKBNR b KQkq - 0 3", { "d7d5", "f8b4" }, { 85, 15 }, 2 },
    // Catalan 4.Bg2
    { "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/6P1/PP2PP1P/RNBQKBNR w KQkq d6 0 4", { "f1g2", "g1f3" }, { 90, 10 }, 2 },
    // Catalan 4...Be7
    { "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/6P1/PP2PPBP/RNBQK1NR b KQkq - 1 4", { "f8e7", "d5c4" }, { 60, 40 }, 2 },
    // KID / Grunfeld 3rd move
    { "rnbqkb1r/pppppp1p/5np1/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3", { "b1c3", "g1f3", "g2g3" }, { 75, 15, 10 }, 3 },
    // KID vs Grunfeld
    { "rnbqkb1r/pppppp1p/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3", { "f8g7", "d7d5" }, { 65, 35 }, 2 },
    // KID 4.e4
    { "rnbqk2r/ppppppbp/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4", { "e2e4", "g1f3" }, { 90, 10 }, 2 },
    // KID 4...d6
    { "rnbqk2r/ppppppbp/5np1/8/2PPP3/2N5/PP3PPP/R1BQKBNR b KQkq e3 0 4", { "d7d6" }, { 100 }, 1 },
    // KID 5.Nf3
    { "rnbqk2r/ppp1ppbp/3p1np1/8/2PPP3/2N5/PP3PPP/R1BQKBNR w KQkq - 0 5", { "g1f3", "f2f3", "c1e2" }, { 70, 20, 10 }, 3 },
    // KID 5...O-O
    { "rnbqk2r/ppp1ppbp/3p1np1/8/2PPP3/2N2N2/PP3PPP/R1BQKB1R b KQkq - 1 5", { "e8g8" }, { 100 }, 1 },
    // KID Classical 6.Be2
    { "rnbq1nk1/ppp1ppbp/3p1np1/8/2PPP3/2N2N2/PP3PPP/R1BQKB1R w KQ - 2 6", { "f1e2", "c1e3" }, { 85, 15 }, 2 },
    // KID 6...e5
    { "rnbq1nk1/ppp1ppbp/3p1np1/8/2PPP3/2N2N2/PP2BPPP/R1BQK2R b KQ - 3 6", { "e7e5", "c7c5" }, { 90, 10 }, 2 },
    // KID 7.O-O
    { "rnbq1nk1/ppp2pbp/3p1np1/4p3/2PPP3/2N2N2/PP2BPPP/R1BQK2R w KQ e6 0 7", { "e1g1", "d4d5" }, { 80, 20 }, 2 },
    // Grunfeld 4th move
    { "rnbqkb1r/ppp1pp1p/5np1/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq d6 0 4", { "c4d5", "g1f3", "d1b3" }, { 65, 20, 15 }, 3 },
    // Grunfeld 4...Nxd5
    { "rnbqkb1r/ppp1pp1p/5np1/3P4/3P4/2N5/PP2PPPP/R1BQKBNR b KQkq - 0 4", { "f6d5" }, { 100 }, 1 },
    // Grunfeld 5.e4
    { "rnbqkb1r/ppp1pp1p/6p1/3n4/3P4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 5", { "e2e4", "c1d2" }, { 90, 10 }, 2 },
    // Grunfeld 5...Nxc3
    { "rnbqkb1r/ppp1pp1p/6p1/3n4/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq e3 0 5", { "d5c3" }, { 100 }, 1 },
    // Grunfeld 6.bxc3
    { "rnbqkb1r/ppp1pp1p/6p1/8/3PP3/2n5/PP3PPP/R1BQKBNR w KQkq - 0 6", { "b2c3" }, { 100 }, 1 },
    // Grunfeld 6...Bg7
    { "rnbqkb1r/ppp1pp1p/6p1/8/3PP3/2P5/P4PPP/R1BQKBNR b KQkq - 0 6", { "f8g7" }, { 100 }, 1 },
    // Grunfeld 7.Nf3
    { "rnbqk2r/ppp1ppbp/6p1/8/3PP3/2P5/P4PPP/R1BQKBNR w KQkq - 1 7", { "g1f3", "f1c4" }, { 65, 35 }, 2 },
    // Benoni 3.d5
    { "rnbqkb1r/pp1ppppp/5n2/2p5/2PP4/8/PP2PPPP/RNBQKBNR w KQkq c6 0 3", { "d4d5", "g1f3" }, { 95, 5 }, 2 },
    // Benoni / Benko 3...
    { "rnbqkb1r/pp1ppppp/5n2/2pP4/2P5/8/PP2PPPP/RNBQKBNR b KQkq - 0 3", { "e7e6", "b7b5" }, { 60, 40 }, 2 },
    // Dutch 2nd move
    { "rnbqkbnr/ppppp1pp/8/5p2/3P4/8/PPP1PPPP/RNBQKBNR w KQkq f6 0 2", { "g1f3", "c2c4", "g2g3" }, { 45, 35, 20 }, 3 },
    // Dutch 2...Nf6
    { "rnbqkbnr/ppppp1pp/8/5p2/3P4/6P1/PPP1PP1P/RNBQKBNR b KQkq - 0 2", { "g8f6", "e7e6" }, { 90, 10 }, 2 },
    // Dutch 3.Bg2
    { "rnbqkb1r/ppppp1pp/5n2/5p2/3P4/6P1/PPP1PP1P/RNBQKBNR w KQkq - 1 3", { "f1g2" }, { 100 }, 1 },
    // 1. c4
    { "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1", { "e7e5", "c7c5", "g8f6", "e7e6" }, { 40, 30, 20, 10 }, 4 },
    // English 1...e5
    { "rnbqkbnr/pppp1ppp/8/4p3/2P5/8/PP1PPPPP/RNBQKBNR w KQkq e6 0 2", { "b1c3", "g2g3", "g1f3" }, { 70, 20, 10 }, 3 },
    // English 2...Nf6
    { "rnbqkbnr/pppp1ppp/8/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR b KQkq - 1 2", { "g8f6", "b8c6", "f7f5" }, { 65, 25, 10 }, 3 },
    // English 3.Nf3/g3
    { "rnbqkb1r/pppp1ppp/5n2/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR w KQkq - 2 3", { "g1f3", "g2g3" }, { 60, 40 }, 2 },
    // Symmetrical English 2nd move
    { "rnbqkbnr/pp1ppppp/8/2p5/2P5/8/PP1PPPPP/RNBQKBNR w KQkq c6 0 2", { "b1c3", "g1f3", "g2g3" }, { 60, 25, 15 }, 3 },
    // Symmetrical English 2...
    { "rnbqkbnr/pp1ppppp/8/2p5/2P5/2N5/PP1PPPPP/R1BQKBNR b KQkq - 1 2", { "b8c6", "g8f6" }, { 60, 40 }, 2 },
    // English 1...Nf6
    { "rnbqkb1r/pppppppp/5n2/8/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 1 2", { "b1c3", "g1f3", "d2d4" }, { 50, 30, 20 }, 3 },
    // 1. Nf3
    { "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1", { "d7d5", "g8f6", "c7c5" }, { 50, 40, 10 }, 3 },
    // 1. Nf3 d5
    { "rnbqkbnr/ppp1pppp/8/3p4/8/5N2/PPPPPPPP/RNBQKB1R w KQkq d6 0 2", { "d2d4", "g2g3", "c2c4" }, { 45, 35, 20 }, 3 },
    // 1. Nf3 Nf6
    { "rnbqkb1r/pppppppp/5n2/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 2 2", { "c2c4", "g2g3", "d2d4" }, { 45, 35, 20 }, 3 },
    // King's Indian Attack / Reti
    { "rnbqkb1r/pppppppp/5n2/8/8/5NP1/PPPPPP1P/RNBQKB1R b KQkq - 0 2", { "d7d5", "g7g6" }, { 60, 40 }, 2 },
    // Reti 3.Bg2
    { "rnbqkb1r/ppp1pppp/5n2/3p4/8/5NP1/PPPPPP1P/RNBQKB1R w KQkq d6 0 3", { "f1g2" }, { 100 }, 1 },
};
// clang-format on

#define RAW_BOOK_SIZE (sizeof(raw_book) / sizeof(raw_book[0]))

static struct BookKeyEntry book_table[1024];
static int book_count = 0;

static uint64_t book_prng_state = 0x853c49e6748fea9bULL;

static inline uint32_t book_rand(void) {
    book_prng_state ^= book_prng_state >> 12;
    book_prng_state ^= book_prng_state << 25;
    book_prng_state ^= book_prng_state >> 27;
    return (uint32_t)((book_prng_state * 0x2545F4914F6CDD1DULL) >> 32);
}

static int cmp_book_entry(const void *a, const void *b) {
    const struct BookKeyEntry *ea = (const struct BookKeyEntry *)a;
    const struct BookKeyEntry *eb = (const struct BookKeyEntry *)b;
    if (ea->key < eb->key)
        return -1;
    if (ea->key > eb->key)
        return 1;
    return 0;
}

void init_book(void) {
    book_count = 0;
    book_prng_state ^= get_time_ms();
    struct Board b;

    for (size_t i = 0; i < RAW_BOOK_SIZE; ++i) {
        if (!set_fen(&b, raw_book[i].fen))
            continue;
        uint64_t key = board_key(&b);

        for (int m = 0; m < raw_book[i].count; ++m) {
            move_t parsed = parse_move(&b, raw_book[i].moves[m]);
            if (parsed && book_count < 1024) {
                book_table[book_count].key = key;
                book_table[book_count].move = parsed;
                book_table[book_count].weight = raw_book[i].weights[m];
                book_count++;
            }
        }
    }

    qsort(book_table, book_count, sizeof(struct BookKeyEntry), cmp_book_entry);
}

move_t probe_book(const struct Board *b) {
    if (!option_own_book || book_count == 0)
        return 0;

    uint64_t key = board_key(b);
    struct BookKeyEntry target = {.key = key, .move = 0, .weight = 0};
    struct BookKeyEntry *match = (struct BookKeyEntry *)bsearch(
        &target, book_table, book_count, sizeof(struct BookKeyEntry), cmp_book_entry);

    if (!match)
        return 0;

    int first = (int)(match - book_table);
    while (first > 0 && book_table[first - 1].key == key)
        first--;

    int last = first;
    int total_weight = 0;
    while (last < book_count && book_table[last].key == key) {
        total_weight += book_table[last].weight;
        last++;
    }

    if (total_weight <= 0)
        return match->move;

    int r = (int)(book_rand() % (uint32_t)total_weight);
    int acc = 0;
    for (int i = first; i < last; ++i) {
        acc += book_table[i].weight;
        if (r < acc)
            return book_table[i].move;
    }

    return book_table[first].move;
}
