"""Find evaluation terms NanoChess is missing, using Stockfish 11 as the judge.

For every NanoChess move in a set of PGNs, Stockfish scores its own best move and
NanoChess's move from the same position at the same depth. Moves that lose a lot
are blunders; positions where the two engines disagree are misjudgments. Each flagged position is classified with simple
board detectors and with Stockfish 11's per-term static eval (its `eval` command),
and the report shows which terms are unusually large in the positions we get wrong.

Usage:
    python tools/find_eval_gaps.py --sf <stockfish11.exe> [--pgn "build/ladder_*.pgn"]

Writes build/eval_gaps.md, build/eval_gaps.json and build/eval_gaps.epd.
"""

import argparse
import glob
import json
import re
import statistics
import subprocess
import threading
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor

import chess
import chess.pgn

VALUES = {chess.PAWN: 1, chess.KNIGHT: 3, chess.BISHOP: 3, chess.ROOK: 5, chess.QUEEN: 9, chess.KING: 0}
SF_NPM = {chess.KNIGHT: 781, chess.BISHOP: 825, chess.ROOK: 1276, chess.QUEEN: 2538}
TERMS = ["Material", "Imbalance", "Pawns", "Knights", "Bishops", "Rooks", "Queens",
         "Mobility", "King safety", "Threats", "Passed", "Space", "Initiative"]
HINTS = {
    "Threats": "threat terms: hanging pieces, piece attacked by lesser piece, pawn push threats",
    "King safety": "king safety: safe checks, weak king-ring squares, shelter/storm",
    "Passed": "passed pawns: safe path, block square control, king distances",
    "Space": "space term (missing entirely)",
    "Initiative": "endgame scaling / complexity (drawish material, pawn count)",
    "Imbalance": "material imbalance (bishop pair, piece combinations)",
    "Mobility": "mobility curves / mobility area",
    "Pawns": "pawn structure: backward, candidate passers, connected pawns",
    "Knights": "knight terms: outposts, minor behind pawn, distance to king",
    "Bishops": "bishop terms: bad bishop vs own pawns, long diagonal",
    "Rooks": "rook terms: open files, trapped rook",
    "Queens": "queen terms: pinned or exposed queen",
    "Material": "material values / piece-square tables",
}

COMMENT_RE = re.compile(r"^([+-]?)(M?)(\d+(?:\.\d+)?)/(\d+)\s+([\d.]+)s")
TRACE_RE = re.compile(r"^\s*([A-Za-z ]+?)\s*\|.*\|\s*([-\d.]+)\s+([-\d.]+)\s*$")


class Engine:
    def __init__(self, path):
        self.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, bufsize=1)
        self.send("uci")
        self.read_until("uciok")
        self.send("setoption name Threads value 1")
        self.send("setoption name Hash value 16")
        self.send("isready")
        self.read_until("readyok")

    def send(self, s):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()

    def read_until(self, prefix):
        lines = []
        for line in self.p.stdout:
            line = line.rstrip("\n")
            lines.append(line)
            if line.startswith(prefix):
                return lines
        raise RuntimeError("engine exited")

    def search(self, fen, depth, only=None):
        self.send(f"position fen {fen}")
        self.send(f"go depth {depth}" + (f" searchmoves {only}" if only else ""))
        score, pv = None, []
        for line in self.read_until("bestmove"):
            t = line.split()
            if t and t[0] == "info" and "score" in t and "pv" in t:
                i = t.index("score")
                kind, val = t[i + 1], int(t[i + 2])
                score = val if kind == "cp" else (2000 - abs(val)) * (1 if val > 0 else -1)
                pv = t[t.index("pv") + 1:]
        return score, pv

    def trace(self, fen):
        self.send(f"position fen {fen}")
        self.send("eval")
        self.send("isready")
        terms = {}
        for line in self.read_until("readyok"):
            m = TRACE_RE.match(line)
            if m and m.group(1).strip() in TERMS:
                terms[m.group(1).strip()] = (float(m.group(2)), float(m.group(3)))
        return terms or None

    def quit(self):
        try:
            self.send("quit")
            self.p.wait(timeout=2)
        except Exception:
            self.p.kill()


_local = threading.local()
_engines = []
_lock = threading.Lock()


def engine(path):
    if not hasattr(_local, "e"):
        _local.e = Engine(path)
        with _lock:
            _engines.append(_local.e)
    return _local.e


def clamp(cp, lim=1000):
    return max(-lim, min(lim, cp))


