// SPDX-License-Identifier: GPL-3.0-or-later
#include "petrified_fortress.h"

#include "bitboard.h"
#include "movegen.h"
#include "position.h"

namespace Stockfish::Petrified {
namespace {

// Eight-neighbour reachability deliberately overestimates every king/slider.
// A fatal capture replaces a defending pawn by a wall, preserving this barrier.
Bitboard region(Bitboard seeds, Bitboard blocked) {
    Bitboard reached = seeds & ~blocked, frontier = reached;
    while (frontier) {
        Bitboard next = 0;
        while (frontier) next |= attacks_bb<KING>(pop_lsb(frontier));
        frontier = next & ~blocked & ~reached;
        reached |= frontier;
    }
    return reached;
}

// Quiet pawn reachability, ignoring enemy occupancy and pins. Captures are
// intercepted on the preceding defender turn, as verified separately below.
Bitboard pawn_region(Square start, Color side, Bitboard blocked) {
    Bitboard reached = square_bb(start), frontier = reached;
    while (frontier) {
        Bitboard next = 0;
        while (frontier) {
            Square from = pop_lsb(frontier);
            if (file_of(from) != FILE_A) next |= from + WEST;
            if (file_of(from) != FILE_H) next |= from + EAST;
            Square one = from + pawn_push(side);
            if (is_ok(one) && !(blocked & one)) {
                next |= one;
                Square two = one + pawn_push(side);
                if (relative_rank(side, from) == RANK_2 && !(blocked & two)) next |= two;
            }
        }
        frontier = next & ~blocked & ~reached;
        reached |= frontier;
    }
    return reached;
}

std::optional<Fortress> recognize(const Position& pos, Color defender) {
    const Color attacker = ~defender;
    if (pos.pieces(defender, QUEEN, ROOK, BISHOP)
        || pos.count<KNIGHT>(defender) > 1 || pos.count<KNIGHT>(attacker) > 1
        || pos.count<PAWN>(attacker) > 1)
        return {};

    // Sliders can collect one pawn before disappearing. Only the invading
    // knight can reach the stationary defending knight (worth three points).
    // The single attacking pawn is intercepted before any capture, or trapped.
    const int budget = popcount(pos.pieces(attacker, QUEEN, ROOK, BISHOP))
                     + pos.count<KNIGHT>(attacker) * (pos.count<KNIGHT>(defender) ? 3 : 1);
    const int margin = pos.points(defender) - pos.points(attacker) - budget;
    if (margin <= 0) return {};

    const Square king = pos.square<KING>(defender);
    const Bitboard pawns = pos.pieces(defender, PAWN);
    const Bitboard barrier = pos.walls() | pawns;
    const Bitboard inside = region(square_bb(king), barrier);
    const Bitboard otherKnight = pos.pieces(attacker, KNIGHT);
    if ((inside & pos.pieces(attacker) & ~otherKnight & ~pos.pieces(attacker, PAWN))
        || (pos.pieces(defender, KNIGHT) & ~inside))
        return {};

    // An empty 2x2 refuge always gives a king an escape from one knight,
    // even when the knight occupies a refuge square. No other attacker can
    // enter this component. The defending knight never moves into the refuge.
    Bitboard refuge = 0;
    for (int rank = int(rank_of(king)) - 1; rank <= int(rank_of(king)); ++rank)
        for (int file = int(file_of(king)) - 1; file <= int(file_of(king)); ++file) {
            if (rank < 0 || rank >= 7 || file < 0 || file >= 7) continue;
            Square a = make_square(File(file), Rank(rank));
            Bitboard box = square_bb(a) | square_bb(a + EAST)
                         | square_bb(a + NORTH) | square_bb(a + NORTH_EAST);
            if (!(box & ~inside)
                && !(box & pos.occupied() & ~square_bb(king) & ~otherKnight)) {
                refuge = box;
                break;
            }
        }
    if (!refuge) return {};

    Move intercept = Move::none();
    if (pos.pieces(attacker, PAWN)) {
        Square pawn = lsb(pos.pieces(attacker, PAWN));
        // This first family excludes double pushes and their en-passant state.
        if (relative_rank(attacker, pawn) == RANK_2) return {};
        const Bitboard reach = pawn_region(pawn, attacker, barrier);
        if ((reach & inside) || (reach & (attacker == WHITE ? Rank8BB : Rank1BB))) return {};
        // Every remaining pawn which could intercept must be safe: another
        // interceptor might already have been replaced by a wall. Such changes
        // only shrink this graph; all the other barrier squares stay occupied.
        for (Bitboard b = reach; b;) {
            Square to = pop_lsb(b);
            Bitboard froms = attacks_bb(PAWN, to, attacker) & pawns;
            while (froms) {
                Square from = pop_lsb(froms);
                Bitboard afterBarrier = (barrier & ~square_bb(from)) | square_bb(to);
                if (relative_rank(defender, to) == RANK_8
                    || (region(square_bb(king), afterBarrier) & ~(inside | square_bb(from))))
                    return {};
                if (to == pawn && pos.side_to_move() == defender) {
                    Move capture(from, to);
                    if (contains_legal_move(pos, capture)) intercept = capture;
                }
            }
        }
        if (attacks_bb(PAWN, pawn, attacker) & pawns) {
            // Already exposed with the attacker to move is not covered by
            // pre-emption. Nor is a position where a separate check prevents it.
            if (pos.side_to_move() != defender || intercept == Move::none()) return {};
        }
    }

    Move defence = intercept;
    if (pos.side_to_move() == defender && defence == Move::none()) {
        for (Move move : MoveList<ALL>(pos))
            if (move.from_sq() == king && (refuge & move.to_sq())
                && move.type_of() == NORMAL) {
                defence = move;
                break;
            }
        if (defence == Move::none()) return {};
    }
    // Opponent-to-move FENs with the defending king already in knight check
    // are outside the invariant (and cannot follow the defensive policy).
    if (pos.side_to_move() == attacker && otherKnight
        && (attacks_bb<KNIGHT>(lsb(otherKnight)) & king)) return {};

    return Fortress{defender, refuge, margin, defence};
}
} // namespace

std::optional<Fortress> fortress(const Position& pos) {
    if (!pos.game_of_points() || !pos.walls() || pos.ep_squares()
        || pos.state()->castlingRights || (pos.pieces(PAWN) & (Rank1BB | Rank8BB)))
        return {};
    for (Color defender : {WHITE, BLACK})
        if (auto proof = recognize(pos, defender)) return proof;
    return {};
}
} // namespace Stockfish::Petrified
