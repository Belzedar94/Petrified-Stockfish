// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_QSEARCH_H_INCLUDED
#define SEARCH_QSEARCH_H_INCLUDED

#include "api/petrified_outcome.h"
#include "movegen.h"
#include "position.h"

namespace Stockfish::Search {

// A zero threshold uses the existing points SEE, including fatal captures,
// earned points, walls and legal recaptures. It is a selective-search heuristic,
// not an upper bound on the positional or eventual settlement value of a move.
inline bool qsearch_see_candidate(const Position& pos, Move move, bool forced,
                                 bool givesCheck, Value bestValue) {
    return pos.game_of_points() && !pos.checkers() && !forced && !is_loss(bestValue)
        && pos.capture(move) && move.type_of() != PROMOTION && !givesCheck
        && !pos.see_ge(move, 0);
}

// Call after the real move. Never discard a settlement, a resignation claim,
// a bare-king ending or a wall-forced reply based on the local exchange model.
inline bool qsearch_see_prunable_child(const Position& child, int ply) {
    return child.count<ALL_PIECES>(WHITE) > 1 && child.count<ALL_PIECES>(BLACK) > 1
        && !Petrified::outcome(child, true, ply).terminal()
        && Petrified::resignation_value(child, ply) == -VALUE_INFINITE
        && unique_legal_move(child) == Move::none();
}

}  // namespace Stockfish::Search
#endif
