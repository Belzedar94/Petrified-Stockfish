// SPDX-License-Identifier: GPL-3.0-or-later
#include "petrified_fen.h"
#include "../position.h"
#include <string>
namespace Stockfish::Petrified {
int validate_fen(std::string_view fen, bool chess960) {
    if(fen.empty()) return FEN_EMPTY;
    Position pos; StateInfo state;
    const auto error=pos.set(std::string(fen),chess960,&state);
    if(!error) return FEN_OK;
    const std::string message=error->what();
    if(message.find("side to move")!=std::string::npos) return FEN_INVALID_SIDE_TO_MOVE;
    if(message.find("castling")!=std::string::npos) return FEN_INVALID_CASTLING_INFO;
    if(message.find("en-passant")!=std::string::npos) return FEN_INVALID_EN_PASSANT_SQ;
    if(message.find("king")!=std::string::npos || message.find("King")!=std::string::npos) return FEN_INVALID_NUMBER_OF_KINGS;
    if(message.find("Rule50")!=std::string::npos) return FEN_INVALID_HALF_MOVE_COUNTER;
    if(message.find("Game ply")!=std::string::npos) return FEN_INVALID_MOVE_COUNTER;
    if(message.find("Invalid piece")!=std::string::npos) return FEN_INVALID_CHAR;
    return FEN_INVALID_BOARD_GEOMETRY;
}
}
