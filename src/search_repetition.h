// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_REPETITION_H_INCLUDED
#define SEARCH_REPETITION_H_INCLUDED

#include "position.h"
#include "movegen.h"
#include "api/petrified_outcome.h"
#include <algorithm>

namespace Stockfish {

struct PointsRepetitionWin {
    Move move = Move::none();
    Value value = -VALUE_INFINITE;
};

// An optional legal move to an actual third occurrence, not the orthodox
// twofold-after-root shortcut. The caller must resolve current terminals first.
// No score is cached: this proof depends on the complete reversible history.
inline PointsRepetitionWin points_repetition_win(Position& pos, int ply,
                                                 Move excluded = Move::none()) {
    const Color us = pos.side_to_move();
    if (!pos.game_of_points() || ply >= MAX_PLY
        || pos.points(us) <= pos.points(~us))
        return {};

    // The next position must already have occurred twice on the opposite
    // side's turn. Pawn moves, captures and null moves bound the usable history.
    const int end = std::min(pos.rule50_count(), pos.state()->pliesFromNull);
    const StateInfo* prior = pos.state();
    bool possible = false;
    for (int distance = 1; distance <= end && prior->previous; ++distance) {
        prior = prior->previous;
        if (distance >= 3 && (distance & 1) && prior->repetition) {
            possible = true;
            break;
        }
    }
    if (!possible)
        return {};

    for (Move move : MoveList<ALL>(pos)) {
        if (move == excluded || move.type_of() != NORMAL || pos.capture(move)
            || type_of(pos.moved_piece(move)) == PAWN)
            continue;
        StateInfo child;
        pos.do_move(move, child);
        Value value = -VALUE_INFINITE;
        if (pos.is_repetition(0)) {
            const auto result = Petrified::outcome(pos, true, ply + 1);
            if (result.termination == Petrified::Termination::ThreefoldRepetition)
                value = -result.value;
        }
        pos.undo_move(move);
        if (value > VALUE_DRAW)
            return {move, value};
    }
    return {};
}

} // namespace Stockfish
#endif
