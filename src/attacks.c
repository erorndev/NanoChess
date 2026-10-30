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

// clang-format off
static const uint64_t bishop_magic_nums[64] = {
    0x0042100208204081ULL, 0x2010b08080828028ULL, 0x1008008112001000ULL, 0x0028208228280002ULL,
    0x21a2021008002080ULL, 0x0002021006480080ULL, 0x0049014110407080ULL, 0x8004820802020200ULL,
    0x0001042104140080ULL, 0x0000908408004041ULL, 0x8400c20401002004ULL, 0x0324c40400870820ULL,
    0x0c0404050400401aULL, 0x8060020802080080ULL, 0x0401020924424090ULL, 0x0004005246103000ULL,
    0x0060831042102902ULL, 0x041000202410c080ULL, 0x004840b0004600a1ULL, 0x0019840802004220ULL,
    0x0804000210220210ULL, 0x800140220300a000ULL, 0x0212000042100400ULL, 0x00010a0610410400ULL,
    0x0250084010421000ULL, 0x1008020120029200ULL, 0x0604010010810400ULL, 0x0002100808008020ULL,
    0x2002488004002000ULL, 0xa0901c4000880800ULL, 0x000d042281108800ULL, 0x8040404002010404ULL,
    0x0442021048206089ULL, 0x04208a2002510441ULL, 0x0003080200010408ULL, 0x8100020080080082ULL,
    0x8004040400001010ULL, 0x4a29084200010100ULL, 0x4c24080040120128ULL, 0x0014084200044128ULL,
    0xa20c88043040c284ULL, 0x08845410180104c4ULL, 0x02000c0044020800ULL, 0x000104202420a800ULL,
    0x2005400811400a08ULL, 0x0040068800c10882ULL, 0xa010094801215088ULL, 0x1010040100442030ULL,
    0xa001008210400800ULL, 0x0021008090088802ULL, 0x003a002908084420ULL, 0x2c23004108680000ULL,
    0x04440010203a0080ULL, 0x0046044868084001ULL, 0x2444051014030400ULL, 0x14481000c0810200ULL,
    0x1008840845042060ULL, 0x400104404c100848ULL, 0x0000000844140400ULL, 0x0800900004840401ULL,
    0x090802130810241aULL, 0x0440482434080210ULL, 0x0001085111080504ULL, 0x1048301012002020ULL,
};
static const uint64_t rook_magic_nums[64] = {
    0x0100102080004100ULL, 0x0040100020004000ULL, 0x2080081000200081ULL, 0x0480100080842801ULL,
    0x0080080080040002ULL, 0x0100020400080100ULL, 0x5280220031000080ULL, 0x0200008203284d04ULL,
    0x0048800040068021ULL, 0x0002400040201000ULL, 0x0086801000a00080ULL, 0x1000808008001000ULL,
    0x0240800402800801ULL, 0x0100800200040080ULL, 0x004a000200048108ULL, 0x2002000100805402ULL,
    0x42a0208000804000ULL, 0x2a00404000201001ULL, 0x0040410020010812ULL, 0x2010808010000801ULL,
    0x1010818004002800ULL, 0x0002008004000280ULL, 0x8000040082011008ULL, 0x2a001a0001054384ULL,
    0x8404400380022090ULL, 0xe0100040c0112000ULL, 0x0110088080102000ULL, 0x2408080080801000ULL,
    0x0000108500080100ULL, 0x0002000200041008ULL, 0x0000820400010890ULL, 0x0000004200010084ULL,
    0x0000400082800023ULL, 0x001000e003400240ULL, 0x1002001042002081ULL, 0x0030010125000810ULL,
    0x01494801010004b0ULL, 0x4010800400800200ULL, 0x1000023004008108ULL, 0x4040800040800100ULL,
    0x0184269040018001ULL, 0x4101020020820040ULL, 0x0288200100410011ULL, 0x001a0108c1920020ULL,
    0x0020080100650010ULL, 0x0002000904020010ULL, 0x0240010208040010ULL, 0x6001000880410012ULL,
    0x01010040802a0200ULL, 0x4520088040042880ULL, 0x4120010410244100ULL, 0x0040420020081200ULL,
    0x0000080100100500ULL, 0x0002000410080200ULL, 0x0026000401080200ULL, 0x0000086884010200ULL,
    0x0200528001250041ULL, 0x9204104009008221ULL, 0x0904088022001042ULL, 0x2100200410000901ULL,
    0x100200082090540eULL, 0x0226000130046842ULL, 0x0008008841021004ULL, 0x4200064021041482ULL,
};
// clang-format on


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
    bitboard_t *table = rook ? rook_table : bishop_table;
    const uint64_t *nums = rook ? rook_magic_nums : bishop_magic_nums;
    int offset = 0;

    for (int sq = 0; sq < 64; ++sq) {
        struct Magic *m = rook ? &rook_magics[sq] : &bishop_magics[sq];
        m->mask = rook ? mask_rook(sq) : mask_bishop(sq);
        m->shift = 64 - popcount64(m->mask);
        m->attacks = table + offset;
        m->magic = nums[sq];

        bitboard_t b = 0;
        do {
            m->attacks[(b * m->magic) >> m->shift] = ray_attacks(sq, b, rook);
            b = (b - m->mask) & m->mask;
        } while (b);

        offset += 1 << (64 - m->shift);
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
