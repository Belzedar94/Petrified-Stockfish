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

#include "benchmark.h"
#include "engine.h"
#include "numa.h"
#include "misc.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace {

// Frozen Petrified corpus, mirrored in tests/petrified/bench.epd.
const std::vector<std::string> Defaults = {
    "r1bqkbnr/pppppppp/*7/8/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 0 2",
    "r1bqkbnr/pppppppp/n7/8/2P5/5P2/PP1PP1PP/RNBQKBNR b KQkq c3c4 0 2",
    "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2",
    "rnbqkbnr/1ppppppp/8/p7/8/N4N2/PPPPPPPP/R1BQKB1R b KQkq - 1 2",
    "rnbqkbnr/p1pppppp/8/1p6/4P2P/8/PPPP1PP1/RNBQKBNR b KQkq e3e4 0 2",
    "rnbqkbnr/pp1ppppp/8/2p5/8/P1P5/1P1PPPPP/RNBQKBNR b KQkq - 0 2",
    "rnbqkbnr/pppp1ppp/4p3/8/5P2/7N/PPPPP1PP/RNBQKB1R b KQkq - 1 2",
    "rnbqkbnr/ppppp1pp/5p2/P7/8/8/1PPPPPPP/RNBQKBNR b KQkq - 0 2",
    "rnbqkbnr/pppppp1p/8/6p1/8/2P5/PPP1PPPP/RNBQKBNR b KQkq - 1 2",
    "rnbqkbnr/ppppppp1/8/P6p/8/8/1PPPPPPP/RNBQKBNR b KQkq - 0 2",
    "4k2r/6P1/8/8/8/8/8/4K3 w - - 0 1",
    "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
    "8/8/8/8/8/8/3*k*2/3*K*2 w - - 0 1",
    "4k3/8/8/8/8/8/3***2/3*K*2 w - - 0 1",
};
const std::vector<std::vector<std::string>> BenchmarkPositions = {Defaults};

}  // namespace

namespace Stockfish::Benchmark {

// Builds a list of UCI commands to be run by bench. There
// are five parameters: TT size in MB, number of search threads that
// should be used, the limit value spent for each position, a file name
// where to look for positions in FEN format, and the type of the limit:
// depth, perft, nodes and movetime (in milliseconds). Examples:
//
// bench                            : search default positions up to depth 10
// bench 64 1 15                    : search default positions up to depth 15 (TT = 64MB)
// bench 64 1 100000 default nodes  : search default positions for 100K nodes each
// bench 64 4 5000 current movetime : search current position with 4 threads for 5 sec
// bench 16 1 5 blah perft          : run a perft 5 on positions in file "blah"
std::vector<std::string> setup_bench(const std::string& currentFen, std::istream& is) {

    std::vector<std::string> fens, list;
    std::string              go, token;

    // Assign default values to missing arguments
    std::string ttSize    = (is >> token) ? token : "16";
    std::string threads   = (is >> token) ? token : "1";
    std::string limit     = (is >> token) ? token : "10";
    std::string fenFile   = (is >> token) ? token : "default";
    std::string limitType = (is >> token) ? token : "depth";

    go = limitType == "eval" ? "eval" : "go " + limitType + " " + limit;

    if (fenFile == "default")
        fens = Defaults;

    else if (fenFile == "current")
        fens.push_back(currentFen);

    else
    {
        std::string   fen;
        std::ifstream file(fenFile);

        if (!file.is_open())
        {
            std::cerr << "Unable to open file " << fenFile << std::endl;
            exit(EXIT_FAILURE);
        }

        while (getline(file, fen))
            if (!fen.empty())
                fens.push_back(fen);
    }

    list.emplace_back("setoption name Threads value " + threads);
    list.emplace_back("setoption name Hash value " + ttSize);
    list.emplace_back("ucinewgame");

    for (const std::string& fen : fens)
        if (fen.find("setoption") != std::string::npos)
            list.emplace_back(fen);
        else
        {
            list.emplace_back("position fen " + fen);
            list.emplace_back(go);
        }

    return list;
}

BenchmarkSetup setup_benchmark(std::istream& is) {
    // TT_SIZE_PER_THREAD is chosen such that roughly half of the hash is used all positions
    // for the current sequence have been searched.
    static constexpr int TT_SIZE_PER_THREAD = 128;

    static constexpr int DEFAULT_DURATION_S = 150;

    static constexpr int MaxDurationS = std::numeric_limits<int>::max() / 1000;

    BenchmarkSetup setup{};

    auto clamped = [](const char* what, i64 value, i64 lo, i64 hi) {
        const i64 fixed = std::clamp(value, lo, hi);
        if (fixed != value)
            std::cerr << "info string speedtest: " << what << ' ' << value << " is outside [" << lo
                      << ", " << hi << "]; using " << fixed << std::endl;
        return int(fixed);
    };

    // Assign default values to missing arguments
    i64 desiredTimeS;
    i64 requested;

    if (!(is >> requested))
        setup.threads = int(get_hardware_concurrency());
    else
    {
        setup.threads = clamped("threads", requested, 1, MaxThreads);
        setup.originalInvocation += std::to_string(setup.threads);
    }

    if (!(is >> requested))
        setup.ttSize = clamped("hash", i64(TT_SIZE_PER_THREAD) * setup.threads, 1, MaxHashMB);
    else
    {
        setup.ttSize = clamped("hash", requested, 1, MaxHashMB);
        setup.originalInvocation += " " + std::to_string(setup.ttSize);
    }

    if (!(is >> desiredTimeS))
        desiredTimeS = DEFAULT_DURATION_S;
    else
    {
        desiredTimeS = clamped("seconds", desiredTimeS, 1, MaxDurationS);
        setup.originalInvocation += " " + std::to_string(desiredTimeS);
    }

    setup.filledInvocation += std::to_string(setup.threads) + " " + std::to_string(setup.ttSize)
                            + " " + std::to_string(desiredTimeS);

    auto getCorrectedTime = [&](int ply) {
        // time per move is fit roughly based on LTC games
        // seconds = 50/{ply+15}
        // ms = 50000/{ply+15}
        // with this fit 10th move gets 2000ms
        // adjust for desired 10th move time
        return 50000.0 / (static_cast<double>(ply) + 15.0);
    };

    float totalTime = 0;
    for (const auto& game : BenchmarkPositions)
        for (usize i = 0; i < game.size(); ++i)
            totalTime += float(getCorrectedTime(int(i + 1)));

    float timeScaleFactor = static_cast<float>(desiredTimeS * 1000) / totalTime;

    for (const auto& game : BenchmarkPositions)
    {
        setup.commands.emplace_back("ucinewgame");
        int ply = 1;
        for (const std::string& fen : game)
        {
            setup.commands.emplace_back("position fen " + fen);
            const int correctedTime = static_cast<int>(getCorrectedTime(ply++) * timeScaleFactor);
            setup.commands.emplace_back("go movetime " + std::to_string(correctedTime));
        }
    }

    return setup;
}

}  // namespace Stockfish::Benchmark
