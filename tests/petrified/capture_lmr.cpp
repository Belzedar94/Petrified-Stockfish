// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "search_capture.h"
#include "api/petrified_outcome.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;
static void require(bool condition, const char* why) {
    if (!condition) { std::cerr << why << '\n'; std::exit(1); }
}
static int material(const Position& pos, Color us) {
    int result = 0;
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
        if (Piece p = pos.piece_on(s))
            result += (color_of(p) == us ? 1 : -1) * PieceValue[p];
    return result;
}
static std::string notation(Move m) {
    std::string text;
    for (Square s : {m.from_sq(), m.to_sq()}) {
        text += char('a' + file_of(s)); text += char('1' + rank_of(s));
    }
    if (m.type_of() == PROMOTION) text += " pnbrqk"[m.promotion_type()];
    return text;
}
static std::size_t checks = 0, fatal = 0, pawnCaptures = 0, promotions = 0, ep = 0;
static void inspect(Position& pos, std::ofstream& cases, bool emit) {
    const auto fen = pos.fen(); const auto key = pos.key();
    const auto us = pos.side_to_move();
    const int before = material(pos, us) + 100 * (pos.points(us) - pos.points(~us));
    for (Move m : MoveList<ALL>(pos)) {
        const Piece moved = pos.moved_piece(m);
        const bool stage = pos.capture_stage(m);
        StateInfo state; pos.do_move(m, state);
        const Piece victim = pos.captured_piece();
        const int value = Search::capture_lmr_value(pos.game_of_points(), moved, victim, m);
        const int after = material(pos, us) + 100 * (pos.points(us) - pos.points(~us));
        require(Search::capture_lmr_value(false, moved, victim, m) == PieceValue[victim],
                "legacy capture term changed");
        if (pos.game_of_points()) require(value == after - before, "transaction differs from board/points recount");
        ++checks;
        if (victim) {
            fatal += type_of(moved) != PAWN; pawnCaptures += type_of(moved) == PAWN;
        }
        promotions += m.type_of() == PROMOTION; ep += m.type_of() == EN_PASSANT;
        pos.undo_move(m);
        require(pos.fen() == fen && pos.key() == key, "transaction test failed undo");
        if (cases && pos.game_of_points() && stage && emit)
            cases << "{\"fen\":\"" << fen << "\",\"move\":\"" << notation(m)
                  << "\",\"value\":" << value << "}\n";
    }
}
int main(int argc, char** argv) {
    Attacks::init(); Position::init(); std::ofstream cases;
    if (argc == 3 && std::string(argv[1]) == "--cases") cases.open(argv[2]);
    for (const char* board : {
        "8/7k/8/3p4/3Q4/8/8/K7 w - - 0 1",
        "7k/8/8/3q4/2P5/8/8/K7 w - - 0 1",
        "k7/8/8/2p5/3Q4/8/8/7K b - - 0 1",
        "k6r/6P1/8/8/8/8/8/K7 w - - 0 1",
        "7k/8/8/8/8/8/6p1/K6R b - - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
        "7k/1P6/8/8/8/8/8/K7 w - - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"
    }) for (const char* scores : {"", " points=0,0", " points=19,0", " points=0,21"}) {
        Position pos; StateInfo state;
        require(!pos.set(std::string(board)+scores, false, &state), "invalid capture fixture");
        inspect(pos, cases, true);
    }
    std::mt19937 rng(20261029);
    for (int game = 0; game < 40; ++game) {
        Position pos; std::vector<StateInfo> states(161); std::vector<Move> played;
        const std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        require(!pos.set(start + (game % 5 ? " points=0,0" : ""), false, &states[0]), "startpos");
        for (int ply = 0; ply < 160; ++ply) {
            if (pos.game_of_points() && Petrified::outcome(pos, true).terminal()) break;
            inspect(pos, cases, game < 12 && ply % 11 == 0);
            MoveList<ALL> moves(pos); if (!moves.size()) break;
            Move m = moves.begin()[rng() % moves.size()]; played.push_back(m);
            pos.do_move(m, states[ply + 1]);
        }
        while (!played.empty()) { pos.undo_move(played.back()); played.pop_back(); }
        require(pos.fen() == start + (game % 5 ? " points=0,0" : ""), "walk undo");
    }
    require(fatal && pawnCaptures && promotions && ep, "special-move coverage missing");
    require(Search::capture_lmr_value(true, W_QUEEN, B_PAWN, Move(SQ_D4,SQ_D5)) == -2230, "fatal queen transaction");
    require(Search::capture_lmr_value(true, W_PAWN, B_QUEEN, Move(SQ_C4,SQ_D5)) == 3438, "surviving pawn transaction");
    std::cout << "capture LMR transactions=" << checks << " fatal=" << fatal
              << " pawn-captures=" << pawnCaptures << " promotions=" << promotions << " en-passant=" << ep << '\n';
}
