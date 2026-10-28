// SPDX-License-Identifier: GPL-3.0-or-later
#include "petrified_outcome.h"
#include "../position.h"
#include "../movegen.h"
#include <sstream>

namespace Stockfish::Petrified {
std::string Outcome::result() const {
    return !terminal() ? "*" : !winner ? "1/2-1/2" : *winner == WHITE ? "1-0" : "0-1";
}
std::string Outcome::reason() const {
    switch (termination) {
    case Termination::Ongoing: return "ongoing";
    case Termination::Checkmate: return "checkmate";
    case Termination::Stalemate: return "stalemate";
    case Termination::InsufficientMaterial: return "insufficient-material";
    case Termination::FiftyMoveRule: return "fifty-move-rule";
    case Termination::ThreefoldRepetition: return "threefold-repetition";
    case Termination::Resignation: return "resignation";
    case Termination::Timeout: return "timeout";
    case Termination::Agreement: return "agreement";
    }
    return "unknown";
}

// Chess.com's two-player FFA test: walls make a lone bishop/knight sufficient.
// Preserve the pinned Walls COMMONER contract in the legacy variant.
bool has_insufficient_material(Color c, const Position& pos) {
    if (!pos.game_of_points()) return false;
    const int count = popcount(pos.pieces(c));
    return count == 1 || (count == 2 && !pos.walls() && pos.pieces(c, KNIGHT, BISHOP));
}
bool insufficient_material(const Position& pos) {
    if (!has_insufficient_material(WHITE, pos) || !has_insufficient_material(BLACK, pos)) return false;
    // The client defers this ending when the player to move can still capture.
    for (Move move : MoveList<ALL>(pos)) if (pos.capture(move)) return false;
    return true;
}
Bitboard checked_squares(const Position& pos) {
    return (pos.game_of_points() ? bool(pos.checkers()) : pos.reported_check())
           ? pos.pieces(pos.side_to_move(), KING) : 0;
}

Outcome settle_points(const Position& pos, Termination reason, Color actor, int ply) {
    Outcome result;
    result.termination = reason;
    result.points = {pos.points(WHITE), pos.points(BLACK)};
    int bonus[COLOR_NB] = {10, 10};
    if (reason == Termination::Checkmate
        || ((reason == Termination::Resignation || reason == Termination::Timeout)
            && !has_insufficient_material(~actor, pos))) {
        bonus[actor] = 0;
        bonus[~actor] = 20;
    }
    for (Color c : {WHITE, BLACK}) result.points[c] += bonus[c];
    if (result.points[WHITE] != result.points[BLACK])
        result.winner = result.points[WHITE] > result.points[BLACK] ? WHITE : BLACK;
    result.value = pos.points_value(bonus[WHITE], bonus[BLACK], ply);
    // Reserve mate scores for a checkmate that actually wins on points.
    // A tied mate, or one won by the mated player, keeps its points result.
    if (reason == Termination::Checkmate && result.winner == ~actor)
        result.value = *result.winner == pos.side_to_move() ? mate_in(ply) : mated_in(ply);
    return result;
}

Value resignation_value(const Position& pos, int ply) {
    if (!pos.game_of_points()) return -VALUE_INFINITE;
    const Value value = settle_points(pos, Termination::Resignation, pos.side_to_move(), ply).value;
    // Only a winning/drawing claim is a useful search action. A losing
    // resignation must not bound away an unavoidable mate with a non-mate loss.
    return value >= VALUE_DRAW ? value : -VALUE_INFINITE;
}

Outcome outcome(const Position& pos, bool claimDraw, int repetitionPly) {
    if (!MoveList<ALL>(pos).size()) {
        const bool check = pos.game_of_points() ? bool(pos.checkers()) : pos.reported_check();
        if (pos.game_of_points())
            return settle_points(pos, check ? Termination::Checkmate : Termination::Stalemate,
                                 pos.side_to_move(), repetitionPly);
        if (check) return {Termination::Checkmate, -VALUE_MATE, ~pos.side_to_move()};
        return {Termination::Stalemate, VALUE_DRAW, std::nullopt};
    }
    if (pos.game_of_points()) {
        if (insufficient_material(pos))
            return settle_points(pos, Termination::InsufficientMaterial, pos.side_to_move(), repetitionPly);
        if (pos.rule50_count() >= 100)
            return settle_points(pos, Termination::FiftyMoveRule, pos.side_to_move(), repetitionPly);
        if (pos.is_repetition(0))
            return settle_points(pos, Termination::ThreefoldRepetition, pos.side_to_move(), repetitionPly);
    }
    else {
        if (claimDraw && pos.rule50_count() >= 100)
            return {Termination::FiftyMoveRule, VALUE_DRAW, std::nullopt};
        if (claimDraw && pos.is_repetition(repetitionPly))
            return {Termination::ThreefoldRepetition, VALUE_DRAW, std::nullopt};
    }
    return {};
}

std::string points_status(const Position& pos) {
    const auto end = outcome(pos, true);
    const auto resign = settle_points(pos, Termination::Resignation, pos.side_to_move());
    std::ostringstream out;
    out << "{\"rules\":\"" << (pos.game_of_points() ? "chesscom-points-v1" : "walls-legacy")
        << "\",\"earned\":[" << pos.points(WHITE) << ',' << pos.points(BLACK)
        << "],\"final\":[" << (end.terminal() ? end.points[WHITE] : pos.points(WHITE))
        << ',' << (end.terminal() ? end.points[BLACK] : pos.points(BLACK))
        << "],\"result\":\"" << end.result() << "\",\"termination\":\"" << end.reason()
        << "\",\"canClaimWin\":"
        << (pos.game_of_points() && !end.terminal() && resign.value > 0 ? "true" : "false")
        << ",\"resignResult\":\"" << resign.result() << "\"}";
    return out.str();
}
} // namespace Stockfish::Petrified
