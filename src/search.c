#include "search.h"
#include "attacks.h"
#include "evaluate.h"
#include "movegen.h"
#include "platform.h"
#include "tt.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
static const int see_val[7] = { 100, 320, 330, 500, 950, 20000, 0 };
// clang-format on

int option_threads = 1;
static atomic_bool search_stopped = false;
static int lmr_table[MAX_SEARCH_DEPTH][256];

struct SearchStack {
    int eval;
    move_t excluded;
    move_t move;
    enum Piece moved_piece;
    move_t pv[MAX_SEARCH_DEPTH + 4];
    int pv_len;
};

struct SearchContext {
    uint64_t start_time;
    uint64_t soft_limit;
    uint64_t hard_limit;
    uint64_t max_nodes;
    uint64_t nodes;
    uint64_t singular_exts;
    int seldepth;
    bool stopped;
    bool uci_output;
    struct SearchStack ss[MAX_SEARCH_DEPTH + 4];
};

struct Worker {
    int id;
    int max_depth;
    int root_depth;
    struct Board board;
    struct SearchContext ctx;
    int16_t history[COLOR_NB][64][64];
    int16_t conthist[12][64][12][64];
    move_t killers[MAX_SEARCH_DEPTH + 4][2];
    move_t countermoves[13][64];
    struct SearchResult result;
};

static struct Worker *workers = NULL;

void set_threads(int count) {
    if (count < 1)
        count = 1;
    if (count > MAX_THREADS)
        count = MAX_THREADS;
    option_threads = count;
    free(workers);
    workers = (struct Worker *)calloc((size_t)option_threads, sizeof(struct Worker));
}

void init_search(void) {
    for (int d = 1; d < MAX_SEARCH_DEPTH; ++d) {
        for (int m = 1; m < 256; ++m) {
            lmr_table[d][m] = (int)(0.75 + log((double)d) * log((double)m) / 2.25);
        }
    }
    if (!workers)
        set_threads(option_threads);
    else
        clear_search();
}

void clear_search(void) {
    if (!workers)
        return;
    for (int i = 0; i < option_threads; ++i) {
        memset(workers[i].history, 0, sizeof(workers[i].history));
        memset(workers[i].conthist, 0, sizeof(workers[i].conthist));
        memset(workers[i].killers, 0, sizeof(workers[i].killers));
        memset(workers[i].countermoves, 0, sizeof(workers[i].countermoves));
    }
}

int conthist_entry(enum Piece prev_pc, enum Square prev_to, enum Piece pc, enum Square to) {
    if (!workers || prev_pc >= NO_PIECE || prev_to >= SQ_NONE || pc >= NO_PIECE || to >= SQ_NONE)
        return 0;
    return workers[0].conthist[prev_pc][prev_to][pc][to];
}

uint64_t last_singular_extensions(void) {
    if (!workers)
        return 0;
    uint64_t sum = 0;
    for (int i = 0; i < option_threads; ++i)
        sum += workers[i].ctx.singular_exts;
    return sum;
}

static uint64_t total_search_nodes(void) {
    uint64_t sum = 0;
    for (int i = 0; i < option_threads; ++i)
        sum += workers[i].ctx.nodes;
    return sum;
}

static bitboard_t attackers_to(const struct Board *b, enum Square sq, bitboard_t occ) {
    bitboard_t bq =
        b->pieces[W_BISHOP] | b->pieces[B_BISHOP] | b->pieces[W_QUEEN] | b->pieces[B_QUEEN];
    bitboard_t rq = b->pieces[W_ROOK] | b->pieces[B_ROOK] | b->pieces[W_QUEEN] | b->pieces[B_QUEEN];
    return (pawn_attacks[BLACK][sq] & b->pieces[W_PAWN]) |
           (pawn_attacks[WHITE][sq] & b->pieces[B_PAWN]) |
           (knight_attacks[sq] & (b->pieces[W_KNIGHT] | b->pieces[B_KNIGHT])) |
           (king_attacks[sq] & (b->pieces[W_KING] | b->pieces[B_KING])) |
           (bishop_attacks(sq, occ) & bq) | (rook_attacks(sq, occ) & rq);
}

