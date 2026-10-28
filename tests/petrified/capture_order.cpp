// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movepick.h"
#include "position.h"
#include "uci_move.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace Stockfish;

static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
static std::string mirror(std::string move) {
    move[1] = char('9' - move[1] + '0');
    move[3] = char('9' - move[3] + '0');
    return move;
}
int main() {
    Attacks::init(); Position::init();
    auto main = std::make_unique<ButterflyHistory>(); main->fill(0);
    auto captures = std::make_unique<CapturePieceToHistory>(); captures->fill(0);
    PieceToHistory continuation; continuation.fill(0);
    const PieceToHistory* histories[] = {&continuation};
    const auto pick = [&](const Position& pos, Move tt = Move::none(), bool skipQuiets = false) {
        MovePicker picker(pos, tt, skipQuiets ? 1 : DEPTH_QS, main.get(), nullptr,
                          captures.get(), histories, nullptr, 0);
        if (skipQuiets) picker.skip_quiet_moves();
        std::vector<std::string> moves;
        for (Move m; (m = picker.next_move()) != Move::none();) moves.push_back(UCI::move(m));
        return moves;
    };
    struct Case { const char* fen; const char* first; };
    const Case cases[] = {
      // PxB preserves a pawn; QxR destroys the queen. Orthodox MVV prefers QxR.
      {"4k3/8/8/1r5b/6P1/8/8/1Q2K3 w - - 0 1", "g4h5"},
      // Capturing promotion retains its new piece, ahead of a petrifying rook.
      {"2r4k/1P6/8/8/8/8/8/2R1K3 w - - 0 1", "b7c8q"},
      // The EP victim is off the destination square.
      {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6"},
      // Both a pawn and a queen can remove the checking rook; keep the pawn.
      {"k7/8/8/4Q2r/6P1/8/8/7K w - - 0 1", "g4h5"}
    };
    for (const auto& test : cases) for (bool black : {false, true}) {
        Position pos; StateInfo state;
        require(!pos.set(std::string(test.fen)+" points=0,0",false,&state),"fixture FEN");
        if (black) require(!pos.flip(),"mirrored FEN");
        auto moves = pick(pos);
        require(!moves.empty() && moves.front() == (black ? mirror(test.first) : test.first),
                "points capture order");
        std::vector<std::string> legal;
        for (Move m : MoveList<ALL>(pos))
            if (pos.checkers() || pos.capture_stage(m)) legal.push_back(UCI::move(m));
        auto sorted = moves; std::sort(sorted.begin(),sorted.end());
        std::sort(legal.begin(),legal.end());
        require(sorted == legal,"all stage moves emitted exactly once");
        const Move tt = UCI::to_move(pos,moves.back());
        const auto withTT = pick(pos,tt);
        require(withTT.front() == moves.back() && withTT.size() == moves.size(),"TT priority and uniqueness");
    }
    Position pos; StateInfo state;
    require(!pos.set(cases[0].fen,false,&state),"legacy FEN");
    require(pick(pos).front()=="b1b5","legacy MVV unchanged");
    require(!pos.set(std::string(cases[0].fen)+" points=0,0",false,&state),"stage FEN");
    const auto stages=pick(pos,Move::none(),true);
    require(stages==std::vector<std::string>({"g4h5","b1b5"}),"good/bad capture boundary preserved");
    std::cout << "PASS: capture order, both colors, EP, promotion, evasions, TT and stage coverage\n";
}
