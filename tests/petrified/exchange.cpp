// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "position.h"
#include "uci_move.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
using namespace Stockfish;

static void require(bool ok, const std::string& message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

// Slow reference: use real legal move generation and make/undo; measure the
// complete board and scoreboard before/after, rather than duplicate the fast
// bitboard exchange algorithm. This is a restricted material/points game:
// declining is always allowed, and terminal bonuses are deliberately absent.
static int balance(const Position& pos, Color us) {
    int value = 100 * (pos.points(us) - pos.points(~us));
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
        if (const Piece pc = pos.piece_on(s))
            value += (color_of(pc) == us ? 1 : -1) * PieceValue[pc];
    return value;
}

static int reference_reply(Position& pos, Square target) {
    if (!pos.piece_on(target) || type_of(pos.piece_on(target)) == KING)
        return 0;
    const Color us = pos.side_to_move();
    const int before = balance(pos, us);
    int best = 0;
    Move legal[MAX_MOVES];
    Move* end = reference_generate<ALL>(pos, legal);
    for (Move* it = legal; it != end; ++it) {
        const Move move = *it;
        const Square captured = move.type_of() == EN_PASSANT
                              ? move.to_sq() - pawn_push(us) : move.to_sq();
        if (!pos.capture(move) || captured != target) continue;
        StateInfo state;
        pos.do_move(move, state);
        const int gain = balance(pos, us) - before - reference_reply(pos, move.to_sq());
        pos.undo_move(move);
        best = std::max(best, gain);
    }
    return best;
}

static int reference_see(Position& pos, Move move) {
    if (move.type_of() == CASTLING || type_of(pos.moved_piece(move)) == KING) return 0;
    const Color us = pos.side_to_move();
    const int before = balance(pos, us);
    StateInfo state;
    pos.do_move(move, state);
    const int gain = balance(pos, us) - before - reference_reply(pos, move.to_sq());
    pos.undo_move(move);
    return gain;
}

static std::size_t positions = 0, comparisons = 0, thresholds = 0;
static void check(Position& pos, Move move, int expected = std::numeric_limits<int>::max()) {
    const auto fen = pos.fen(); const Key key = pos.key();
    const int actual = pos.points_see(move), reference = reference_see(pos, move);
    const std::string context = fen + " " + UCI::move(move) + " fast="
                              + std::to_string(actual) + " reference=" + std::to_string(reference);
    require(actual == reference, "Exchange mismatch: " + context);
    if (expected != std::numeric_limits<int>::max())
        require(actual == expected, "Hand-calculated fixture: " + context
                                   + " expected=" + std::to_string(expected));
    for (int threshold : {actual - 1, actual, actual + 1, -4096, -1000, -177, -75, -23,
                          0, 100, 500, 4096, std::numeric_limits<int>::min(),
                          std::numeric_limits<int>::max()}) {
        require(pos.see_ge(move, threshold) == (reference >= threshold), "Threshold: " + context);
        ++thresholds;
    }
    require(pos.fen() == fen && pos.key() == key && pos.pos_is_ok(), "Exchange mutated position");
    ++comparisons;
}

static void emit(std::ostream& out, const Position& pos, Move move,
                 const std::vector<std::string>* history = nullptr) {
    out << "{\"fen\":\"" << pos.fen() << "\",\"move\":\"" << UCI::move(move)
        << "\",\"value\":" << pos.points_see(move);
    if (history) {
        out << ",\"history\":[";
        for (std::size_t i = 0; i < history->size(); ++i)
            out << (i ? "," : "") << '"' << (*history)[i] << '"';
        out << ']';
    }
    out << "}\n";
}

int main(int argc, char** argv) {
    Attacks::init(); Position::init();
    if (argc == 2 && std::string(argv[1]) == "--probe") {
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto split = line.find('\t');
            Position pos; StateInfo state;
            require(split != std::string::npos && !pos.set(line.substr(0, split), false, &state), "probe FEN");
            const Move move = UCI::to_move(pos, line.substr(split + 1));
            require(move != Move::none(), "probe illegal move");
            std::cout << pos.points_see(move) << std::endl;
        }
        return 0;
    }
    std::ofstream cases;
    if (argc == 3 && std::string(argv[1]) == "--cases") cases.open(argv[2]);
    struct Fixture { const char* fen; const char* move; int value; };
    for (const auto& f : {
        Fixture{"7k/8/8/3q4/4P3/8/8/K7 w - - 0 1 points=0,0", "e4d5", 3438},
        Fixture{"7k/8/2p5/3p4/4P3/8/8/K7 w - - 0 1 points=0,0", "e4d5", 0},
        Fixture{"7k/8/2q5/3p4/4P3/8/8/K7 w - - 0 1 points=0,0", "e4d5", 308},
        Fixture{"7k/8/8/3r4/3R4/8/8/K7 w - - 0 1 points=0,0", "d4d5", 500},
        Fixture{"8/7k/8/3p4/3Q4/8/8/K7 w - - 0 1 points=0,0", "d4d5", -2230},
        Fixture{"7k/8/8/2p5/8/3Q4/8/K7 w - - 0 1 points=0,0", "d3d4", -3438},
        Fixture{"7k/8/8/5n2/8/3Q4/8/K7 w - - 0 1 points=0,0", "d3d4", -2657},
        Fixture{"2k5/8/8/2p5/8/3Q4/8/K1R5 w - - 0 1 points=0,0", "d3d4", 0},
        Fixture{"2k5/8/8/2r5/3Q4/8/8/K1R5 w - - 0 1 points=0,0", "d4d5", 0},
        Fixture{"7k/8/8/3pP3/8/8/8/K7 w - d6 0 1 points=0,0", "e5d6", 308},
        Fixture{"7k/8/8/8/3p4/8/4P3/K7 w - - 0 1 points=0,0", "e2e4", -308},
        Fixture{"7k/8/8/8/3p4/8/3PP3/K7 w - - 0 1 points=0,0", "e2e4", 0},
        Fixture{"3k4/8/8/8/3p4/8/4P3/K2R4 w - - 0 1 points=0,0", "e2e4", 0},
        Fixture{"1r5k/P7/8/8/8/8/8/7K w - - 0 1 points=0,0", "a7a8q", 168},
        Fixture{"1r5k/P7/8/8/8/8/8/7K w - - 0 1 points=0,0", "a7a8r", 568},
        Fixture{"1r5k/P7/8/8/8/8/8/7K w - - 0 1 points=0,0", "a7a8b", 568},
        Fixture{"1r5k/P7/8/8/8/8/8/7K w - - 0 1 points=0,0", "a7a8n", 573},
        Fixture{"1r5k/P2n4/8/8/8/8/8/7K w - - 0 1 points=0,0", "a7b8q", 1449},
        Fixture{"8/8/4k3/3p4/4P3/8/8/K7 w - - 0 1 points=0,0", "e4d5", 308},
        Fixture{"r6k/8/8/*7/8/8/8/1Q5K w - - 0 1 points=0,0", "b1a1", 0},
        Fixture{"r6k/8/8/8/8/8/8/1Q5K w - - 0 1 points=0,0", "b1a1", -2162},
        Fixture{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1 points=0,0", "e1g1", 0}
    }) {
        Position pos; StateInfo state;
        if (auto error = pos.set(f.fen, false, &state))
            require(false, std::string("fixture FEN: ") + f.fen + ": " + error->what());
        for (int flip = 0; flip < 2; ++flip) {
            std::string uci = f.move;
            if (flip) {
                require(!pos.flip(), "flip fixture");
                uci[1] = char('1' + '8' - uci[1]); uci[3] = char('1' + '8' - uci[3]);
            }
            Move move = UCI::to_move(pos, uci);
            require(move != Move::none(), "illegal fixture: " + pos.fen() + " " + uci);
            check(pos, move, f.value);
            if (cases && pos.ep_squares() == 0) emit(cases, pos, move);
            for (Move other : MoveList<ALL>(pos)) check(pos, other);
            ++positions;
        }
    }
    std::mt19937 rng(20261029);
    std::size_t exported = 0;
    for (int game = 0; game < 32; ++game) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=0,0",
                         false, &states.back()), "walk FEN");
        std::vector<Move> played;
        std::vector<std::string> history;
        for (int ply = 0; ply < 128; ++ply) {
            const MoveList<ALL> moves(pos);
            if (!moves.size()) break;
            if (ply % 3 == 0) {
                for (Move move : moves) {
                    check(pos, move);
                    if (cases && exported < 768 && (pos.capture(move) || comparisons % 12 == 0)) {
                        emit(cases, pos, move, &history); ++exported;
                    }
                }
                ++positions;
            }
            const Move move = moves.begin()[rng() % moves.size()];
            history.push_back(UCI::move(move)); played.push_back(move);
            states.emplace_back(); pos.do_move(move, states.back());
        }
        while (!played.empty()) {
            pos.undo_move(played.back()); played.pop_back(); states.pop_back();
            if (!played.empty() && played.size() % 8 == 0) {
                for (Move move : MoveList<ALL>(pos)) check(pos, move);
                ++positions;
            }
        }
    }
    std::cout << "Exchange positions: " << positions << "; moves: " << comparisons
              << "; thresholds: " << thresholds << "; result: PASS\n";
}