bool see(const struct Board *b, move_t m, int threshold) {
    enum Square from = move_from(m);
    enum Square to = move_to(m);
    uint16_t flags = move_flags(m);

    int victim = 0;
    if (flags == MOVE_EN_PASSANT)
        victim = see_val[PAWN];
    else if (b->mailbox[to] != NO_PIECE)
        victim = see_val[piece_type(b->mailbox[to])];

    enum PieceType prom_pt = move_promoted_type(m);
    if (prom_pt != NONE_PIECE_TYPE)
        victim += see_val[prom_pt] - see_val[PAWN];

    int balance = victim - threshold;
    if (balance < 0)
        return false;

    enum PieceType next_pt = (prom_pt != NONE_PIECE_TYPE) ? prom_pt : piece_type(b->mailbox[from]);
    balance -= see_val[next_pt];
    if (balance >= 0)
        return true;

    bitboard_t occ = b->all ^ (1ULL << from) ^ (1ULL << to);
    if (flags == MOVE_EN_PASSANT) {
        enum Square ep_cap = (enum Square)(b->side == WHITE ? to - 8 : to + 8);
        occ ^= 1ULL << ep_cap;
    }

    bitboard_t bq =
        b->pieces[W_BISHOP] | b->pieces[B_BISHOP] | b->pieces[W_QUEEN] | b->pieces[B_QUEEN];
    bitboard_t rq = b->pieces[W_ROOK] | b->pieces[B_ROOK] | b->pieces[W_QUEEN] | b->pieces[B_QUEEN];
    bitboard_t attackers = attackers_to(b, to, occ) & occ;

    enum Color stm = (enum Color)(b->side ^ 1);

    for (;;) {
        bitboard_t my_atks = attackers & b->occ[stm];
        if (!my_atks)
            break;

        enum PieceType pt = PAWN;
        for (; pt <= KING; ++pt) {
            if (my_atks & b->pieces[make_piece(stm, pt)])
                break;
        }

        stm = (enum Color)(stm ^ 1);
        balance = -balance - 1 - see_val[pt];

        bitboard_t piece_bb = my_atks & b->pieces[make_piece((enum Color)(stm ^ 1), pt)];
        occ ^= piece_bb & -(int64_t)piece_bb;

        if (pt == PAWN || pt == BISHOP || pt == QUEEN)
            attackers |= bishop_attacks(to, occ) & bq;
        if (pt == ROOK || pt == QUEEN)
            attackers |= rook_attacks(to, occ) & rq;
        attackers &= occ;

        if (balance >= 0) {
            if (pt == KING && (attackers & b->occ[stm]))
                stm = (enum Color)(stm ^ 1);
            break;
        }
    }

    return stm != b->side;
}

static inline void update_history_entry(int16_t *entry, int bonus) {
    if (bonus > 1600)
        bonus = 1600;
    if (bonus < -1600)
        bonus = -1600;
    *entry += (int16_t)(bonus - (*entry * abs(bonus)) / 16384);
}

static inline int get_conthist(const struct Worker *w, int ply, enum Piece pc, enum Square to) {
    if (pc >= NO_PIECE || to >= SQ_NONE)
        return 0;
    int score = 0;
    if (ply >= 1 && w->ctx.ss[ply - 1].move != 0 && w->ctx.ss[ply - 1].moved_piece < NO_PIECE) {
        enum Piece p1 = w->ctx.ss[ply - 1].moved_piece;
        enum Square t1 = move_to(w->ctx.ss[ply - 1].move);
        score += w->conthist[p1][t1][pc][to];
    }
    if (ply >= 2 && w->ctx.ss[ply - 2].move != 0 && w->ctx.ss[ply - 2].moved_piece < NO_PIECE) {
        enum Piece p2 = w->ctx.ss[ply - 2].moved_piece;
        enum Square t2 = move_to(w->ctx.ss[ply - 2].move);
        score += w->conthist[p2][t2][pc][to];
    }
    return score;
}

