#include "attacks.h"
#include "platform.h"

#include <string.h>

bitboard_t pawn_attacks[COLOR_NB][64];
bitboard_t knight_attacks[64];
bitboard_t king_attacks[64];
struct Magic bishop_magics[64];
struct Magic rook_magics[64];

static bitboard_t bishop_table[5248];
static bitboard_t rook_table[102400];

static uint64_t magic_rng = 0x123456789ABCDEFULL;

static inline uint64_t rand64_sparse(void) {
    uint64_t r[3];
    for (int i = 0; i < 3; ++i) {
        magic_rng ^= magic_rng >> 12;
        magic_rng ^= magic_rng << 25;
        magic_rng ^= magic_rng >> 27;
        r[i] = magic_rng * 0x2545F4914F6CDD1DULL;
    }
    return r[0] & r[1] & r[2];
}

static bitboard_t mask_bishop(int sq) {
    bitboard_t res = 0;
    int sr = sq >> 3, sf = sq & 7;
    const int dr[4] = {1, 1, -1, -1};
    const int df[4] = {1, -1, 1, -1};
    for (int d = 0; d < 4; ++d) {
        int r = sr + dr[d], f = sf + df[d];
        while (r > 0 && r < 7 && f > 0 && f < 7) {
            res |= 1ULL << ((r << 3) | f);
            r += dr[d];
            f += df[d];
        }
    }
    return res;
}

static bitboard_t mask_rook(int sq) {
    bitboard_t res = 0;
    int sr = sq >> 3, sf = sq & 7;
    for (int r = sr + 1; r < 7; ++r)
        res |= 1ULL << ((r << 3) | sf);
    for (int r = sr - 1; r > 0; --r)
        res |= 1ULL << ((r << 3) | sf);
    for (int f = sf + 1; f < 7; ++f)
        res |= 1ULL << ((sr << 3) | f);
    for (int f = sf - 1; f > 0; --f)
        res |= 1ULL << ((sr << 3) | f);
    return res;
}

static bitboard_t ray_attacks(int sq, bitboard_t occ, bool rook) {
    bitboard_t res = 0;
    int sr = sq >> 3, sf = sq & 7;
    const int b_dr[4] = {1, 1, -1, -1}, b_df[4] = {1, -1, 1, -1};
    const int r_dr[4] = {1, -1, 0, 0}, r_df[4] = {0, 0, 1, -1};
    const int *dr = rook ? r_dr : b_dr;
    const int *df = rook ? r_df : b_df;

    for (int d = 0; d < 4; ++d) {
        int r = sr + dr[d], f = sf + df[d];
        while (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
            bitboard_t bit = 1ULL << ((r << 3) | f);
            res |= bit;
            if (occ & bit)
                break;
            r += dr[d];
            f += df[d];
        }
    }
    return res;
}

static void init_slider(bool rook) {
    bitboard_t sub_occ[4096];
    bitboard_t sub_atk[4096];
    uint32_t used[4096] = {0};
    uint32_t epoch = 0;
    bitboard_t *table = rook ? rook_table : bishop_table;
    int offset = 0;

    for (int sq = 0; sq < 64; ++sq) {
        struct Magic *m = rook ? &rook_magics[sq] : &bishop_magics[sq];
        m->mask = rook ? mask_rook(sq) : mask_bishop(sq);
        int bits = popcount64(m->mask);
        m->shift = 64 - bits;
        m->attacks = table + offset;

        int count = 1 << bits;
        bitboard_t b = 0;
        for (int i = 0; i < count; ++i) {
            sub_occ[i] = b;
            sub_atk[i] = ray_attacks(sq, b, rook);
            b = (b - m->mask) & m->mask;
        }

        for (;;) {
            uint64_t magic = rand64_sparse();
            if (popcount64((m->mask * magic) & 0xFF00000000000000ULL) < 6)
                continue;

            ++epoch;
            bool ok = true;
            for (int i = 0; i < count; ++i) {
                uint32_t idx = (uint32_t)((sub_occ[i] * magic) >> m->shift);
                if (used[idx] != epoch) {
                    used[idx] = epoch;
                    m->attacks[idx] = sub_atk[i];
                } else if (m->attacks[idx] != sub_atk[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                m->magic = magic;
                break;
            }
        }
        offset += count;
    }
}

void init_attacks(void) {
    const int n_dr[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    const int n_df[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    const int k_dr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int k_df[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int sq = 0; sq < 64; ++sq) {
        int r = sq >> 3, f = sq & 7;

        bitboard_t wp = 0, bp = 0, n = 0, k = 0;
        if (r < 7) {
            if (f > 0)
                wp |= 1ULL << (((r + 1) << 3) | (f - 1));
            if (f < 7)
                wp |= 1ULL << (((r + 1) << 3) | (f + 1));
        }
        if (r > 0) {
            if (f > 0)
                bp |= 1ULL << (((r - 1) << 3) | (f - 1));
            if (f < 7)
                bp |= 1ULL << (((r - 1) << 3) | (f + 1));
        }
        pawn_attacks[WHITE][sq] = wp;
        pawn_attacks[BLACK][sq] = bp;

        for (int i = 0; i < 8; ++i) {
            int nr = r + n_dr[i], nf = f + n_df[i];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                n |= 1ULL << ((nr << 3) | nf);

            int kr = r + k_dr[i], kf = f + k_df[i];
            if (kr >= 0 && kr <= 7 && kf >= 0 && kf <= 7)
                k |= 1ULL << ((kr << 3) | kf);
        }
        knight_attacks[sq] = n;
        king_attacks[sq] = k;
    }

    init_slider(false);
    init_slider(true);
}
