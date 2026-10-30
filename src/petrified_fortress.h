// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PETRIFIED_FORTRESS_H_INCLUDED
#define PETRIFIED_FORTRESS_H_INCLUDED

#include <optional>
#include "types.h"

namespace Stockfish {
class Position;
namespace Petrified {

// A constructive points win, not a terminal rule or an immediate claim.
// Unknown includes equal/losing point bounds and every unsupported geometry.
struct Fortress {
    Color defender;
    Bitboard refuge;
    int pointMargin;
    Move defence; // Legal policy move only when the defender is to move.
    Value value(Color turn, int ply) const {
        return turn == defender ? VALUE_TB - ply : -VALUE_TB + ply;
    }
};

std::optional<Fortress> fortress(const Position& pos);

} // namespace Petrified
} // namespace Stockfish
#endif
