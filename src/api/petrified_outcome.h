/*
  Petrified-Stockfish, a specialized Petrified Chess engine derived from Stockfish
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Petrified-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef PETRIFIED_OUTCOME_H_INCLUDED
#define PETRIFIED_OUTCOME_H_INCLUDED

#include <optional>
#include <array>
#include <string>

#include "../types.h"

namespace Stockfish {

class Position;

namespace Petrified {

enum class Termination {
    Ongoing,
    Checkmate,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition,
    Resignation,
    Timeout,
    Agreement
};

struct Outcome {
    Termination          termination = Termination::Ongoing;
    Value                value       = VALUE_ZERO;  // Relative to side to move.
    std::optional<Color> winner;
    std::array<int, COLOR_NB> points{};

    bool        terminal() const { return termination != Termination::Ongoing; }
    std::string result() const;
    std::string reason() const;
};

Outcome  outcome(const Position& pos, bool claimDraw = false, int repetitionPly = 0);
bool     has_insufficient_material(Color color, const Position& pos);
Bitboard checked_squares(const Position& pos);
Outcome settle_points(const Position& pos, Termination reason, Color actor, int ply = 0);
Value resignation_value(const Position& pos, int ply = 0);
bool insufficient_material(const Position& pos);
std::string points_status(const Position& pos);

}  // namespace Petrified
}  // namespace Stockfish

#endif  // PETRIFIED_OUTCOME_H_INCLUDED