static inline void update_conthist(struct Worker *w, int ply, enum Piece pc, enum Square to,
                                   int bonus) {
    if (pc >= NO_PIECE || to >= SQ_NONE)
        return;
    if (ply >= 1 && w->ctx.ss[ply - 1].move != 0 && w->ctx.ss[ply - 1].moved_piece < NO_PIECE) {
        enum Piece p1 = w->ctx.ss[ply - 1].moved_piece;
        enum Square t1 = move_to(w->ctx.ss[ply - 1].move);
        update_history_entry(&w->conthist[p1][t1][pc][to], bonus);
    }
    if (ply >= 2 && w->ctx.ss[ply - 2].move != 0 && w->ctx.ss[ply - 2].moved_piece < NO_PIECE) {
        enum Piece p2 = w->ctx.ss[ply - 2].moved_piece;
        enum Square t2 = move_to(w->ctx.ss[ply - 2].move);
        update_history_entry(&w->conthist[p2][t2][pc][to], bonus);
    }
}

static void score_moves(const struct Worker *w, struct MoveList *list, move_t tt_move, int ply,
                        move_t prev_move) {
    const struct Board *b = &w->board;
    move_t k1 = w->killers[ply][0];
    move_t k2 = w->killers[ply][1];
    move_t cm = 0;
    if (prev_move != 0) {
        enum Square pto = move_to(prev_move);
        enum Piece pp = b->mailbox[pto];
        if (pp != NO_PIECE)
            cm = w->countermoves[pp][pto];
    }

    for (int i = 0; i < list->count; ++i) {
        move_t m = list->moves[i];
        if (m == tt_move) {
            list->scores[i] = 2000000;
        } else if (move_is_capture(m) || move_is_promotion(m)) {
            int victim = PAWN;
            if (move_flags(m) != MOVE_EN_PASSANT && b->mailbox[move_to(m)] != NO_PIECE)
                victim = piece_type(b->mailbox[move_to(m)]);
            int attacker = piece_type(b->mailbox[move_from(m)]);
            int mvv_lva = victim * 16 - attacker;
            if (move_is_promotion(m))
                mvv_lva += move_promoted_type(m) * 16;

            if (see(b, m, 0))
                list->scores[i] = 1000000 + mvv_lva;
            else
                list->scores[i] = -100000 + mvv_lva;
        } else if (m == k1) {
            list->scores[i] = 900000;
        } else if (m == k2) {
            list->scores[i] = 800000;
        } else if (m == cm) {
            list->scores[i] = 700000;
        } else {
            enum Piece pc = b->mailbox[move_from(m)];
            list->scores[i] = (int)w->history[b->side][move_from(m)][move_to(m)] +
                              get_conthist(w, ply, pc, move_to(m));
        }
    }
}

static inline move_t pick_move(struct MoveList *list, int idx) {
    int best_idx = idx;
    int best_score = list->scores[idx];
    for (int i = idx + 1; i < list->count; ++i) {
        if (list->scores[i] > best_score) {
            best_score = list->scores[i];
            best_idx = i;
        }
    }
    if (best_idx != idx) {
        move_t tm = list->moves[idx];
        int ts = list->scores[idx];
        list->moves[idx] = list->moves[best_idx];
        list->scores[idx] = list->scores[best_idx];
        list->moves[best_idx] = tm;
        list->scores[best_idx] = ts;
    }
    return list->moves[idx];
}

