/*
  Petrified-Stockfish, a specialized Petrified Chess engine derived from Stockfish
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Petrified-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#include "petrified_init.h"

#include <mutex>

#include "attacks.h"
#include "bitboard.h"
#include "position.h"

namespace Stockfish {

void initialize_petrified_core() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        Attacks::init();
        Position::init();
    });
}

}  // namespace Stockfish
