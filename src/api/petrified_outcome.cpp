// SPDX-License-Identifier: GPL-3.0-or-later
#include "petrified_outcome.h"
#include "../position.h"
#include "../movegen.h"
namespace Stockfish::Petrified {
std::string Outcome::result() const {
    return !terminal() ? "*" : !winner ? "1/2-1/2" : *winner == WHITE ? "1-0" : "0-1";
}
// The pinned FSF API considers COMMONER sufficient. Never import orthodox
// insufficient-material adjudication, including in bare-king positions.
bool has_insufficient_material(Color, const Position&) { return false; }
Bitboard checked_squares(const Position& pos) { return pos.reported_check() ? pos.pieces(pos.side_to_move(),KING) : 0; }
Outcome outcome(const Position& pos, bool claimDraw, int repetitionPly) {
    if (!MoveList<ALL>(pos).size()) {
        if (pos.reported_check()) return {Termination::Checkmate,-VALUE_MATE,~pos.side_to_move()};
        return {Termination::Stalemate,VALUE_DRAW,std::nullopt};
    }
    if(claimDraw && pos.rule50_count()>=100) return {Termination::FiftyMoveRule,VALUE_DRAW,std::nullopt};
    if(claimDraw && pos.is_repetition(repetitionPly)) return {Termination::ThreefoldRepetition,VALUE_DRAW,std::nullopt};
    return {};
}
}