def phase128(board):
    npm = sum(SF_NPM.get(p.piece_type, 0) for p in board.piece_map().values())
    return max(0, min(128, round((npm - 1500) * 128 / (15258 - 1500))))


def blend(terms, board, pov):
    """Phase-blended per-term values in centipawns from `pov`'s side."""
    ph = phase128(board)
    sign = 1 if pov == chess.WHITE else -1
    return {k: sign * 100 * (mg * ph + eg * (128 - ph)) / 128 for k, (mg, eg) in terms.items()}


def material_signature(board):
    def side(c):
        return "K" + "".join(chess.piece_symbol(pt).upper() * len(board.pieces(pt, c))
                             for pt in (chess.QUEEN, chess.ROOK, chess.BISHOP, chess.KNIGHT, chess.PAWN))
    return f"{side(chess.WHITE)}v{side(chess.BLACK)}"


def loose_pieces(board, color):
    """Our non-pawn pieces that are attacked and undefended, or attacked by a cheaper piece."""
    out = []
    for sq, p in board.piece_map().items():
        if p.color != color or p.piece_type in (chess.PAWN, chess.KING):
            continue
        attackers = board.attackers(not color, sq)
        if not attackers:
            continue
        cheapest = min(VALUES[board.piece_type_at(a)] or 100 for a in attackers)
        if not board.attackers(color, sq) or cheapest < VALUES[p.piece_type]:
            out.append(chess.square_name(sq) + chess.piece_symbol(p.piece_type).upper())
    return out


def king_zone_attackers(board, color):
    ksq = board.king(color)
    zone = chess.SquareSet(chess.BB_KING_ATTACKS[ksq]) | chess.SquareSet([ksq])
    attackers = set()
    for sq in zone:
        attackers |= set(board.attackers(not color, sq))
    return len([a for a in attackers if board.piece_type_at(a) != chess.PAWN])


def parse_comment(text):
    m = COMMENT_RE.match(text.strip())
    if not m:
        return None
    sign, mate, val, depth, secs = m.groups()
    if mate:
        cp = 2000 - int(float(val))
    else:
        cp = round(float(val) * 100)
    cp = -cp if sign == "-" else cp
    return cp, int(depth), float(secs)


def collect(pgn_paths, name):
    items = []
    for path in pgn_paths:
        with open(path, encoding="utf-8", errors="replace") as f:
            while (game := chess.pgn.read_game(f)) is not None:
                white, black = game.headers.get("White", ""), game.headers.get("Black", "")
                if name in white:
                    color, opp = chess.WHITE, black
                elif name in black:
                    color, opp = chess.BLACK, white
                else:
                    continue
                result = game.headers.get("Result", "*")
                score = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5}.get(result)
                if score is not None and color == chess.BLACK:
                    score = 1.0 - score
                board = game.board()
                for node in game.mainline():
                    mover = board.turn
                    info = parse_comment(node.comment)
                    if mover == color and info and info[1] > 1 and info[2] > 0:
                        before = board.fen()
                        after_board = board.copy()
                        after_board.push(node.move)
                        items.append({
                            "file": path, "opponent": opp, "game_score": score,
                            "color": "w" if color == chess.WHITE else "b",
                            "fen": before, "move": board.san(node.move), "uci": node.move.uci(),
                            "after": after_board.fen(), "nano_eval": info[0], "nano_depth": info[1],
                        })
                    board.push(node.move)
    return items


def analyse(item, sf, depth):
    e = engine(sf)
    before, pv_before = e.search(item["fen"], depth)
    after_board = chess.Board(item["after"])
    if pv_before and pv_before[0] == item["uci"]:
        after, pv_after = before, pv_before
    else:
        # Same root and depth as the best-move search, so the comparison is like for like.
        after, pv_after = e.search(item["fen"], depth, only=item["uci"])
    item["sf_before"], item["sf_after"] = before, after
    item["sf_best"] = pv_before[0] if pv_before else None
    item["sf_reply"] = pv_after[1] if len(pv_after) > 1 else None
    item["loss"] = clamp(before) - clamp(after)
    item["trace_before"] = e.trace(item["fen"])
    item["trace_after"] = e.trace(item["after"]) if not after_board.is_game_over() else None
    return item


