// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SEARCH_THREAT_H_INCLUDED
#define SEARCH_THREAT_H_INCLUDED

#include "movegen.h"
#include "position.h"

namespace Stockfish::Search {

// A bounded qsearch hint, not a forced-capture or settlement certificate.
// A hypothetical pass clears EP. The existing SEE measures only the legal
// local material/points exchange; it does not price the resulting wall.
inline bool quiet_capture_threat(Position& pos) {
    if (!pos.game_of_points() || pos.checkers())
        return false;

    const MoveList<ALL> replies(pos);
    if (replies.size() < 2)
        return false;
    for (Move move : replies)
        if (pos.capture_stage(move))
            return false;

    StateInfo pass;
    pos.do_null_move(pass);
    bool threatened = false;
    for (Move move : MoveList<ALL>(pos))
        if (pos.capture(move) && pos.see_ge(move, 1))
        {
            threatened = true;
            break;
        }
    pos.undo_null_move();
    return threatened;
}

// Entry nodes may search a quiet response layer; recursive nodes may not.
// Capture-only TT bounds must not hide the richer entry search.
inline Depth threat_qsearch_depth(bool points, bool entry) {
    return points && !entry ? DEPTH_QS - 1 : DEPTH_QS;
}

}  // namespace Stockfish::Search
#endif
