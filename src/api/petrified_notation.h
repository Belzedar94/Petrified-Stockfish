/*
  Petrified-Stockfish, a specialized Petrified Chess engine derived from Stockfish
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Petrified-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef PETRIFIED_NOTATION_H_INCLUDED
#define PETRIFIED_NOTATION_H_INCLUDED

#include <string>
#include <string_view>

#include "../types.h"

namespace Stockfish {

class Position;

namespace Petrified {

// Format and parse legal Petrified moves. Position is non-const because check and
// mate suffixes are determined by a reversible make/undo of the candidate.
std::string to_san(Position& pos, Move move);
std::string to_lan(Position& pos, Move move);
Move        parse_san(Position& pos, std::string_view notation);
Move        parse_lan(Position& pos, std::string_view notation);

}  // namespace Petrified
}  // namespace Stockfish

#endif  // PETRIFIED_NOTATION_H_INCLUDED
