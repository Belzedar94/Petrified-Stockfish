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

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"

namespace Stockfish {

static Value scale_evaluation(Value nnue, int, const Position& pos, const Eval::NNUE::Network& network) {
    const int material=Eval::NNUE::legacy_material(pos,WHITE)+Eval::NNUE::legacy_material(pos,BLACK);
    const int scale=903+32*material/1024;
    Value v=nnue*scale/1024;
    v=v*(100-std::min(pos.rule50_count(),100))/100;
    if (pos.game_of_points())
    {
        // run4test has no points inputs. This provisional correction values
        // earned points without changing the frozen NNUE backend/scale.
        const Color us = pos.side_to_move();
        const Value adjustment=network.points_adjustment(pos,v);
        v += 100 * std::clamp(pos.points(us) - pos.points(~us), -200, 200) + adjustment;
    }
    return std::clamp(v,VALUE_TB_LOSS_IN_MAX_PLY+1,VALUE_TB_WIN_IN_MAX_PLY-1);
}
Value Eval::evaluate(const NNUE::Network& network, const Position& pos,
                     NNUE::AccumulatorStack& stack, NNUE::AccumulatorCaches& cache, int optimism) {
    assert(!pos.checkers());
    return scale_evaluation(network.evaluate(pos,stack,cache),optimism,pos,network);
}
std::string Eval::trace(Position& pos, const NNUE::Network& network) {
    auto cache=std::make_unique<NNUE::AccumulatorCaches>(network);
    auto stack=std::make_unique<NNUE::AccumulatorStack>();
    const Value raw=network.evaluate(pos,*stack,*cache);
    std::ostringstream out;
    out<<"Legacy raw adjusted: "<<raw<<"\nLegacy reference: "<<network.evaluate_reference(pos)
       <<"\nLegacy raw unadjusted: "<<network.evaluate_reference(pos,false)
       <<"\nLegacy final: "<<scale_evaluation(raw,0,pos,network)<<"\n";
    if (network.has_points_adapter()) {
        const int material=Eval::NNUE::legacy_material(pos,WHITE)+Eval::NNUE::legacy_material(pos,BLACK);
        const Value boardValue=(raw*(903+32*material/1024)/1024)*(100-std::min(pos.rule50_count(),100))/100;
        out<<"Points adapter: "<<network.points_adjustment(pos,boardValue)<<"\nPoints features:";
        for (auto x:Eval::NNUE::points_features(pos,boardValue)) out<<' '<<x;
        out<<'\n';
    }
    return out.str();
}
} // namespace Stockfish
