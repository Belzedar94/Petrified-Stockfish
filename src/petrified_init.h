/*
  Petrified-Stockfish, a specialized Petrified Chess engine derived from Stockfish
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Petrified-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef PETRIFIED_INIT_H_INCLUDED
#define PETRIFIED_INIT_H_INCLUDED

namespace Stockfish {

// Initialize immutable board, attack and Zobrist tables exactly once per
// process. Every executable, binding and standalone data reader shares this
// entry point so public APIs have no hidden initialization precondition.
void initialize_petrified_core();

}  // namespace Stockfish

#endif  // PETRIFIED_INIT_H_INCLUDED
