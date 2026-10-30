// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movegen_reference.h"
#include "search_qsearch.h"
#include "uci_move.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;

static void require(bool ok, const std::string& message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
static std::vector<Move> full(const Position& pos) {
    Move moves[MAX_MOVES]; const auto end = reference_generate<ALL>(pos, moves);
    return {moves, end};
}
static int balance(const Position& pos, Color us) {
    int value = 100 * (pos.points(us) - pos.points(~us));
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
        if (const Piece pc = pos.piece_on(sq))
            value += (color_of(pc) == us ? 1 : -1) * PieceValue[pc];
    return value;
}
static int reply(Position& pos, Square target) {
    if (!pos.piece_on(target) || type_of(pos.piece_on(target)) == KING) return 0;
    const Color us = pos.side_to_move(); const int before = balance(pos, us);
    int best = 0;
    for (Move move : full(pos)) {
        const Square captured = move.type_of() == EN_PASSANT
                              ? move.to_sq() - pawn_push(us) : move.to_sq();
        if (!pos.capture(move) || captured != target) continue;
        StateInfo state; pos.do_move(move, state);
        const int gain = balance(pos, us) - before - reply(pos, move.to_sq());
        pos.undo_move(move); best = std::max(best, gain);
    }
    return best;
}
static int exchange(Position& pos, Move move) {
    if (move.type_of() == CASTLING || type_of(pos.moved_piece(move)) == KING) return 0;
    const Color us = pos.side_to_move(); const int before = balance(pos, us);
    StateInfo state; pos.do_move(move, state);
    const int gain = balance(pos, us) - before - reply(pos, move.to_sq());
    pos.undo_move(move); return gain;
}
static void moves_json(std::ostream& out, const std::vector<Move>& moves) {
    out << '[';
    for (std::size_t i = 0; i < moves.size(); ++i)
        out << (i ? "," : "") << '"' << UCI::move(moves[i]) << '"';
    out << ']';
}
static std::ofstream cases;
static unsigned checked = 0, candidates = 0, pruned = 0, protectedChildren = 0,
                terminalChildren = 0, forcedChildren = 0, bareChildren = 0, exported = 0;

static void check(Position& pos, Move move, int expected = -1, bool emit = false,
                  const std::vector<std::string>* history = nullptr) {
    const auto fen = pos.fen(); const Key key = pos.key();
    const auto legal = full(pos); const bool forced = legal.size() == 1;
    const bool givesCheck = pos.gives_check(move);
    const int see = pos.game_of_points() ? exchange(pos, move) : 0;
    if (pos.game_of_points()) require(see == pos.points_see(move), "full exchange mismatch");
    const bool candidate = Search::qsearch_see_candidate(pos, move, forced, givesCheck, VALUE_DRAW);
    const bool referenceCandidate = pos.game_of_points() && !pos.checkers()
        && legal.size() > 1 && pos.capture(move) && move.type_of() != PROMOTION
        && !givesCheck && see < 0;
    require(candidate == referenceCandidate, "parent guards differ from full reference");
    require(!Search::qsearch_see_candidate(pos, move, true, givesCheck, VALUE_DRAW), "forced guard");
    require(!Search::qsearch_see_candidate(pos, move, forced, true, VALUE_DRAW), "checking guard");
    require(!Search::qsearch_see_candidate(pos, move, forced, givesCheck, -VALUE_INFINITE), "loss guard");
    StateInfo state; pos.do_move(move, state);
    const auto childLegal = full(pos); const auto end = Petrified::outcome(pos, true, 1);
    const bool claim = Petrified::resignation_value(pos, 1) != -VALUE_INFINITE;
    unsigned live[COLOR_NB] = {0, 0};
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
        if (pos.piece_on(sq)) ++live[color_of(pos.piece_on(sq))];
    const bool bare = live[WHITE] <= 1 || live[BLACK] <= 1;
    const bool referenceChild = !bare && !end.terminal() && !claim && childLegal.size() > 1;
    const bool prune = candidate && Search::qsearch_see_prunable_child(pos, 1);
    require(prune == (referenceCandidate && referenceChild), "child guards differ from full reference");
    if (expected >= 0) require(prune == bool(expected), "hand fixture: " + fen + " " + UCI::move(move));
    ++checked; candidates += candidate; pruned += prune;
    protectedChildren += candidate && !prune;
    terminalChildren += candidate && end.terminal();
    forcedChildren += candidate && childLegal.size() == 1;
    bareChildren += candidate && bare;
    if (emit && cases) {
        cases << "{\"fen\":\"" << fen << "\",\"move\":\"" << UCI::move(move)
              << "\",\"value\":" << see << ",\"candidate\":" << candidate
              << ",\"prune\":" << prune << ",\"parent_legal\":";
        moves_json(cases, legal);
        cases << ",\"child_legal\":"; moves_json(cases, childLegal);
        cases << ",\"child_terminal\":" << end.terminal()
              << ",\"child_result\":\"" << end.result() << "\",\"child_reason\":\""
              << end.reason() << "\",\"child_claim\":" << claim
              << ",\"child_bare\":" << bare << ",\"child_fen\":\"" << pos.fen() << '"';
        if (history) {
            cases << ",\"history\":[";
            for (std::size_t i=0; i<history->size(); ++i)
                cases << (i ? "," : "") << '"' << (*history)[i] << '"';
            cases << ']';
        }
        cases << "}\n"; ++exported;
    }
    pos.undo_move(move);
    require(pos.fen() == fen && pos.key() == key && pos.pos_is_ok(), "state/undo changed");
}

int main(int argc, char** argv) {
    Attacks::init(); Position::init();
    if (argc == 2) { cases.open(argv[1]); require(bool(cases), "cannot open cases"); }
    struct Fixture { const char* board; const char* move; int prune; const char* scores; };
    for (const auto& f : {
        Fixture{"7k/7r/8/3p4/3Q4/8/8/K2R4", "d4d5", 1, "0,0"},
        Fixture{"8/7k/8/3p4/3Q4/8/8/K7", "d4d5", 0, "0,0"},
        Fixture{"7k/7r/8/3p4/3Q4/8/8/K7", "d4d5", 0, "0,0"},
        Fixture{"7k/7r/8/3p4/3Q4/8/8/K2R4", "d4d5", 0, "0,22"},
        Fixture{"***r***k/*******1/********/********/**p*****/**Q*****/********/K*******", "c3c4", 0, "0,0"},
        Fixture{"***r***k/*******1/********/********/**p*****/**Q*****/11******/K**R****", "c3c4", 0, "0,0"},
        Fixture{"***r***k/******p*/*****Q**/********/********/********/1*******/K**R****", "f6g7", 0, "1,0"},
        Fixture{"7k/7r/8/8/8/5n2/5Q2/4K2R", "f2f3", 0, "0,0"},
        Fixture{"k6r/8/8/8/Qp6/8/8/R6K", "a4b4", 0, "0,0"},
        Fixture{"1r5k/P6r/8/8/8/8/8/K2R4", "a7b8q", 0, "0,0"},
        Fixture{"7k/7r/2p5/3p4/4P3/8/8/K2R4", "e4d5", 0, "0,0"},
        Fixture{"r3k2r/8/8/8/8/8/8/R3K2R", "a1a2", 0, "0,0"}
    }) {
        Position pos; StateInfo state;
        require(!pos.set(std::string(f.board) + " w - - 0 1 points=" + f.scores, false, &state), "fixture FEN");
        for (int flip = 0; flip < 2; ++flip) {
            auto uci = std::string(f.move);
            if (flip) { require(!pos.flip(), "flip"); uci[1] = char('1'+'8'-uci[1]); uci[3] = char('1'+'8'-uci[3]); }
            const Move move = UCI::to_move(pos, uci);
            require(move != Move::none(), "illegal fixture: " + pos.fen() + " " + uci);
            check(pos, move, f.prune, true);
        }
    }
    Position legacy; StateInfo legacyState;
    require(!legacy.set("7k/7r/8/3p4/3Q4/8/8/K2R4 w - - 0 1", false, &legacyState), "legacy FEN");
    check(legacy, UCI::to_move(legacy, "d4d5"), 0);
    std::mt19937 rng(20261030);
    for (int game = 0; game < 64; ++game) {
        Position pos; std::deque<StateInfo> states(1); std::vector<Move> played;
        std::vector<std::string> history;
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=0,0",
                         false, &states.back()), "walk FEN");
        for (int ply = 0; ply < 160 && !Petrified::outcome(pos, true).terminal(); ++ply) {
            const auto legal = full(pos);
            if (ply % 3 == 0)
                for (Move move : legal)
                    if (pos.capture(move) || move.type_of() == PROMOTION)
                        check(pos, move, -1, exported < 536, &history);
            const Move move = legal[rng() % legal.size()];
            played.push_back(move); history.push_back(UCI::move(move));
            states.emplace_back(); pos.do_move(move, states.back());
        }
        while (!played.empty()) {
            pos.undo_move(played.back()); played.pop_back(); states.pop_back();
            require(pos.pos_is_ok(), "walk undo");
        }
    }
    require(pruned > 100 && protectedChildren > 0 && terminalChildren > 0
            && forcedChildren > 0 && bareChildren > 0, "missing protection coverage");
    std::cout << "{\"checked\":" << checked << ",\"negative_candidates\":" << candidates
              << ",\"pruned\":" << pruned << ",\"protected_children\":" << protectedChildren
              << ",\"terminal_children\":" << terminalChildren << ",\"forced_children\":" << forcedChildren
              << ",\"bare_children\":" << bareChildren << ",\"exported\":" << exported << "}\n";
}