static inline bool check_stop(struct Worker *w) {
    struct SearchContext *ctx = &w->ctx;
    if (ctx->stopped)
        return true;
    if ((ctx->nodes & 1023) == 0) {
        if (atomic_load_explicit(&search_stopped, memory_order_relaxed)) {
            ctx->stopped = true;
            return true;
        }
        if (w->id == 0) {
            if (ctx->hard_limit > 0 && (get_time_ms() - ctx->start_time) >= ctx->hard_limit) {
                ctx->stopped = true;
                atomic_store_explicit(&search_stopped, true, memory_order_relaxed);
                return true;
            }
            if (ctx->max_nodes > 0 && total_search_nodes() >= ctx->max_nodes) {
                ctx->stopped = true;
                atomic_store_explicit(&search_stopped, true, memory_order_relaxed);
                return true;
            }
        }
    }
    return false;
}

static inline bool has_non_pawns(const struct Board *b, enum Color c) {
    int base = c * 6;
    return (b->pieces[base + KNIGHT] | b->pieces[base + BISHOP] | b->pieces[base + ROOK] |
            b->pieces[base + QUEEN]) != 0;
}

static int qsearch(struct Worker *w, int alpha, int beta, int ply) {
    struct Board *b = &w->board;
    struct SearchContext *ctx = &w->ctx;
    ctx->ss[ply].pv_len = 0;
    if (check_stop(w))
        return 0;

    if (ply > ctx->seldepth)
        ctx->seldepth = ply;

    if (ply > 0 && (is_repetition(b) || b->history[b->ply].halfmove >= 100))
        return 0;

    if (ply >= MAX_SEARCH_DEPTH)
        return evaluate(b);

    uint64_t key = board_key(b);
    struct TTEntry tte;
    move_t tt_move = 0;
    bool tt_hit = probe_tt(key, &tte);
    if (tt_hit) {
        tt_move = tte.move;
        int tt_score = score_from_tt(tte.score, ply);
        uint8_t flag = tte.flag_age & 3;
        if (flag == TT_EXACT || (flag == TT_LOWER && tt_score >= beta) ||
            (flag == TT_UPPER && tt_score <= alpha)) {
            return tt_score;
        }
    }

    bool in_chk = in_check(b);
    int best_score = -INF;
    move_t best_move = 0;
    int stand_pat = -INF;
    int static_eval = -INF;

    if (!in_chk) {
        static_eval = tt_hit ? tte.eval : evaluate(b);
        stand_pat = static_eval;
        if (stand_pat >= beta) {
            if (!tt_hit)
                store_tt(key, 0, ply, stand_pat, static_eval, TT_LOWER, 0);
            return stand_pat;
        }
        if (stand_pat > alpha)
            alpha = stand_pat;
        best_score = stand_pat;
    }

    struct MoveList list;
    if (in_chk)
        generate_moves(b, &list);
    else
        generate_captures(b, &list);
    score_moves(w, &list, tt_move, ply, ply >= 1 ? ctx->ss[ply - 1].move : 0);

    int legal_moves = 0;

    for (int i = 0; i < list.count; ++i) {
        move_t m = pick_move(&list, i);

        if (!in_chk && !move_is_promotion(m)) {
            int gain = see_val[PAWN];
            if (move_flags(m) != MOVE_EN_PASSANT && b->mailbox[move_to(m)] != NO_PIECE)
                gain = see_val[piece_type(b->mailbox[move_to(m)])];
            if (stand_pat + gain + 200 < alpha)
                continue;
            if (list.scores[i] < 0 || (m == tt_move && !see(b, m, 0)))
                continue;
        }

        enum Piece moved_pc = b->mailbox[move_from(m)];
        if (!do_move(b, m))
            continue;
        ctx->ss[ply].move = m;
        ctx->ss[ply].moved_piece = moved_pc;
        legal_moves++;
        ctx->nodes++;

        int score = -qsearch(w, -beta, -alpha, ply + 1);
        undo_move(b, m);

        if (ctx->stopped)
            return 0;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
                best_move = m;
                if (score >= beta)
                    break;
            }
        }
    }

    if (in_chk && legal_moves == 0)
        best_score = -MATE_VAL + ply;

    store_tt(key, 0, ply, best_score, static_eval, best_score >= beta ? TT_LOWER : TT_UPPER,
             best_move);

    return best_score;
}

