// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_CAPTURE_H_INCLUDED
#define SEARCH_CAPTURE_H_INCLUDED

#include "position.h"

namespace Stockfish::Search {

// Immediate material/earned-points transaction for capture depth allocation.
// This is not SEE: it excludes replies, settlement bonuses and wall geometry.
// Pass the original mover and the actual captured piece (including en passant).
inline int capture_lmr_value(bool points, Piece moved, Piece captured, Move move) {
    int value = PieceValue[captured];
    if (!points)
        return value;

    value += 100 * Position::capture_points(type_of(captured));
    if (captured && type_of(moved) != PAWN)
        value -= PieceValue[moved];
    if (move.type_of() == PROMOTION)
        value += PieceValue[move.promotion_type()] - PieceValue[PAWN];
    return value;
}

}  // namespace Stockfish::Search
#endif
