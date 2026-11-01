// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_SETTLEMENT_H_INCLUDED
#define SEARCH_SETTLEMENT_H_INCLUDED

#include "api/petrified_outcome.h"
#include "movegen.h"
#include "position.h"

namespace Stockfish::Search {

// A real non-pawn capture added a wall on the preceding ply. FENs and null
// moves do not establish this search context. Check evasions already receive
// a complete search in PR18, so they need no additional probe.
inline bool settlement_probe_eligible(const Position& pos) {
    return pos.game_of_points() && !pos.checkers()
        && pos.captured_piece() != NO_PIECE && pos.state()->petrified
        && pos.state()->pliesFromNull > 0;
}

struct SettlementProbe {
    Move  move  = Move::none();
    Value value = -VALUE_INFINITE;
};

// The caller has already resolved the parent's terminal/claim conditions.
// Prove only a strictly winning, immediately terminal quiet reply. This is a
// finite rules probe: no evaluation, recursive search, stand-pat suppression,
// new position state or TT entry. Ordinary qsearch is unchanged on a miss.
inline SettlementProbe winning_quiet_settlement(Position& pos, int ply,
                                                Move excluded = Move::none()) {
    SettlementProbe best;
    if (!settlement_probe_eligible(pos))
        return best;

    for (Move move : MoveList<ALL>(pos))
    {
        if (move == excluded || pos.capture_stage(move))
            continue;

        StateInfo child;
        pos.do_move(move, child);
        const auto end = Petrified::outcome(pos, true, ply + 1);
        pos.undo_move(move);

        if (end.terminal() && is_win(-end.value) && -end.value > best.value)
            best = {move, -end.value};
    }
    return best;
}

}  // namespace Stockfish::Search
#endif
