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

#include "movegen.h"
#include "position.h"
#include "attacks.h"

namespace Stockfish {
using namespace Attacks;

template<GenType Type>
Move* generate(const Position& pos, Move* out) {
    const Color us = pos.side_to_move();
    const Bitboard enemies = pos.pieces(~us) & ~pos.pieces(KING);
    const Bitboard empty = ~pos.occupied();
    auto emit = [&](Move m) {
        const bool stage = pos.capture_stage(m);
        if ((Type == CAPTURES && !stage) || (Type == QUIETS && stage)) return;
        if (pos.safe_after(m)) *out++ = m;
    };
    auto pawn = [&](Square from, Square to) {
        if (relative_rank(us, to) == RANK_8)
            for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
                emit(Move::make<PROMOTION>(from, to, pt));
        else emit(Move(from, to));
    };
    for (Bitboard b = pos.pieces(us, PAWN); b;) {
        const Square from = pop_lsb(b);
        const Square one = from + pawn_push(us);
        if (is_ok(one) && pos.empty(one)) {
            pawn(from, one);
            const Square two = one + pawn_push(us);
            if (relative_rank(us, from) == RANK_2 && pos.empty(two)) emit(Move(from, two));
        }
        if (file_of(from) != FILE_A && pos.empty(from + WEST)) emit(Move(from, from + WEST));
        if (file_of(from) != FILE_H && pos.empty(from + EAST)) emit(Move(from, from + EAST));
        for (Bitboard targets = attacks_bb(PAWN, from, us) & enemies; targets;)
            pawn(from, pop_lsb(targets));
        for (Bitboard ep = attacks_bb(PAWN, from, us) & pos.ep_squares() & empty; ep;) {
            const Square to = pop_lsb(ep), cs = to - pawn_push(us);
            if (is_ok(cs) && pos.piece_on(cs) == make_piece(~us, PAWN))
                emit(Move::make<EN_PASSANT>(from, to));
        }
    }
    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING})
        for (Bitboard b = pos.pieces(us, pt); b;) {
            const Square from = pop_lsb(b);
            Bitboard targets = attacks_bb(pt, from, pos.occupied()) & (empty | (pt == KING ? 0 : enemies));
            while (targets) emit(Move(from, pop_lsb(targets)));
        }
    if (Type != CAPTURES && !pos.checkers())
        for (CastlingRights cr : {us & KING_SIDE, us & QUEEN_SIDE})
            if (pos.can_castle(cr) && !pos.castling_impeded(cr))
                emit(Move::make<CASTLING>(pos.square<KING>(us), pos.castling_rook_square(cr)));
    return out;
}
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<EVASIONS>(const Position&, Move*);
template Move* generate<ALL>(const Position&, Move*);
} // namespace Stockfish
