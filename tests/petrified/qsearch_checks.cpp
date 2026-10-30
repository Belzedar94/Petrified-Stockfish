// SPDX-License-Identifier: GPL-3.0-or-later
// Compare the actual picker against full legality and real child king attacks.
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "movepick.h"
#include "uci_move.h"
#include "api/petrified_outcome.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <vector>
using namespace Stockfish;
static ButterflyHistory mainHistory;
static CapturePieceToHistory captureHistory;
static PieceToHistory continuation;
static const PieceToHistory* cont[] = {&continuation};
static unsigned positions = 0, comparisons = 0, checkedMoves = 0, quietChecks = 0;
static unsigned promotions = 0, castles = 0, mates = 0;
static std::ofstream cases;
static void require(bool ok, const char* why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
static std::vector<Move> pick(const Position& pos, bool checks, Move tt) {
    MovePicker picker(pos, tt, DEPTH_QS, &mainHistory, nullptr, &captureHistory,
                      cont, nullptr, 0, checks);
    std::vector<Move> out;
    for (Move m = picker.next_move(); m != Move::none(); m = picker.next_move()) {
        require(out.size() < MAX_MOVES, "picker does not terminate");
        require(std::find(out.begin(), out.end(), m) == out.end(), "duplicate TT/generated move");
        out.push_back(m);
    }
    require(picker.next_move() == Move::none(), "exhausted picker restarted");
    return out;
}
static std::set<unsigned> keys(const std::vector<Move>& moves) {
    std::set<unsigned> out;
    for (Move m : moves) out.insert(m.raw());
    return out;
}
static void array(const std::vector<Move>& moves) {
    cases << '[';
    for (std::size_t i = 0; i < moves.size(); ++i)
        cases << (i ? "," : "") << '"' << UCI::move(moves[i]) << '"';
    cases << ']';
}
static void check(Position& pos, bool emit) {
    Move all[MAX_MOVES]; const Move* end = reference_generate<ALL>(pos, all);
    std::vector<Move> legal(all, end), base, extra;
    const auto fen = pos.fen(); const auto key = pos.key();
    const bool inCheck = bool(pos.checkers());
    for (Move m : legal) {
        const bool stage = pos.capture_stage(m), fastCheck = pos.gives_check(m);
        StateInfo child; pos.do_move(m, child);
        const bool actualCheck = bool(pos.checkers());
        require(actualCheck == fastCheck, "gives_check differs from actual child attacks");
        pos.undo_move(m);
        require(pos.fen() == fen && pos.key() == key, "check query changed parent state");
        ++checkedMoves;
        if (inCheck || stage) base.push_back(m);
        else if (actualCheck) {
            extra.push_back(m); ++quietChecks;
            promotions += m.type_of() == PROMOTION; castles += m.type_of() == CASTLING;
        }
    }
    std::vector<Move> tts{Move::none()};
    if (!base.empty()) tts.push_back(base.front());
    if (!extra.empty()) tts.push_back(extra.front());
    for (Move m : legal)
        if (!pos.capture_stage(m) && !pos.gives_check(m)) { tts.push_back(m); break; }
    for (Move tt : tts) {
        auto expectedBase = base;
        if (tt != Move::none()) expectedBase.push_back(tt);
        const auto off = pick(pos, false, tt), on = pick(pos, true, tt);
        require(keys(off) == keys(expectedBase), "inherited capture/evasion/TT set changed");
        auto expected = expectedBase;
        if (pos.game_of_points()) expected.insert(expected.end(), extra.begin(), extra.end());
        require(keys(on) == keys(expected), "quiet checks differ from full make/undo reference");
        require(on.size() >= off.size() && std::equal(off.begin(), off.end(), on.begin()),
                "inherited capture/evasion/TT order changed");
        if (tt != Move::none()) require(!on.empty() && on.front() == tt, "TT priority changed");
        ++comparisons;
    }
    ++positions;
    if (emit && cases && pos.game_of_points() && !Petrified::outcome(pos, true).terminal()) {
        cases << "{\"fen\":\"" << fen << "\",\"in_check\":" << (inCheck ? "true" : "false")
              << ",\"legal\":"; array(legal); cases << ",\"quiet_checks\":"; array(extra);
        cases << ",\"children\":[";
        for (std::size_t i = 0; i < extra.size(); ++i) {
            StateInfo child; pos.do_move(extra[i], child);
            const auto outcome = Petrified::outcome(pos, true);
            const bool mate = bool(pos.checkers()) && !has_legal_move(pos);
            mates += mate;
            cases << (i ? "," : "") << "{\"move\":\"" << UCI::move(extra[i])
                  << "\",\"terminal\":" << (outcome.terminal() ? "true" : "false")
                  << ",\"mate\":" << (mate ? "true" : "false")
                  << ",\"result\":\"" << outcome.result() << "\"}";
            pos.undo_move(extra[i]);
        }
        cases << "]}\n";
    }
}
int main(int argc, char** argv) {
    if (argc == 2) { cases.open(argv[1]); require(bool(cases), "cases output"); }
    Attacks::init(); Position::init();
    const std::vector<std::string> fixtures = {
        "6*k/6**/6K1/4N3/8/********/1*******/r******* w - - 0 2",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "5k2/8/8/8/8/8/8/4K2R w K - 0 1", // Castling rook checks f8.
        "4k3/8/8/8/8/8/8/4KB1R w K - 0 1", // Discovered rook check.
        "3k4/P7/8/8/8/8/8/4K3 w - - 0 1", // Quiet rook underpromotion checks.
        "8/1P1k4/8/8/8/8/8/4K3 w - - 0 1", // Quiet knight underpromotion checks.
        "4r1k1/8/8/8/8/8/4P3/4K3 w - - 0 1", // In-check evasions.
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/8/2*5/4B3/4R1K1 w - - 0 1" // Discovered checks, wall.
    };
    for (const auto& fixture : fixtures)
        for (const char* suffix : {"", " points=0,0", " points=0,20", " points=0,21"}) {
            Position pos; StateInfo state;
            require(!pos.set(fixture + suffix, false, &state), "invalid fixture");
            check(pos, true);
            require(!pos.flip(), "fixture flip rejected"); check(pos, true);
        }
    std::mt19937 rng(20261030);
    for (int game = 0; game < 48; ++game) {
        Position pos; std::deque<StateInfo> states(1); std::vector<Move> history;
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=0,0",
                         false, &states.back()), "walk FEN");
        for (int ply = 0; ply < 160 && !Petrified::outcome(pos, true).terminal(); ++ply) {
            check(pos, ply % 24 == 0);
            const auto moves = MoveList<ALL>(pos);
            const Move move = moves.begin()[rng() % moves.size()]; history.push_back(move);
            states.emplace_back(); pos.do_move(move, states.back());
        }
        while (!history.empty()) {
            pos.undo_move(history.back()); history.pop_back(); states.pop_back(); check(pos, false);
        }
    }
    require(positions > 1000 && quietChecks > 0 && promotions > 0 && castles > 0,
            "missing quiet-check coverage");
    if (cases) require(mates >= 6, "missing win/tie/loss quiet mate fixtures");
    std::cout << "{\"positions\":" << positions << ",\"picker_comparisons\":" << comparisons
              << ",\"full_child_checks\":" << checkedMoves << ",\"quiet_checks\":" << quietChecks
              << ",\"underpromotion_checks\":" << promotions << ",\"castling_checks\":" << castles
              << ",\"exported_quiet_mates\":" << mates << "}\n";
}