def classify(item):
    board = chess.Board(item["fen"])
    after = chess.Board(item["after"])
    us = board.turn
    tags = []
    loose = loose_pieces(after, us)
    reply = chess.Move.from_uci(item["sf_reply"]) if item["sf_reply"] else None
    if reply and reply in after.legal_moves and after.is_capture(reply):
        victim = after.piece_type_at(reply.to_square)
        if victim and victim != chess.PAWN:
            tags.append("loses_piece")
    if loose:
        tags.append("loose_piece")
    if reply and reply in after.legal_moves and after.gives_check(reply):
        tags.append("allows_check")
    if king_zone_attackers(after, us) >= 3:
        tags.append("king_under_attack")
    if phase128(board) <= 32:
        tags.append("endgame")
    if not tags:
        tags.append("quiet")
    item["tags"] = tags
    item["loose"] = loose
    item["phase"] = phase128(board)
    item["signature"] = material_signature(board)

    delta = None
    if item["trace_before"] and item["trace_after"]:
        b = blend(item["trace_before"], board, us)
        a = blend(item["trace_after"], after, us)
        delta = {k: a.get(k, 0) - b.get(k, 0) for k in TERMS if k != "Material"}
        item["worst_term"] = min(delta, key=delta.get)
    else:
        item["worst_term"] = None
    item["term_delta"] = delta
    return item


def term_suspicion(flagged, baseline):
    """Mean |term| in flagged positions divided by mean |term| over all positions."""
    def means(items):
        acc = defaultdict(list)
        for it in items:
            if not it["trace_before"]:
                continue
            board = chess.Board(it["fen"])
            for k, v in blend(it["trace_before"], board, board.turn).items():
                acc[k].append(abs(v))
        return {k: statistics.mean(v) for k, v in acc.items() if v}
    f, b = means(flagged), means(baseline)
    rows = [(k, f[k], b[k], f[k] / b[k] if b[k] > 0.5 else 0) for k in TERMS if k in f and k in b]
    return sorted(rows, key=lambda r: -r[3])


def fmt_score(cp):
    return f"{cp / 100:+.2f}" if abs(cp) < 1900 else ("+M" if cp > 0 else "-M")


