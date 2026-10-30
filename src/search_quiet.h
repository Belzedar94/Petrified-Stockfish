// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_QUIET_H_INCLUDED
#define SEARCH_QUIET_H_INCLUDED

#include "position.h"

namespace Stockfish::Search {

// Ordering heuristic only: a hypothetical capture earns points, and non-pawn
// capturers petrify. No replies, pins, settlement bonus or positional wall value.
inline bool points_capture_threat(PieceType attacker, PieceType victim) {
    if (attacker < PAWN || attacker > QUEEN || victim < PAWN || victim > QUEEN)
        return false;
    return PieceValue[victim] + 100 * Position::capture_points(victim)
             - (attacker == PAWN ? 0 : PieceValue[attacker]) > 0;
}

inline void quiet_threats(const Position& pos, Bitboard (&threats)[KING + 1]) {
    threats[KING] = 0;  // Royal commoners cannot capture; their safety is legality.
    if (!pos.game_of_points())
    {
        threats[PAWN] = 0;
        threats[KNIGHT] = threats[BISHOP] = pos.threats_by(PAWN);
        threats[ROOK] = pos.threats_by(KNIGHT) | pos.threats_by(BISHOP) | threats[KNIGHT];
        threats[QUEEN] = pos.threats_by(ROOK) | threats[ROOK];
        return;
    }

    Bitboard attacks[QUEEN + 1];
    for (PieceType attacker = PAWN; attacker <= QUEEN; ++attacker)
        attacks[attacker] = pos.threats_by(attacker);
    for (PieceType victim = PAWN; victim <= QUEEN; ++victim)
    {
        threats[victim] = 0;
        for (PieceType attacker = PAWN; attacker <= QUEEN; ++attacker)
            if (points_capture_threat(attacker, victim))
                threats[victim] |= attacks[attacker];
    }
}

}  // namespace Stockfish::Search
#endif
