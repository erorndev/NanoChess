#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t bitboard_t;

enum Color { WHITE = 0, BLACK = 1, COLOR_NB = 2 };

enum PieceType {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE_PIECE_TYPE = 6
};

// clang-format off
enum Square {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE = 64
};

enum Piece {
    W_PAWN = 0, W_KNIGHT = 1, W_BISHOP = 2, W_ROOK = 3, W_QUEEN = 4, W_KING = 5,
    B_PAWN = 6, B_KNIGHT = 7, B_BISHOP = 8, B_ROOK = 9, B_QUEEN = 10, B_KING = 11,
    NO_PIECE = 12
};
// clang-format on

enum File { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };

enum CastlingRights {
    CASTLE_NONE = 0,
    CASTLE_WK = 1 << 0,
    CASTLE_WQ = 1 << 1,
    CASTLE_BK = 1 << 2,
    CASTLE_BQ = 1 << 3,
    CASTLE_WHITE = CASTLE_WK | CASTLE_WQ,
    CASTLE_BLACK = CASTLE_BK | CASTLE_BQ,
    CASTLE_ALL = CASTLE_WHITE | CASTLE_BLACK
};

enum MoveFlag {
    MOVE_QUIET = 0 << 12,
    MOVE_DOUBLE_PAWN = 1 << 12,
    MOVE_CASTLE = 2 << 12,
    MOVE_EN_PASSANT = 3 << 12,
    MOVE_CAPTURE = 4 << 12,
    MOVE_PROM_N = 8 << 12,
    MOVE_PROM_B = 9 << 12,
    MOVE_PROM_R = 10 << 12,
    MOVE_PROM_Q = 11 << 12,
    MOVE_PROM_CAP_N = 12 << 12,
    MOVE_PROM_CAP_B = 13 << 12,
    MOVE_PROM_CAP_R = 14 << 12,
    MOVE_PROM_CAP_Q = 15 << 12
};

typedef uint16_t move_t;

[[nodiscard]] static inline move_t make_move(enum Square from, enum Square to, uint16_t flags) {
    return (move_t)((from & 0x3F) | ((to & 0x3F) << 6) | flags);
}

[[nodiscard]] static inline enum Square move_from(move_t m) {
    return (enum Square)(m & 0x3F);
}

[[nodiscard]] static inline enum Square move_to(move_t m) {
    return (enum Square)((m >> 6) & 0x3F);
}

[[nodiscard]] static inline uint16_t move_flags(move_t m) {
    return (uint16_t)(m & 0xF000);
}

[[nodiscard]] static inline bool move_is_capture(move_t m) {
    return (m & 0x4000) != 0;
}

[[nodiscard]] static inline bool move_is_promotion(move_t m) {
    return (m & 0x8000) != 0;
}

[[nodiscard]] static inline enum PieceType move_promoted_type(move_t m) {
    if (!move_is_promotion(m))
        return NONE_PIECE_TYPE;
    return (enum PieceType)(((m >> 12) & 0x3) + KNIGHT);
}

[[nodiscard]] static inline enum Square make_square(enum File f, enum Rank r) {
    return (enum Square)((r << 3) | f);
}

[[nodiscard]] static inline enum File square_file(enum Square sq) {
    return (enum File)(sq & 7);
}

[[nodiscard]] static inline enum Rank square_rank(enum Square sq) {
    return (enum Rank)(sq >> 3);
}

[[nodiscard]] static inline enum Piece make_piece(enum Color c, enum PieceType pt) {
    return (enum Piece)((c * 6) + pt);
}

[[nodiscard]] static inline enum PieceType piece_type(enum Piece p) {
    return (enum PieceType)(p % 6);
}

[[nodiscard]] static inline enum Color piece_color(enum Piece p) {
    return (enum Color)(p / 6);
}
