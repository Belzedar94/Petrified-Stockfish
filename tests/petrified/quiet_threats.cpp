// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "search_quiet.h"
#include "api/petrified_outcome.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;
static void require(bool ok, const char* why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
// Independently enumerated profitable attacker sets in victim order P,N,B,R,Q,K.
static const char* allowed[] = {"", "P", "PNB", "PNB", "PNBR", "PNBRQ", ""};
static const char* legacy[] = {"", "", "P", "P", "PNB", "PNBR", ""};
static char symbol(PieceType pt) { return " PNBRQK"[pt]; }

// Coordinate walker: no engine attack tables, xattacks_by or capture helper.
static Bitboard reference(const Position& pos, PieceType victim) {
    Bitboard result = 0;
    const std::string permitted = (pos.game_of_points() ? allowed : legacy)[victim];
    for (Square from = SQ_A1; from <= SQ_H8; ++from) {
        Piece pc = pos.piece_on(from);
        if (!pc || color_of(pc) == pos.side_to_move()
            || permitted.find(symbol(type_of(pc))) == std::string::npos) continue;
        for (int dx = -2; dx <= 2; ++dx) for (int dy = -2; dy <= 2; ++dy) {
            PieceType pt = type_of(pc);
            bool leap = pt == PAWN || pt == KNIGHT;
            bool direction = pt == PAWN ? std::abs(dx) == 1 && dy == (color_of(pc) == WHITE ? 1 : -1)
              : pt == KNIGHT ? std::abs(dx) * std::abs(dy) == 2
              : (std::abs(dx) <= 1 && std::abs(dy) <= 1 && (dx || dy)
                 && (pt == QUEEN || (pt == ROOK && (!dx || !dy)) || (pt == BISHOP && dx && dy)));
            if (!direction) continue;
            for (int n = 1; n <= (leap ? 1 : 7); ++n) {
                int x = int(file_of(from)) + n * dx, y = int(rank_of(from)) + n * dy;
                if (x < 0 || x > 7 || y < 0 || y > 7) break;
                Square to = Square(y * 8 + x); result |= square_bb(to);
                // Existing maps deliberately x-ray through the defending king.
                bool transparent = pos.piece_on(to) == make_piece(pos.side_to_move(), KING);
                if (!transparent && (pos.occupied() & to)) break;
            }
        }
    }
    return result;
}
static std::size_t positions=0, maps=0, quiets=0, children=0, exported=0;
static void inspect(Position& pos, std::ofstream& out, bool emit) {
    const std::string fen = pos.fen(); const Key key = pos.key();
    Bitboard actual[KING+1] = {}; Search::quiet_threats(pos, actual);
    Bitboard expected[KING+1] = {};
    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        expected[pt] = reference(pos, pt);
        require(actual[pt] == expected[pt], "quiet threat coordinate reference differs"); ++maps;
    }
    if (emit && out && pos.game_of_points()) {
        out << "{\"fen\":\"" << fen << "\",\"maps\":[";
        for (PieceType pt = PAWN; pt <= KING; ++pt) out << (pt == PAWN ? "" : ",") << '"' << actual[pt] << '"';
        out << "]}\n"; ++exported;
    }
    for (Move m : MoveList<ALL>(pos)) {
        if (!pos.capture_stage(m)) {
            PieceType pt = type_of(pos.moved_piece(m));
            int a = bool(actual[pt] & m.from_sq()) - bool(actual[pt] & m.to_sq());
            int b = bool(expected[pt] & m.from_sq()) - bool(expected[pt] & m.to_sq());
            require(a == b, "quiet threat escape/arrival differs"); ++quiets;
        }
        StateInfo child; pos.do_move(m, child);
        Bitboard next[KING+1] = {}; Search::quiet_threats(pos, next);
        for (PieceType pt = PAWN; pt <= KING; ++pt) {
            require(next[pt] == reference(pos, pt), "child stale threat cache"); ++maps;
        }
        ++children; pos.undo_move(m);
        require(pos.fen() == fen && pos.key() == key, "threat query altered state/undo");
        Bitboard restored[KING+1] = {}; Search::quiet_threats(pos, restored);
        for (PieceType pt = PAWN; pt <= KING; ++pt) require(restored[pt] == actual[pt], "parent cache changed");
    }
    ++positions;
}
int main(int argc, char** argv) {
    Attacks::init(); Position::init(); std::ofstream out;
    if (argc == 2) { out.open(argv[1]); require(bool(out), "export unavailable"); }
    for (PieceType a = NO_PIECE_TYPE; a <= KING; ++a)
        for (PieceType v = NO_PIECE_TYPE; v <= KING; ++v)
            require(Search::points_capture_threat(a,v) == (a >= PAWN && a <= QUEEN
                && std::string(allowed[v]).find(symbol(a)) != std::string::npos), "capture economy matrix");
    for (const char* board : {
        "4k3/8/8/3n4/3N4/8/8/4K3 w - - 0 1",
        "4k3/8/8/3b4/3N4/8/8/4K3 w - - 0 1",
        "4k3/8/8/3r4/3R4/8/8/4K3 w - - 0 1",
        "4k3/8/8/3q4/3Q4/8/8/4K3 w - - 0 1",
        "4k3/8/8/3p4/2P5/8/8/4K3 w - - 0 1",
        "4k3/8/2*5/3n4/5*2/8/8/3RK3 w - - 99 1",
        "4k3/4n3/8/8/8/8/8/K3R3 w - - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "4k3/1P6/8/8/8/8/6p1/4K3 w - - 0 1"
    }) for (const char* suffix : {"", " points=0,0", " points=19,21"}) {
        Position pos; StateInfo state; require(!pos.set(std::string(board)+suffix,false,&state), "invalid threat fixture");
        inspect(pos,out,true);
    }
    std::mt19937 rng(20261030);
    for (int game = 0; game < 24; ++game) {
        Position pos; std::vector<StateInfo> states(161); std::vector<Move> played;
        std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        if (game % 4) start += " points=0,0";
        require(!pos.set(start,false,&states[0]),"startpos");
        for (int ply=0; ply<160; ++ply) {
            if (pos.game_of_points() && Petrified::outcome(pos,true).terminal()) break;
            inspect(pos,out,game<8 && ply%7==0);
            MoveList<ALL> moves(pos); if (!moves.size()) break;
            Move m=moves.begin()[rng()%moves.size()]; played.push_back(m); pos.do_move(m,states[ply+1]);
        }
        while (!played.empty()) { pos.undo_move(played.back()); played.pop_back(); }
        require(pos.fen()==start,"walk undo");
    }
    std::cout << "{\"result\":\"PASS\",\"positions\":" << positions << ",\"map_comparisons\":" << maps
      << ",\"quiet_moves\":" << quiets << ",\"children\":" << children << ",\"exported\":" << exported << "}\n";
}