def write_reports(items, blunders, misjudged, args):
    lines = ["# NanoChess evaluation gap report", "",
             f"PGNs: {', '.join(args.pgn)}  ",
             f"NanoChess moves analysed: {len(items)}  ",
             f"Stockfish depth: {args.depth}, blunder threshold: {args.blunder} cp, "
             f"misjudgment threshold: {args.misjudge} cp", ""]

    lines += ["## Blunders by pattern", "",
              "A move can carry several tags; cp lost is SF11's eval before minus after.", "",
              "| Tag | Moves | Total cp lost | Avg cp lost |", "|---|---|---|---|"]
    by_tag = defaultdict(list)
    for it in blunders:
        for t in it["tags"]:
            by_tag[t].append(it["loss"])
    for t, losses in sorted(by_tag.items(), key=lambda kv: -sum(kv[1])):
        lines.append(f"| {t} | {len(losses)} | {sum(losses)} | {round(statistics.mean(losses))} |")

    lines += ["", "## Blunders by SF11 term that got worse", "",
              "Which static term moved most against us with the move (quiet blunders only, "
              "so pure material losses don't dominate).", "",
              "| Term | Blunders | What NanoChess is missing |", "|---|---|---|"]
    worst = Counter(it["worst_term"] for it in blunders
                    if it["worst_term"] and "loses_piece" not in it["tags"])
    for term, n in worst.most_common():
        lines.append(f"| {term} | {n} | {HINTS.get(term, '')} |")

    lines += ["", "## Misjudgments", "",
              f"Positions where NanoChess's search score and SF11's differ by more than {args.misjudge} cp.", ""]
    over = [it for it in misjudged if it["nano_eval"] > it["sf_before"]]
    lines.append(f"- Too optimistic: {len(over)}, too pessimistic: {len(misjudged) - len(over)}")
    eg = [it for it in misjudged if it["phase"] <= 32]
    lines.append(f"- In endgames (low material): {len(eg)} of {len(misjudged)}")
    drawish = [it for it in eg if abs(it["sf_before"]) < 50 and abs(it["nano_eval"]) > 150]
    lines.append(f"- Endgames SF11 calls drawn that NanoChess thinks are decided: {len(drawish)}")
    if eg:
        lines += ["", "Most frequent misjudged endgame material:", "",
                  "| Material | Count |", "|---|---|"]
        for sig, n in Counter(it["signature"] for it in eg).most_common(10):
            lines.append(f"| {sig} | {n} |")

    lines += ["", "## Term suspicion", "",
              "Average size of each SF11 term in the positions we get wrong, relative to all "
              "positions. A ratio well above 1 means the term matters most exactly where we fail.", "",
              "| Term | Flagged avg cp | All avg cp | Ratio | What NanoChess is missing |",
              "|---|---|---|---|---|"]
    for k, f, b, r in term_suspicion(blunders + misjudged, items):
        lines.append(f"| {k} | {f:.0f} | {b:.0f} | {r:.2f} | {HINTS.get(k, '')} |")

    lines += ["", "## Worst blunders", "", "| Loss | Move | SF best | SF reply | Tags | FEN |",
              "|---|---|---|---|---|---|"]
    for it in sorted(blunders, key=lambda x: -x["loss"])[:args.examples]:
        lines.append(f"| {it['loss']} | {it['move']} | {it['sf_best']} | {it['sf_reply']} | "
                     f"{', '.join(it['tags'])} | `{it['fen']}` |")

    lines += ["", "## Worst misjudgments", "", "| NanoChess | SF11 | Material | FEN |", "|---|---|---|---|"]
    for it in sorted(misjudged, key=lambda x: -abs(clamp(x["nano_eval"]) - clamp(x["sf_before"])))[:args.examples]:
        lines.append(f"| {fmt_score(it['nano_eval'])} | {fmt_score(it['sf_before'])} | "
                     f"{it['signature']} | `{it['fen']}` |")

    with open(args.out + ".md", "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    keep = ("fen", "move", "uci", "sf_best", "sf_reply", "nano_eval", "sf_before", "sf_after",
            "loss", "tags", "loose", "worst_term", "signature", "phase", "term_delta", "file", "opponent")
    with open(args.out + ".json", "w", encoding="utf-8") as f:
        json.dump({"blunders": [{k: it.get(k) for k in keep} for it in blunders],
                   "misjudged": [{k: it.get(k) for k in keep} for it in misjudged]}, f, indent=1)

    with open(args.out + ".epd", "w", encoding="ascii") as f:
        for kind, group in (("blunder", blunders), ("misjudged", misjudged)):
            for it in group:
                board = chess.Board(it["fen"])
                best = board.san(chess.Move.from_uci(it["sf_best"])) if it["sf_best"] else "?"
                f.write(f'{board.epd()} bm {best}; c0 "{kind}"; c1 "{",".join(it["tags"])}"; '
                        f'c2 "nano {it["move"]}";\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sf", required=True, help="Stockfish 11 executable (needs its `eval` command)")
    ap.add_argument("--pgn", nargs="+", default=["build/ladder_*.pgn", "build/sf11_*.pgn"])
    ap.add_argument("--name", default="NanoChess", help="substring identifying NanoChess in PGN headers")
    ap.add_argument("--depth", type=int, default=18)
    ap.add_argument("--workers", type=int, default=24)
    ap.add_argument("--blunder", type=int, default=100, help="cp lost to count a move as a blunder")
    ap.add_argument("--misjudge", type=int, default=150, help="cp disagreement to count a misjudgment")
    ap.add_argument("--examples", type=int, default=25)
    ap.add_argument("--out", default="build/eval_gaps")
    args = ap.parse_args()

    paths = sorted({p for g in args.pgn for p in glob.glob(g)})
    if not paths:
        raise SystemExit("no PGN files matched")
    items = collect(paths, args.name)
    print(f"{len(paths)} PGNs, {len(items)} NanoChess moves; searching at depth {args.depth} "
          f"with {args.workers} workers...", flush=True)

    done = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(analyse, it, args.sf, args.depth) for it in items]
        for fut in futures:
            fut.result()
            done += 1
            if done % 500 == 0:
                print(f"  {done}/{len(items)}", flush=True)
    for e in _engines:
        e.quit()

    for it in items:
        classify(it)
    alive = [it for it in items if abs(it["sf_before"]) <= 400]
    blunders = [it for it in alive if it["loss"] >= args.blunder]
    misjudged = [it for it in alive
                 if abs(clamp(it["nano_eval"]) - clamp(it["sf_before"])) >= args.misjudge
                 and it not in blunders]

    write_reports(items, blunders, misjudged, args)
    print(f"{len(blunders)} blunders, {len(misjudged)} misjudgments -> {args.out}.md / .json / .epd")


if __name__ == "__main__":
    main()
