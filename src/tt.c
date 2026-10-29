#include "tt.h"

#include <stdlib.h>
#include <string.h>

#define MATE_THRESHOLD 30000

static struct TTEntry *tt_table = NULL;
static size_t tt_mask = 0;
static uint8_t tt_age = 0;

void init_tt(size_t mb) {
    if (mb < 1)
        mb = 1;
    if (mb > 1024)
        mb = 1024;

    size_t bytes = mb * 1024ULL * 1024ULL;
    size_t count = bytes / sizeof(struct TTEntry);

    size_t pow2 = 1;
    while ((pow2 << 1) <= count)
        pow2 <<= 1;

    free(tt_table);
    tt_table = (struct TTEntry *)calloc(pow2, sizeof(struct TTEntry));
    tt_mask = pow2 - 1;
    tt_age = 0;
}

void free_tt(void) {
    free(tt_table);
    tt_table = NULL;
    tt_mask = 0;
}

void clear_tt(void) {
    if (tt_table)
        memset(tt_table, 0, (tt_mask + 1) * sizeof(struct TTEntry));
    tt_age = 0;
}

void step_tt_age(void) {
    tt_age = (tt_age + 1) & 0x3F;
}

int score_to_tt(int score, int ply) {
    if (score > MATE_THRESHOLD)
        return score + ply;
    if (score < -MATE_THRESHOLD)
        return score - ply;
    return score;
}

int score_from_tt(int score, int ply) {
    if (score > MATE_THRESHOLD)
        return score - ply;
    if (score < -MATE_THRESHOLD)
        return score + ply;
    return score;
}

static inline uint64_t pack_tt_data(const struct TTEntry *e) {
    uint64_t d;
    memcpy(&d, &e->move, sizeof(uint64_t));
    return d;
}

bool probe_tt(uint64_t key, struct TTEntry *out) {
    if (!tt_table)
        return false;
    struct TTEntry e = tt_table[key & tt_mask];
    uint64_t data = pack_tt_data(&e);
    if ((e.flag_age & 3) != TT_NONE && (e.key ^ data) == key) {
        e.key = key;
        *out = e;
        return true;
    }
    return false;
}

void store_tt(uint64_t key, int depth, int ply, int score, int eval, enum TTFlag flag,
              move_t move) {
    if (!tt_table)
        return;

    struct TTEntry *slot = &tt_table[key & tt_mask];
    struct TTEntry cur = *slot;
    uint64_t cur_data = pack_tt_data(&cur);
    bool same_key = ((cur.key ^ cur_data) == key);
    uint8_t slot_flag = cur.flag_age & 3;
    uint8_t slot_age = cur.flag_age >> 2;

    if (slot_flag == TT_NONE || !same_key || slot_age != tt_age || flag == TT_EXACT ||
        depth + 2 >= cur.depth) {
        if (same_key && move == 0)
            move = cur.move;

        struct TTEntry next = {
            .key = 0,
            .move = move,
            .score = (int16_t)score_to_tt(score, ply),
            .eval = (int16_t)eval,
            .depth = (uint8_t)depth,
            .flag_age = (uint8_t)((tt_age << 2) | (flag & 3)),
        };
        next.key = key ^ pack_tt_data(&next);
        *slot = next;
    }
}
