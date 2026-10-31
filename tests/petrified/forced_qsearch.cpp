// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "uci_move.h"
#include "api/petrified_outcome.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;
static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
static unsigned checked = 0, unique = 0, quiet = 0, branches = 0, empty = 0;
static std::ofstream cases;
static void check(const Position& pos, bool emit) {
    Move expected[MAX_MOVES];
    const Move* end = reference_generate<ALL>(pos, expected);
    const auto size = end - expected;
    const auto actual = MoveList<ALL>(pos);
    require(actual.size() == std::size_t(size)
            && std::equal(actual.begin(), actual.end(), expected), "full list/order changed");
    require(unique_legal_move(pos) == (size == 1 ? expected[0] : Move::none()),
            "capped query differs from full reference");
    require(has_legal_move(pos) == (size != 0), "existence mismatch");
    ++checked;
    if (!size) ++empty;
    else if (size > 1) ++branches;
    else { ++unique; if (!pos.capture_stage(expected[0]) && !pos.checkers()) ++quiet; }
    if (emit && cases) {
        cases << "{\"fen\":\"" << pos.fen() << "\",\"legal\":[";
        for (const Move* m = expected; m != end; ++m)
            cases << (m == expected ? "" : ",") << '"' << UCI::move(*m) << '"';
        cases << "],\"terminal\":" << (Petrified::outcome(pos, true).terminal() ? "true" : "false")
              << ",\"result\":\"" << Petrified::outcome(pos, true).result() << "\"}\n";
    }
}
int main(int argc, char** argv) {
    if (argc == 2) { cases.open(argv[1]); require(bool(cases), "cannot create cases"); }
    Attacks::init(); Position::init();
    // Isolated rooks prevent insufficient-material settlement. Both kings
    // have a one-square corridor: every move is quiet until repetition.
    const std::vector<std::string> boards = {
        "***r***k/*******1/********/********/********/********/1*******/K**R****",
        "***r***k/********/********/********/********/********/********/K**R****",
        "***r***k/******2/********/********/********/********/2******/K**R****",
        "***r***k/*******1/********/********/**p*****/**1*****/**P*****/K**R****",
        "***r***k/*******1/********/********/**p*****/**R*****/********/K*******",
        "1**r***k/P******1/********/********/********/********/********/K**R****",
        "4k3/8/8/3pP3/8/8/8/4K3",
        "r3k2r/8/8/8/8/8/8/R3K2R",
        "4r1k1/8/8/8/8/8/4P3/4K3",
        "*4r2/2p1qpkQ/2*2*P1/1p2p3/1P2P1*1/8/2P5/1N2K1NR"
    };
    for (std::size_t i = 0; i < boards.size(); ++i)
        for (const char* side : {"w", "b"})
            for (const char* score : {"0,0", "1,0", "0,1", "20,0", "21,0"}) {
                if (i == 9 && std::string(side) == "w") continue; // The black king is mated.
                Position pos; std::deque<StateInfo> states(1);
                const auto rights = i == 7 ? " KQkq " : " - ";
                const auto ep = i == 6 && std::string(side) == "w" ? "d6" : "-";
                require(!pos.set(boards[i] + " " + side + rights + ep + " 0 1 points=" + score,
                                 false, &states.back()), "fixture FEN rejected");
                check(pos, true);
                const auto original = pos.fen(); const auto key = pos.key();
                std::vector<Move> played;
                for (int ply = 0; ply < 16 && !Petrified::outcome(pos, true).terminal(); ++ply) {
                    const Move move = unique_legal_move(pos);
                    if (move == Move::none()) break;
                    played.push_back(move); states.emplace_back(); pos.do_move(move, states.back());
                    check(pos, false);
                }
                while (!played.empty()) {
                    pos.undo_move(played.back()); played.pop_back(); states.pop_back(); check(pos, false);
                }
                require(pos.fen() == original && pos.key() == key, "forced chain undo mismatch");
            }
    std::mt19937 rng(20261030);
    for (int game = 0; game < 60; ++game) {
        Position pos; std::deque<StateInfo> states(1); std::vector<Move> played;
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=0,0",
                         false, &states.back()), "walk FEN");
        for (int ply = 0; ply < 200; ++ply) {
            check(pos, ply % 24 == 0);
            if (Petrified::outcome(pos, true).terminal()) break;
            const auto legal = MoveList<ALL>(pos);
            const Move move = legal.begin()[rng() % legal.size()]; played.push_back(move);
            states.emplace_back(); pos.do_move(move, states.back());
        }
        while (!played.empty()) {
            pos.undo_move(played.back()); played.pop_back(); states.pop_back(); check(pos, false);
        }
    }
    require(unique > 0 && quiet > 0 && branches > 0 && empty > 0, "missing query class coverage");
    std::cout << "{\"positions\":" << checked << ",\"unique\":" << unique
              << ",\"forced_quiet\":" << quiet << ",\"multiple\":" << branches
              << ",\"no_moves\":" << empty << "}\n";
}
