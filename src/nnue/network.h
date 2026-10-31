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

#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "../types.h"
#include "nnue_misc.h"
#include "points_adapter.h"
namespace Stockfish { class Position; }
namespace Stockfish::Eval::NNUE {
class AccumulatorStack;
struct AccumulatorCaches;
class Network {
public:
    // Training export: transformer and PSQT stay frozen in the embedded run4test.
    struct HeadInput {
        std::array<u8,1056> input{};
        i32 bucket, psqt, entertainment, scale, rule50, difference;
    };
    HeadInput head_input(const Position&) const;
    void load(const std::filesystem::path&, std::filesystem::path, EvalFile&);
    bool save(const EvalFile&, const std::optional<std::filesystem::path>&) const;
    usize get_content_hash() const { return initialized ? usize(contentHash) : 0; }
    Value points_adjustment(const Position& pos, Value boardValue) const { return adapter.evaluate(pos,boardValue); }
    bool has_points_adapter() const { return adapter.active; }
    Value evaluate(const Position&, AccumulatorStack&, AccumulatorCaches&) const;
    Value evaluate_reference(const Position&, bool adjusted = true) const;
    void verify(const std::function<void(std::string_view)>&, const EvalFile&, std::filesystem::path) const;
private:
    struct Head {
        i32 bias1[16]; i8 weight1[16][1024];
        i32 bias2[32]; i8 weight2[32][32]; // 16 inputs padded to 32 on disk
        i32 bias3; i8 weight3[32];
    };
    struct PointsHead { Head board; i8 scoreWeight[16][32]; };
    alignas(64) i16 biases[512];
    alignas(64) i16 weights[49152][512];
    alignas(64) i32 psqtWeights[49152][8];
    Head heads[8];
    PointsHead pointsHeads[8];
    bool pointsHeadActive = false;
    bool initialized = false;
    PointsAdapter adapter;
    std::array<char,65> networkSha{};
    u64 contentHash=0;
    bool read_file(const std::filesystem::path&, EvalFile&);
    void update(const Position&, Color, AccumulatorCaches&, bool reference = false) const;
    Value forward(const Position&, const AccumulatorCaches&, bool adjusted, bool scalar = false) const;
};
int legacy_material(const Position&, Color);
}
template<> struct std::hash<Stockfish::Eval::NNUE::Network> {
    Stockfish::usize operator()(const Stockfish::Eval::NNUE::Network& n) const noexcept { return n.get_content_hash(); }
};
#endif