static int negamax(struct Worker *w, int depth, int alpha, int beta, int ply, move_t prev_move,
                   bool cut_node) {
    struct Board *b = &w->board;
    struct SearchContext *ctx = &w->ctx;
    move_t excluded = ctx->ss[ply].excluded;
    if (!excluded)
        ctx->ss[ply].pv_len = 0;
    if (ply + 1 < MAX_SEARCH_DEPTH + 4)
        ctx->ss[ply + 1].excluded = 0;
    if (check_stop(w))
        return 0;

    if (ply > ctx->seldepth)
        ctx->seldepth = ply;

    bool root_node = (ply == 0);
    bool pv_node = (beta - alpha > 1);

    if (!root_node) {
        if (is_repetition(b) || b->history[b->ply].halfmove >= 100)
            return 0;

        if (ply >= MAX_SEARCH_DEPTH)
            return evaluate(b);

        if (alpha < -MATE_VAL + ply)
            alpha = -MATE_VAL + ply;
        if (beta > MATE_VAL - ply - 1)
            beta = MATE_VAL - ply - 1;
        if (alpha >= beta)
            return alpha;
    }

    bool in_chk = in_check(b);
    if (in_chk && !excluded)
        depth++;

    if (depth <= 0)
        return qsearch(w, alpha, beta, ply);

    uint64_t key = board_key(b);
    struct TTEntry tte;
    move_t tt_move = 0;
    int tt_score = 0;
    uint8_t tt_flag = TT_NONE;
    bool tt_hit = !excluded && probe_tt(key, &tte);

    if (tt_hit) {
        tt_move = tte.move;
        tt_score = score_from_tt(tte.score, ply);
        tt_flag = tte.flag_age & 3;
        if (!pv_node && tte.depth >= depth) {
            if (tt_flag == TT_EXACT || (tt_flag == TT_LOWER && tt_score >= beta) ||
                (tt_flag == TT_UPPER && tt_score <= alpha)) {
                return tt_score;
            }
        }
    }

    int static_eval =
        in_chk ? -INF : (excluded ? ctx->ss[ply].eval : (tt_hit ? tte.eval : evaluate(b)));
    if (!excluded)
        ctx->ss[ply].eval = static_eval;
    bool improving = !in_chk && ply >= 2 && static_eval > ctx->ss[ply - 2].eval;

    if (!pv_node && !in_chk && !excluded) {
        // Reverse Futility Pruning
        if (depth <= 7) {
            int margin = 80 * depth - (improving ? 40 : 0);
            if (static_eval - margin >= beta && abs(beta) < MATE_THRESHOLD &&
                abs(static_eval) < MATE_THRESHOLD)
                return static_eval;
        }

        // Null Move Pruning
        if (depth >= 3 && prev_move != 0 && static_eval >= beta && has_non_pawns(b, b->side)) {
            int r =
                3 + depth / 4 + ((static_eval - beta) / 200 > 3 ? 3 : (static_eval - beta) / 200);
            ctx->ss[ply].move = 0;
            ctx->ss[ply].moved_piece = NO_PIECE;
            do_null_move(b);
            ctx->nodes++;
            int score = -negamax(w, depth - r, -beta, -beta + 1, ply + 1, 0, !cut_node);
            undo_null_move(b);
            if (ctx->stopped)
                return 0;
            if (score >= beta)
                return score >= MATE_THRESHOLD ? beta : score;
        }
    }

    // Internal Iterative Reductions (IIR)
    if (depth >= 4 && tt_move == 0 && !excluded)
        depth--;

    struct MoveList list;
    generate_moves(b, &list);
    score_moves(w, &list, tt_move, ply, prev_move);

    move_t quiets_tried[64];
    int quiet_count = 0;

    int best_score = -INF;
    move_t best_move = 0;
    int orig_alpha = alpha;
    int legal_moves = 0;

    for (int i = 0; i < list.count; ++i) {
        move_t m = pick_move(&list, i);
        if (m == excluded)
            continue;

        int extension = 0;
        if (!root_node && !excluded && depth >= 8 && ply < w->root_depth * 2 && m == tt_move &&
            abs(tt_score) < MATE_THRESHOLD && tte.depth >= depth - 3 &&
            (tt_flag == TT_EXACT || tt_flag == TT_LOWER)) {
            int s_beta = tt_score - 2 * depth;
            ctx->ss[ply].excluded = m;
            int s_score =
                negamax(w, (depth - 1) / 2, s_beta - 1, s_beta, ply, prev_move, cut_node);
            ctx->ss[ply].excluded = 0;
            if (ctx->stopped)
                return 0;
            if (s_score < s_beta) {
                extension = (!pv_node && s_score < s_beta - 25) ? 2 : 1;
                ctx->singular_exts++;
            } else if (s_beta >= beta) {
                return s_beta;
            }
        }

        enum Piece moved_pc = b->mailbox[move_from(m)];
        bool is_tactical = move_is_capture(m) || move_is_promotion(m);
        int hist_score = !is_tactical ? ((int)w->history[b->side][move_from(m)][move_to(m)] +
                                         get_conthist(w, ply, moved_pc, move_to(m)))
                                      : 0;
        bool can_prune = !root_node && !pv_node && !in_chk && legal_moves >= 1 && !is_tactical &&
                         best_score > -MATE_THRESHOLD;

        // History Pruning
        if (can_prune && depth <= 3 && hist_score < -4000 * depth)
            continue;

        bool see_ok = !can_prune || depth > 4 || see(b, m, -80 * depth);

        if (!do_move(b, m))
            continue;

        bool gives_check = in_check(b);

        if (can_prune && !gives_check) {
            if (depth <= 3 && legal_moves >= (4 + depth * depth) / (2 - improving)) {
                undo_move(b, m);
                continue;
            }
            if (depth <= 4 && static_eval + 150 + 150 * depth <= alpha) {
                undo_move(b, m);
                continue;
            }
            if (!see_ok) {
                undo_move(b, m);
                continue;
            }
        }

        ctx->ss[ply].move = m;
        ctx->ss[ply].moved_piece = moved_pc;
        legal_moves++;
        ctx->nodes++;

        int new_depth = depth - 1 + extension;
        int score;
        if (legal_moves == 1) {
            score = -negamax(w, new_depth, -beta, -alpha, ply + 1, m, false);
        } else {
            int r = 0;
            if (depth >= 3 && legal_moves > (pv_node ? 4 : 2) && !is_tactical && !gives_check) {
                r = lmr_table[depth < MAX_SEARCH_DEPTH ? depth : MAX_SEARCH_DEPTH - 1]
                             [legal_moves < 256 ? legal_moves : 255];
                if (!pv_node)
                    r++;
                if (!improving)
                    r++;
                if (list.scores[i] >= 700000)
                    r--;
                r -= hist_score / 8192;
                if (r < 0)
                    r = 0;
                if (new_depth - r < 1)
                    r = new_depth - 1;
            }

            score = -negamax(w, new_depth - r, -alpha - 1, -alpha, ply + 1, m, true);

            if (r > 0 && score > alpha)
                score = -negamax(w, new_depth, -alpha - 1, -alpha, ply + 1, m, !cut_node);

            if (pv_node && score > alpha && (root_node || score < beta))
                score = -negamax(w, new_depth, -beta, -alpha, ply + 1, m, false);
        }

        undo_move(b, m);

        if (ctx->stopped)
            return 0;

        if (score > best_score) {
            best_score = score;
            best_move = m;

            if (score > alpha) {
                alpha = score;

                if (pv_node) {
                    ctx->ss[ply].pv[0] = m;
                    int child_len = ctx->ss[ply + 1].pv_len;
                    for (int j = 0; j < child_len; ++j)
                        ctx->ss[ply].pv[j + 1] = ctx->ss[ply + 1].pv[j];
                    ctx->ss[ply].pv_len = child_len + 1;
                }

                if (score >= beta) {
                    if (!is_tactical) {
                        if (w->killers[ply][0] != m) {
                            w->killers[ply][1] = w->killers[ply][0];
                            w->killers[ply][0] = m;
                        }
                        if (prev_move != 0) {
                            enum Square pto = move_to(prev_move);
                            enum Piece pp = b->mailbox[pto];
                            if (pp != NO_PIECE)
                                w->countermoves[pp][pto] = m;
                        }
                        int bonus = 16 * depth * depth;
                        update_history_entry(&w->history[b->side][move_from(m)][move_to(m)], bonus);
                        update_conthist(w, ply, moved_pc, move_to(m), bonus);
                        for (int q = 0; q < quiet_count; ++q) {
                            move_t qm = quiets_tried[q];
                            enum Piece qpc = b->mailbox[move_from(qm)];
                            update_history_entry(&w->history[b->side][move_from(qm)][move_to(qm)],
                                                 -bonus);
                            update_conthist(w, ply, qpc, move_to(qm), -bonus);
                        }
                    }
                    break;
                }
            }
        }

        if (!is_tactical && quiet_count < 64)
            quiets_tried[quiet_count++] = m;
    }

    if (legal_moves == 0) {
        if (excluded)
            return alpha;
        return in_chk ? (-MATE_VAL + ply) : 0;
    }

    if (!excluded) {
        enum TTFlag flag = (best_score >= beta)        ? TT_LOWER
                           : (best_score > orig_alpha) ? TT_EXACT
                                                       : TT_UPPER;
        store_tt(key, depth, ply, best_score, static_eval, flag, best_move);
    }

    return best_score;
}

