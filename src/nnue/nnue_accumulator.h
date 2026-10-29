/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NNUE_ACCUMULATOR_H_INCLUDED
#define NNUE_ACCUMULATOR_H_INCLUDED
#include <array>
#include <cassert>
#include "../types.h"
namespace Stockfish { class Position; }
namespace Stockfish::Eval::NNUE {
struct AccumulatorCaches {
    struct alignas(64) Entry {
        std::array<i16,512> sums{};
        std::array<i32,8> psqt{};
        std::array<Piece,64> board{};
        Bitboard walls = 0;
        bool valid = false;
        bool pointsWall = false;
    };
    std::array<std::array<Entry,64>,2> entries;
    template<typename N> explicit AccumulatorCaches(const N&) { clear(); }
    void clear() { for(auto& side:entries) for(auto& e:side) e.valid=false; }
    template<typename N> void clear(const N&) { clear(); }
};
// Search still owns its move dirties. Legacy refresh caches are independent of
// the modern SFNNv19 feature implementation retained for the next architecture.
class AccumulatorStack {
    std::array<Dirties,MAX_PLY+1> dirties;
    usize size = 0;
public:
    void reset() { size=0; }
    Dirties& push(const Position&) { assert(size<dirties.size()); return dirties[size++]; }
    void pop() { assert(size); --size; }
};
}
#endif
