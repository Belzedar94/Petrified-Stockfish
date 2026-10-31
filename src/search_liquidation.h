// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_LIQUIDATION_H_INCLUDED
#define SEARCH_LIQUIDATION_H_INCLUDED

#include "position.h"

namespace Stockfish::Search {

// A real petrifying capture removes both live pieces and adds a wall. Before
// treating its non-check child as quiet, search the opponent's legal replies.
// Null moves copy petrified but clear capturedPiece/pliesFromNull. A FEN alone
// carries no last-move obligation; this is search context, not a rule or proof.
inline bool liquidation_reply(const Position& pos) {
    return pos.game_of_points() && !pos.checkers() && pos.captured_piece() != NO_PIECE
        && pos.state()->petrified && pos.state()->pliesFromNull > 0;
}

// Keep mandatory-response qsearch bounds separate from ordinary stand-pat
// bounds for the same board. Main-search keys and persistent state are intact.
inline Key liquidation_qsearch_key(const Position& pos, bool reply) {
    constexpr Key ReplyDomain = 0xA8F6D139C27E405BULL;
    return pos.key() ^ (reply ? ReplyDomain : Key(0));
}

}  // namespace Stockfish::Search
#endif
