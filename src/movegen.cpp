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

namespace {
template<GenType Type, bool FirstOnly>
Move* generate_impl(const Position& pos, Move* out, Move wanted = Move::none()) {
    const Color us = pos.side_to_move();
    const Bitboard enemies = pos.pieces(~us) & ~pos.pieces(KING);
    const Bitboard empty = ~pos.occupied();
    // A generated non-king move leaves its destination occupied by the mover
    // or a new wall. Outside check, only vacating a king blocker can expose an
    // attack. En passant removes a second square and always needs the full test.
    const Bitboard safetyOrigins = pos.checkers() ? ~Bitboard(0)
        : pos.blockers_for_king(us) | pos.pieces(us, KING);
    auto emit = [&](Move m) {
        if constexpr (FirstOnly)
            if (wanted != Move::none() && m != wanted) return false;
        const bool stage = pos.capture_stage(m);
        if ((Type == CAPTURES && !stage) || (Type == QUIETS && stage)) return false;
        if (((safetyOrigins & m.from_sq()) || m.type_of() == EN_PASSANT)
            && !pos.safe_after(m)) return false;
        *out++ = m;
        return FirstOnly;
    };
    auto pawn = [&](Square from, Square to) {
        if (relative_rank(us, to) == RANK_8) {
            for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
                if (emit(Move::make<PROMOTION>(from, to, pt))) return true;
        }
        else if (emit(Move(from, to))) return true;
        return false;
    };
    for (Bitboard b = pos.pieces(us, PAWN); b;) {
        const Square from = pop_lsb(b);
        const Square one = from + pawn_push(us);
        if (is_ok(one) && pos.empty(one)) {
            if (pawn(from, one)) return out;
            const Square two = one + pawn_push(us);
            if (relative_rank(us, from) == RANK_2 && pos.empty(two)
                && emit(Move(from, two))) return out;
        }
        // Chess.com's Sideways modifier disallows every lateral move by a
        // slider-pinned pawn, even a move that stays on the pin ray. Legacy
        // Walls permits such a move when the resulting king remains safe.
        if (!pos.game_of_points() || !(pos.blockers_for_king(us) & from)) {
            if (file_of(from) != FILE_A && pos.empty(from + WEST)
                && emit(Move(from, from + WEST))) return out;
            if (file_of(from) != FILE_H && pos.empty(from + EAST)
                && emit(Move(from, from + EAST))) return out;
        }
        for (Bitboard targets = attacks_bb(PAWN, from, us) & enemies; targets;)
            if (pawn(from, pop_lsb(targets))) return out;
        for (Bitboard ep = attacks_bb(PAWN, from, us) & pos.ep_squares() & empty; ep;) {
            const Square to = pop_lsb(ep), cs = to - pawn_push(us);
            if (is_ok(cs) && pos.piece_on(cs) == make_piece(~us, PAWN)
                && emit(Move::make<EN_PASSANT>(from, to))) return out;
        }
    }
    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING})
        for (Bitboard b = pos.pieces(us, pt); b;) {
            const Square from = pop_lsb(b);
            Bitboard targets = attacks_bb(pt, from, pos.occupied()) & (empty | (pt == KING ? 0 : enemies));
            while (targets)
                if (emit(Move(from, pop_lsb(targets)))) return out;
        }
    if (Type != CAPTURES && !pos.checkers())
        for (CastlingRights cr : {us & KING_SIDE, us & QUEEN_SIDE})
            if (pos.can_castle(cr) && !pos.castling_impeded(cr)
                && emit(Move::make<CASTLING>(pos.square<KING>(us), pos.castling_rook_square(cr))))
                return out;
    return out;
}
} // namespace

template<GenType Type>
Move* generate(const Position& pos, Move* out) {
    return generate_impl<Type, false>(pos, out);
}

bool has_legal_move(const Position& pos) {
    Move first;
    return generate_impl<ALL, true>(pos, &first) != &first;
}

bool contains_legal_move(const Position& pos, Move move) {
    Move first;
    return move.is_ok() && generate_impl<ALL, true>(pos, &first, move) != &first;
}
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<EVASIONS>(const Position&, Move*);
template Move* generate<ALL>(const Position&, Move*);
} // namespace Stockfish