static int worker_entry(void *arg) {
    struct Worker *w = (struct Worker *)arg;
    struct SearchContext *ctx = &w->ctx;
    int prev_score = 0;
    move_t last_best_move = 0;
    int stable_count = 0;

    for (int d = 1; d <= w->max_depth; ++d) {
        if (atomic_load_explicit(&search_stopped, memory_order_relaxed))
            break;

        int search_d = d;
        if (w->id > 0 && d > 1 && ((d + w->id) & 1) == 0 && d < w->max_depth)
            search_d = d + 1;
        w->root_depth = search_d;

        ctx->seldepth = 0;
        int alpha = -INF;
        int beta = INF;
        int delta = 20 + (w->id & 7) * 3;

        if (search_d >= 5) {
            alpha = prev_score - delta > -INF ? prev_score - delta : -INF;
            beta = prev_score + delta < INF ? prev_score + delta : INF;
        }

        int score = 0;
        for (;;) {
            score = negamax(w, search_d, alpha, beta, 0, 0, false);
            if (ctx->stopped || atomic_load_explicit(&search_stopped, memory_order_relaxed))
                break;

            if (score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = score - delta > -INF ? score - delta : -INF;
                delta += delta / 2 + 10;
            } else if (score >= beta) {
                beta = score + delta < INF ? score + delta : INF;
                delta += delta / 2 + 10;
            } else {
                break;
            }
        }

        if ((ctx->stopped || atomic_load_explicit(&search_stopped, memory_order_relaxed)) && d > 1)
            break;

        move_t current_best_move = ctx->ss[0].pv_len > 0 ? ctx->ss[0].pv[0] : 0;
        if (current_best_move == last_best_move)
            stable_count++;
        else
            stable_count = 0;
        last_best_move = current_best_move;

        int score_drop = prev_score - score;
        prev_score = score;
        w->result.score = score;
        w->result.depth = search_d;
        w->result.nodes = ctx->nodes;
        if (current_best_move)
            w->result.best_move = current_best_move;

        if (w->id == 0 && ctx->uci_output) {
            uint64_t elapsed = get_time_ms() - ctx->start_time;
            uint64_t all_nodes = total_search_nodes();
            uint64_t nps = elapsed > 0 ? (all_nodes * 1000ULL) / elapsed : all_nodes;

            printf("info depth %d seldepth %d ", d, ctx->seldepth);
            if (abs(score) > MATE_THRESHOLD) {
                int mate_ply = MATE_VAL - abs(score);
                int mate_moves = (mate_ply + 1) / 2;
                if (score < 0)
                    mate_moves = -mate_moves;
                printf("score mate %d ", mate_moves);
            } else {
                printf("score cp %d ", score);
            }
            printf("nodes %llu nps %llu time %llu pv", (unsigned long long)all_nodes,
                   (unsigned long long)nps, (unsigned long long)elapsed);
            for (int i = 0; i < ctx->ss[0].pv_len; ++i) {
                char mbuf[6];
                move_to_str(ctx->ss[0].pv[i], mbuf);
                printf(" %s", mbuf);
            }
            printf("\n");
            fflush(stdout);
        }

        if (w->id == 0 && ctx->soft_limit > 0) {
            uint64_t elapsed = get_time_ms() - ctx->start_time;
            uint64_t target_soft = ctx->soft_limit;
            if (d >= 6 && stable_count >= 3)
                target_soft = (target_soft * 65) / 100;
            else if (d >= 6 && score_drop >= 40 && ctx->hard_limit > 0) {
                uint64_t panic_soft = (target_soft * 150) / 100;
                target_soft = panic_soft < ctx->hard_limit ? panic_soft : ctx->hard_limit;
            }
            if (elapsed >= target_soft)
                break;
        }
    }

    if (w->id == 0)
        atomic_store_explicit(&search_stopped, true, memory_order_relaxed);

    return 0;
}

struct SearchResult search_position(struct Board *b, const struct SearchLimits *limits) {
    if (!workers)
        set_threads(option_threads);

    uint64_t start_time = get_time_ms();
    int max_depth = limits->depth > 0 ? limits->depth : MAX_SEARCH_DEPTH - 1;
    if (max_depth >= MAX_SEARCH_DEPTH)
        max_depth = MAX_SEARCH_DEPTH - 1;

    step_tt_age();
    atomic_store_explicit(&search_stopped, false, memory_order_relaxed);

    struct MoveList root_legal;
    generate_legal(b, &root_legal);
    move_t fallback_move = root_legal.count > 0 ? root_legal.moves[0] : 0;
    if (root_legal.count <= 1) {
        if (limits->uci_output && fallback_move) {
            char mbuf[6];
            move_to_str(fallback_move, mbuf);
            printf("info depth 1 score cp 0 nodes 1 nps 1 pv %s\n", mbuf);
            fflush(stdout);
        }
        return (struct SearchResult){fallback_move, 0, 1, 1};
    }

    for (int i = 0; i < option_threads; ++i) {
        workers[i].id = i;
        workers[i].max_depth = max_depth;
        workers[i].board = *b;
        memset(&workers[i].ctx, 0, sizeof(struct SearchContext));
        workers[i].ctx.start_time = start_time;
        workers[i].ctx.soft_limit = limits->soft_time;
        workers[i].ctx.hard_limit = limits->hard_time;
        workers[i].ctx.max_nodes = limits->max_nodes;
        workers[i].ctx.uci_output = (i == 0) && limits->uci_output;
        workers[i].result = (struct SearchResult){fallback_move, 0, 0, 0};
    }

    thread_t threads[MAX_THREADS];
    for (int i = 1; i < option_threads; ++i)
        thread_create(&threads[i], worker_entry, &workers[i]);

    worker_entry(&workers[0]);

    atomic_store_explicit(&search_stopped, true, memory_order_relaxed);
    for (int i = 1; i < option_threads; ++i)
        thread_join(threads[i]);

    struct SearchResult res = workers[0].result;
    for (int i = 1; i < option_threads; ++i) {
        if (workers[i].result.best_move != 0 &&
            (workers[i].result.depth > res.depth ||
             (workers[i].result.depth == res.depth && workers[i].result.score > res.score))) {
            res = workers[i].result;
        }
    }
    res.nodes = total_search_nodes();
    return res;
}
