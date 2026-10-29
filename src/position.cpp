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

#include "position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>

#include "api/petrified_outcome.h"
#include "bitboard.h"
#include "history.h"
#include "misc.h"
#include "movegen.h"
#include "uci_move.h"

using std::string;

namespace Stockfish {

using namespace Attacks;

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key enpassant[FILE_NB];
Key castling[CASTLING_RIGHT_NB];
Key side, noPawns;
Key walls[SQUARE_NB];

}

namespace {

constexpr std::string_view PieceToChar(" PNBRQK  pnbrqk");

static constexpr Piece Pieces[] = {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
                                   B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING};
}  // namespace


// Returns an ASCII representation of the position
std::ostream& operator<<(std::ostream& os, const Position& pos) {

    os << "\n +---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
            os << " | " << ((pos.walls() & make_square(f, r)) ? '*' : PieceToChar[pos.piece_on(make_square(f, r))]);

        os << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";

        if (r == RANK_1)
            break;
    }

    os << "   a   b   c   d   e   f   g   h\n"
       << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << std::setfill('0')
       << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\nCheckers: ";

    for (Bitboard b = pos.checkers(); b;)
        os << UCI::square(pop_lsb(b)) << " ";

    return os;
}


// Implements Marcel van Kervinck's cuckoo algorithm to detect repetition of positions
// for 3-fold repetition draws. The algorithm uses two hash tables with Zobrist hashes
// to allow fast detection of recurring positions. For details see:
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// First and second hash functions for indexing the cuckoo tables
inline int H1(Key h) { return h & 0x1fff; }
inline int H2(Key h) { return (h >> 16) & 0x1fff; }

// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
static std::array<Key, 8192>  cuckoo;
static std::array<Move, 8192> cuckooMove;

// Initializes at startup the various arrays used to compute hash keys
void Position::init() {

    PRNG rng(1070372);

    for (Piece pc : Pieces)
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            Zobrist::psq[pc][s] = rng.rand<Key>();
    // pawns on these squares will promote
    std::fill_n(Zobrist::psq[W_PAWN] + SQ_A8, 8, 0);
    std::fill_n(Zobrist::psq[B_PAWN], 8, 0);

    for (File f = FILE_A; f <= FILE_H; ++f)
        Zobrist::enpassant[f] = rng.rand<Key>();

    for (int cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr)
        Zobrist::castling[cr] = rng.rand<Key>();

    Zobrist::side    = rng.rand<Key>();
    Zobrist::noPawns = rng.rand<Key>();

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
        Zobrist::walls[s] = rng.rand<Key>();

    // Prepare the cuckoo tables
    cuckoo.fill(0);
    cuckooMove.fill(Move::none());
    [[maybe_unused]] int count = 0;
    for (Piece pc : Pieces)
        for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
            for (Square s2 = Square(s1 + 1); s2 <= SQ_H8; ++s2)
                if (type_of(pc) == PAWN
                      ? rank_of(s1) == rank_of(s2) && rank_of(s1) >= RANK_2
                          && rank_of(s1) <= RANK_7 && int(s2) == int(s1) + 1
                      : bool(attacks_bb(type_of(pc), s1, 0) & s2))
                {
                    Move move = Move(s1, s2);
                    Key  key  = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                    int  i    = H1(key);
                    while (true)
                    {
                        std::swap(cuckoo[i], key);
                        std::swap(cuckooMove[i], move);
                        if (move == Move::none())  // Arrived at empty slot?
                            break;
                        i = (i == H1(key)) ? H2(key) : H1(key);  // Push victim to alternative slot
                    }
                    count++;
                }
    assert(count == 3752);
}


// Initializes the position object with the given FEN string.
// The FEN string is strictly validated; if it is invalid or inconsistent,
// a PositionSetError describing the problem is returned, otherwise std::nullopt.
std::optional<PositionSetError>
Position::set(const string& fenStr, bool isChess960, StateInfo* si) {
    /*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") whilst Black uses lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. Following X-FEN standard, this is recorded
      only if there is a pawn in position to make an en passant capture, and if
      there really is a pawn that might have advanced two squares.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

    unsigned char      token;
    std::istringstream ss(fenStr);

    std::memset(reinterpret_cast<char*>(this), 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
    st = si;

    ss >> std::noskipws;

    int numPieces = 0;
    int file      = FILE_A;
    int rank      = RANK_8;

    // 1. Piece placement
    for (;;)
    {
        if (!(ss >> token))
            return PositionSetError("Invalid FEN. Unexpected end of stream.");

        if (isspace(token))
            break;

        if (isdigit(token))
        {
            const int diff = (token - '0');
            if (diff < 1 || diff > 8)
                return PositionSetError("Invalid FEN. Invalid number of squares to skip.");

            file += diff;
            if (file > FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");
        }
        else if (token == '/')
        {
            if (file != FILE_NB)
                return PositionSetError(
                  "Invalid FEN. Trying to end rank when not at the end of it.");

            --rank;
            file = FILE_A;

            if (rank < RANK_1)
                return PositionSetError("Invalid FEN. Invalid rank reached.");
        }
        else if (token == '*')
        {
            if (file >= FILE_NB)
                return PositionSetError("Invalid FEN. Wall outside board.");
            wallBB |= make_square(File(file++), Rank(rank));
        }
        else
        {
            if (file >= FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");

            const usize idx = PieceToChar.find(token);
            if (idx == string::npos)
                return PositionSetError(std::string("Invalid FEN. Invalid piece: ")
                                        + std::string(1, token));

            if (++numPieces > 32)
                return PositionSetError("Invalid FEN. More than 32 pieces on the board.");

            const Square sq = make_square(File(file), Rank(rank));
            put_piece(Piece(idx), sq);

            ++file;
        }
    }

    if (rank != RANK_1 || file != FILE_NB)
        return PositionSetError("Invalid FEN. Board state encoding ended but cursor not at end.");

    if (pieces(PAWN) & (Rank1BB | Rank8BB))
        return PositionSetError("Unsupported position. Pawns on the first or eighth rank.");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1)
        return PositionSetError("Unsupported position. Incorrect number of kings.");

    for (Color c : {WHITE, BLACK})
    {
        if (count<PAWN>(c) > 8)
            return PositionSetError(std::string("Unsupported position. ")
                                    + (c == WHITE ? "WHITE" : "BLACK") + " has more than 8 pawns.");

        int additional = std::max(count<KNIGHT>(c) - 2, 0) + std::max(count<BISHOP>(c) - 2, 0)
                       + std::max(count<ROOK>(c) - 2, 0) + std::max(count<QUEEN>(c) - 1, 0);
        if (additional > 8 - count<PAWN>(c))
            return PositionSetError(std::string("Unsupported position. Too many pieces for ")
                                    + (c == WHITE ? "WHITE." : "BLACK."));
    }

    // 2. Active color
    if (!(ss >> token))
        return PositionSetError("Invalid FEN. Unexpected end of stream.");
    if (token != 'w' && token != 'b')
        return PositionSetError(std::string("Invalid FEN. Invalid side to move: ")
                                + std::string(1, token));
    sideToMove = (token == 'w' ? WHITE : BLACK);
    if (!(ss >> token) || !isspace(token) || ss.eof())
        return PositionSetError("Invalid FEN. Expected whitespace after side to move.");

    // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
    // Shredder-FEN that uses the letters of the columns on which the rooks began
    // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
    // if an inner rook is associated with the castling right, the castling tag is
    // replaced by the file letter of the involved rook, as for the Shredder-FEN.
    //
    // NOTE: Due to the prevalence of incorrect (or missing) castling rights the
    // validation is less strict. However, incorrect castling rights are still sanitized.
    int num_castling_rights = 0;
    for (;;)
    {
        if (!(ss >> token))
            break;

        if (isspace(token))
            break;

        if (num_castling_rights == 0 && token == '-')
        {
            ss >> std::ws;
            break;
        }

        if (++num_castling_rights > 4)
            return PositionSetError("Invalid FEN. Maximum of 4 castling rights can be specified.");

        Square rsq  = SQ_NONE;
        Square ksq  = SQ_NONE;
        Color  c    = islower(token) ? BLACK : WHITE;
        Piece  rook = make_piece(c, ROOK);
        Piece  king = make_piece(c, KING);

        token = char(toupper(token));

        if (token == 'K' || token == 'Q')
        {
            const int dir = token == 'K' ? -1 : 1;
            Square    sq  = relative_square(c, token == 'K' ? SQ_H1 : SQ_A1);
            // Look for a rook and a king for the castling. King must come later.
            // Only the first rook is noted.
            // If the castling rights are available the king must always be between files 2 and 7 inclusive
            // so there is no need to check the last square.
            for (int i = 0; i < 7; ++i, sq = Square(sq + dir))
            {
                const Piece pc = piece_on(sq);
                if (pc == king)
                {
                    ksq = sq;
                    break;
                }
                else if (pc == rook && rsq == SQ_NONE)
                {
                    rsq = sq;
                }
            }
        }
        else if (token >= 'A' && token <= 'H')
        {
            const Square rsqCandidate = make_square(File(token - 'A'), relative_rank(c, RANK_1));
            if (piece_on(rsqCandidate) == rook)
                rsq = rsqCandidate;

            // If the castling rights are available the king must always be between files 2 and 7 inclusive.
            Square sq = relative_square(c, SQ_B1);
            for (int i = 0; i < 6; ++i, ++sq)
            {
                if (piece_on(sq) == king)
                    ksq = sq;
            }
        }
        else
        {
            return PositionSetError(std::string("Invalid FEN. Expected castling rights. Got: ")
                                    + std::string(1, token));
        }

        // Only apply castling rights if they can be valid.
        if (ksq != SQ_NONE && rsq != SQ_NONE)
            set_castling_right(c, rsq);
    }

    // Walls uses a set of passed squares for the custom sideways pawn,
    // including its destination. Preserve that FEN state even without a capturer.
    std::string ep;
    ss >> std::skipws >> ep;
    st->epSquares = 0;
    if (ep != "-")
    {
        if (ep.empty() || ep.size() % 2)
            return PositionSetError("Invalid FEN. Invalid en-passant field.");
        for (usize i = 0; i < ep.size(); i += 2)
        {
            if (ep[i] < 'a' || ep[i] > 'h' || ep[i+1] < '1' || ep[i+1] > '8')
                return PositionSetError("Invalid FEN. Invalid en-passant square.");
            st->epSquares |= make_square(File(ep[i]-'a'), Rank(ep[i+1]-'1'));
        }
    }
    st->epSquare = st->epSquares ? lsb(st->epSquares) : SQ_NONE;

    // 5-6. Halfmove clock and fullmove number
    if (!(ss >> std::skipws >> st->rule50 >> gamePly))
        return PositionSetError("Invalid FEN. Expected halfmove and fullmove counters.");

    // Normally values larger than 99 would be pointless but we do support ignoring 50 move rule for TB purposes.
    // Limit at 2**15 as it's used multiplicatively with position evaluation during search.
    if (st->rule50 < 0 || st->rule50 > 32767)
        return PositionSetError("Unsupported position. Rule50 counter out of range.");

    if (gamePly < 0 || gamePly > 100000)
        return PositionSetError("Unsupported position. Game ply out of range.");

    std::string extra;
    if (ss >> extra)
    {
        if (extra.substr(0, 7) != "points=")
            return PositionSetError("Invalid FEN. Expected points=white,black.");
        std::istringstream scores(extra.substr(7));
        char comma, trailing;
        if (!(scores >> st->points[WHITE] >> comma >> st->points[BLACK]) || comma != ','
            || (scores >> trailing) || st->points[WHITE] < 0 || st->points[BLACK] < 0
            || st->points[WHITE] > 1000000 || st->points[BLACK] > 1000000 || (ss >> extra))
            return PositionSetError("Invalid FEN. Invalid points field.");
        st->pointsMode = true;
    }

    // Convert from fullmove starting from 1 to gamePly starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    chess960 = isChess960;
    set_state();

    if (attackers_to(square<KING>(~sideToMove)) & pieces(sideToMove))
        return PositionSetError("Unsupported position. King can be captured.");

    assert(pos_is_ok());

    return std::nullopt;
}


// Helper function used to set castling
// rights given the corresponding color and the rook starting square.
void Position::set_castling_right(Color c, Square rfrom) {

    Square         kfrom = square<KING>(c);
    CastlingRights cr    = c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    Square kto = relative_square(c, cr & KING_SIDE ? SQ_G1 : SQ_C1);
    Square rto = relative_square(c, cr & KING_SIDE ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)) & ~(kfrom | rfrom);
}


// Sets pinners and where stm can give check. Squares threatened by ~stm are
// computed lazily by compute_threats().
void Position::set_extra_bitboards() const {

    update_slider_blockers(WHITE);
    update_slider_blockers(BLACK);

    Square ksq                              = square<KING>(~sideToMove);
    const auto [bishopAttacks, rookAttacks] = both_attacks_bb(ksq, occupied());

    st->checkSquares[PAWN]   = attacks_bb(PAWN, ksq, ~sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb(KNIGHT, ksq);
    st->checkSquares[BISHOP] = bishopAttacks;
    st->checkSquares[ROOK]   = rookAttacks;
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;

    st->threats[ALL_PIECES] = 0;
}


void Position::compute_threats() const {

    Bitboard occ            = occupied() ^ square<KING>(sideToMove);
    st->threats[ALL_PIECES] = st->threats[PAWN]    = xattacks_by(PAWN, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[KNIGHT] = xattacks_by(KNIGHT, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[BISHOP] = xattacks_by(BISHOP, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[ROOK]   = xattacks_by(ROOK, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[QUEEN]  = xattacks_by(QUEEN, ~sideToMove, occ);
    st->threats[ALL_PIECES] |= st->threats[KING]   = xattacks_by(KING, ~sideToMove, occ);
}


// Computes the hash keys of the position, and other
// data that once computed is updated incrementally as moves are made.
// The function is only used when a new position is set up
void Position::set_state() const {

    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    set_extra_bitboards();

    for (Bitboard b = pieces(); b;)
    {
        Square s  = pop_lsb(b);
        Piece  pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][s];

        else
        {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

            if (type_of(pc) != KING)
            {
                st->nonPawnMaterial[color_of(pc)] += PieceValue[pc];

                if (type_of(pc) <= BISHOP)
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
            }
        }
    }

    for (Bitboard b = walls(); b;)
    {
        const Key k = Zobrist::walls[pop_lsb(b)];
        st->key ^= k;
        st->pawnKey ^= k;
        st->minorPieceKey ^= k;
        st->nonPawnKey[WHITE] ^= k;
        st->nonPawnKey[BLACK] ^= k;
    }

    for (Bitboard b = st->epSquares; b;)
        st->key ^= make_key(0x455000 + pop_lsb(b));

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;

    st->key ^= Zobrist::castling[st->castlingRights];
    if (game_of_points())
    {
        const Key scoreKey = make_key(0x504f494e5453ULL)
                             ^ make_key(0x100000000ULL + st->points[WHITE])
                             ^ make_key(0x200000000ULL + st->points[BLACK]);
        st->key ^= scoreKey;
        st->pawnKey ^= scoreKey;
        st->minorPieceKey ^= scoreKey;
        st->nonPawnKey[WHITE] ^= scoreKey;
        st->nonPawnKey[BLACK] ^= scoreKey;
    }
    st->materialKey = compute_material_key();
}

Key Position::compute_material_key() const {
    Key k = 0;
    for (Piece pc : Pieces)
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
            k ^= Zobrist::psq[pc][8 + cnt];
    return k;
}


// Overload to initialize the position object with the given endgame code string
// like "KBPKN". It's mainly a helper to get the material key out of an endgame code.
std::optional<PositionSetError> Position::set(const string& code, Color c, StateInfo* si) {

    assert(code[0] == 'K');

    string sides[] = {code.substr(code.find('K', 1)),                                // Weak
                      code.substr(0, std::min(code.find('v'), code.find('K', 1)))};  // Strong

    assert(sides[0].length() > 0 && sides[0].length() < 8);
    assert(sides[1].length() > 0 && sides[1].length() < 8);

    std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

    string fenStr = "8/" + sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/" + sides[1]
                  + char(8 - sides[1].length() + '0') + "/8 w - - 0 10";

    return set(fenStr, false, si);
}


// Returns a FEN representation of the position. In case of
// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.
string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
        {
            for (emptyCnt = 0; f <= FILE_H && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_H)
                ss << ((walls() & make_square(f, r)) ? '*' : PieceToChar[piece_on(make_square(f, r))]);
        }

        if (r == RANK_1)
            break;
        ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');

    if (can_castle(WHITE_OOO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

    if (can_castle(BLACK_OO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');

    if (can_castle(BLACK_OOO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

    if (!can_castle(ANY_CASTLING))
        ss << '-';

    ss << ' ';
    if (!st->epSquares) ss << '-';
    for (Bitboard b = st->epSquares; b;) ss << UCI::square(pop_lsb(b));
    ss << ' ' << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;
    if (game_of_points()) ss << " points=" << points(WHITE) << ',' << points(BLACK);

    return ss.str();
}

// Calculates st->blockersForKing[c] and st->pinners[~c],
// which store respectively the pieces preventing king of color c from being in check
// and the slider pieces of color ~c pinning pieces of color c to the king.
void Position::update_slider_blockers(Color c) const {

    Square ksq = square<KING>(c);

    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    // Snipers are sliders that attack 's' when a piece and other snipers are removed
    Bitboard snipers = ((attacks_bb(ROOK, ksq) & pieces(QUEEN, ROOK))
                        | (attacks_bb(BISHOP, ksq) & pieces(QUEEN, BISHOP)))
                     & pieces(~c);
    Bitboard occupancy = occupied() ^ snipers;

    while (snipers)
    {
        Square   sniperSq = pop_lsb(snipers);
        Bitboard b        = between_bb(ksq, sniperSq) & occupancy;

        if (b && !more_than_one(b))
        {
            st->blockersForKing[c] |= b;
            if (b & pieces(c))
                st->pinners[~c] |= sniperSq;
        }
    }
}


// Computes a bitboard of all pieces which attack a given square.
// Slider attacks use the occupied bitboard to indicate occupancy.
Bitboard Position::attackers_to(Square s, Bitboard occupied) const {

    const auto [bishopAttacks, rookAttacks] = both_attacks_bb(s, occupied);

    return (rookAttacks & pieces(ROOK, QUEEN)) | (bishopAttacks & pieces(BISHOP, QUEEN))
         | (attacks_bb(PAWN, s, BLACK) & pieces(WHITE, PAWN))
         | (attacks_bb(PAWN, s, WHITE) & pieces(BLACK, PAWN))
         | (attacks_bb(KNIGHT, s) & pieces(KNIGHT));
}

// Takes a random move and tests whether the move is legal. It is used to validate moves
// from TT that can be corrupted due to SMP concurrent access or hash position key aliasing.
bool Position::legal(Move m) const {
    return m.is_ok() && MoveList<ALL>(*this).contains(m);
}

bool Position::reported_check() const {
    return checkers()
        || (attacks_bb(KING, square<KING>(sideToMove)) & pieces(~sideToMove, KING));
}

Value Position::no_moves_value(int ply) const {
    if (game_of_points())
        return Petrified::settle_points(*this, checkers() ? Petrified::Termination::Checkmate
                                                        : Petrified::Termination::Stalemate,
                                       sideToMove, ply).value;
    return reported_check() ? mated_in(ply) : VALUE_DRAW;
}

int Position::capture_points(PieceType pt) {
    constexpr int values[PIECE_TYPE_NB] = {0, 1, 3, 5, 5, 9, 20, 0};
    return values[pt];
}

Value Position::points_value(int whiteBonus, int blackBonus, int ply) const {
    int difference = points(WHITE) + whiteBonus - points(BLACK) - blackBonus;
    if (sideToMove == BLACK) difference = -difference;
    // Generic points outcomes; settle_points separately recognizes winning mates.
    return difference > 0 ? VALUE_TB - ply : difference < 0 ? -VALUE_TB + ply : VALUE_DRAW;
}

// Test attacks on the resulting board, including a neutral wall left by a capture.
// Kings are royal commoners: they neither capture nor attack the opposing king.
bool Position::attacked_after(Square s, Color by, Move m) const {
    Bitboard bb[PIECE_TYPE_NB] = {};
    for (PieceType pt = PAWN; pt <= KING; ++pt)
        bb[pt] = pieces(by, pt);
    const Color us = sideToMove;
    const Square from = m.from_sq(), to = m.to_sq();
    const Piece pc = moved_piece(m);
    Bitboard occ = occupied() & ~square_bb(from);
    if (m.type_of() == CASTLING)
    {
        const Square kt = relative_square(us, to > from ? SQ_G1 : SQ_C1);
        const Square rt = relative_square(us, to > from ? SQ_F1 : SQ_D1);
        occ = (occ & ~square_bb(to)) | kt | rt;
        if (by == us)
        {
            bb[KING] = square_bb(kt);
            bb[ROOK] = (bb[ROOK] & ~square_bb(to)) | rt;
        }
    }
    else
    {
        const Square cs = m.type_of() == EN_PASSANT ? to - pawn_push(us) : to;
        const Piece victim = piece_on(cs);
        if (victim)
        {
            occ &= ~square_bb(cs);
            if (by != us)
                bb[type_of(victim)] &= ~square_bb(cs);
        }
        occ |= to; // surviving mover or newly created wall
        if (by == us)
        {
            bb[type_of(pc)] &= ~square_bb(from);
            if (!victim || type_of(pc) == PAWN)
                bb[m.type_of() == PROMOTION ? m.promotion_type() : type_of(pc)] |= to;
        }
    }
    return (attacks_bb(PAWN, s, ~by) & bb[PAWN])
        || (attacks_bb(KNIGHT, s) & bb[KNIGHT])
        || (attacks_bb(BISHOP, s, occ) & (bb[BISHOP] | bb[QUEEN]))
        || (attacks_bb(ROOK, s, occ) & (bb[ROOK] | bb[QUEEN]));
}

bool Position::safe_after(Move m) const {
    Square king = square<KING>(sideToMove);
    if (m.type_of() == CASTLING)
    {
        if (checkers())
            return false;
        const Square end = relative_square(sideToMove, m.to_sq() > king ? SQ_G1 : SQ_C1);
        const int step = end > king ? 1 : -1;
        for (Square s = king; ; s = Square(int(s) + step)) {
            if ((attackers_to(s, occupied() & ~square_bb(king)) & pieces(~sideToMove))
                || (attacks_bb(KING, s) & pieces(~sideToMove, KING)))
                return false;
            if (king == end || Square(int(s) + step) == end) break;
        }
        king = end;
    }
    else if (type_of(moved_piece(m)) == KING)
        king = m.to_sq();
    return !attacked_after(king, ~sideToMove, m);
}

bool Position::gives_check(Move m) const {
    return attacked_after(square<KING>(~sideToMove), sideToMove, m);
}



// Makes a move, and saves all information necessary
// to a StateInfo object. The move is assumed to be legal.
// If a pointer to the shared histories is passed, their entries for the
// new position will be prefetched.
void Position::do_move(Move m, StateInfo& newSt, bool, Dirties& dirties,
                       const SharedHistories*) {
    assert(m.is_ok());
    assert(&newSt != st);
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st = &newSt;
    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;
    st->epSquare = SQ_NONE;
    st->epSquares = 0;
    st->repetition = 0;
    st->petrified = false;
    const Color us = sideToMove;
    const Square from = m.from_sq();
    Square to = m.to_sq();
    const Piece pc = piece_on(from);
    st->movedPiece = pc;
    st->capturedPiece = m.type_of() == CASTLING ? NO_PIECE
                       : m.type_of() == EN_PASSANT ? make_piece(~us, PAWN) : piece_on(to);
    auto& dp = dirties.dirtyPiece;
    dp = {pc, from, to, SQ_NONE, SQ_NONE, NO_PIECE, NO_PIECE};
    new (&dirties.dirtyThreats) DirtyThreats;
    dirties.dirtyPawnPairs.before[WHITE] = pieces(WHITE, PAWN);
    dirties.dirtyPawnPairs.before[BLACK] = pieces(BLACK, PAWN);
    st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
    if (m.type_of() == CASTLING)
    {
        Square rfrom, rto;
        do_castling<true>(us, from, to, rfrom, rto, nullptr, &dp);
    }
    else
    {
        if (st->capturedPiece)
        {
            if (game_of_points()) st->points[us] += capture_points(type_of(st->capturedPiece));
            const Square cs = m.type_of() == EN_PASSANT ? to - pawn_push(us) : to;
            dp.remove_pc = st->capturedPiece;
            dp.remove_sq = cs;
            remove_piece(cs);
            st->rule50 = 0;
        }
        if (st->capturedPiece && type_of(pc) != PAWN)
        {
            remove_piece(from);
            wallBB |= to;
            st->petrified = true;
            dp.to = SQ_NONE;
        }
        else
        {
            move_piece(from, to);
            if (m.type_of() == PROMOTION)
            {
                swap_piece(to, make_piece(us, m.promotion_type()));
                dp.to = SQ_NONE;
                dp.add_sq = to;
                dp.add_pc = piece_on(to);
            }
        }
        if (type_of(pc) == PAWN)
        {
            // Sideways pawn moves are reversible in Walls.
            if (rank_of(from) != rank_of(to)) st->rule50 = 0;
            if (int(to) - int(from) == 2 * int(pawn_push(us)))
            {
                st->epSquares = square_bb(to - pawn_push(us)) | to;
                st->epSquare = lsb(st->epSquares);
            }
        }
    }
    sideToMove = ~sideToMove;
    set_state();
    const int end = std::min(st->rule50, st->pliesFromNull);
    StateInfo* prior = st;
    for (int i = 2; i <= end; i += 2)
    {
        prior = prior->previous->previous;
        if (i >= 4 && prior->key == st->key)
        {
            st->repetition = prior->repetition ? -i : i;
            break;
        }
    }
    dirties.dirtyPawnPairs.after[WHITE] = pieces(WHITE, PAWN);
    dirties.dirtyPawnPairs.after[BLACK] = pieces(BLACK, PAWN);
    assert(pos_is_ok());
}

void Position::undo_move(Move m) {
    sideToMove = ~sideToMove;
    const Square from = m.from_sq();
    Square to = m.to_sq();
    if (m.type_of() == CASTLING)
    {
        Square rfrom, rto;
        do_castling<false>(sideToMove, from, to, rfrom, rto);
    }
    else
    {
        if (st->petrified)
        {
            wallBB &= ~square_bb(to);
            put_piece(st->movedPiece, from);
        }
        else
        {
            if (m.type_of() == PROMOTION)
                swap_piece(to, make_piece(sideToMove, PAWN));
            move_piece(to, from);
        }
        if (st->capturedPiece)
            put_piece(st->capturedPiece,
                      m.type_of() == EN_PASSANT ? to - pawn_push(sideToMove) : to);
    }
    st = st->previous;
    --gamePly;
    assert(pos_is_ok());
}



inline void add_dirty_threat(DirtyThreats* const dts,
                             bool                putPiece,
                             Piece               pc,
                             Piece               threatened,
                             Square              s,
                             Square              threatenedSq) {
    dts->list.push_back({pc, threatened, s, threatenedSq, putPiece});
}

#ifdef USE_AVX512ICL
// Given a DirtyThreat template and bit offsets to insert the piece type and square, write the threats
// present at the given bitboard.
template<int SqShift, int PcShift>
void write_multiple_dirties(const Position& p,
                            Bitboard        mask,
                            DirtyThreat     dt_template,
                            DirtyThreats*   dts) {
    static_assert(sizeof(DirtyThreat) == 4);

    const __m512i board    = _mm512_loadu_si512(p.piece_array().data());
    const int     dt_count = popcount(mask);
    assert(dt_count <= 16);

    const __m512i template_v = _mm512_set1_epi32(dt_template.raw());
    auto*         write      = dts->list.make_space(dt_count);

    // Extract the list of squares and upconvert to 32 bits. There are never more than 16
    // incoming threats so this is sufficient.
    __m512i threat_squares = _mm512_maskz_compress_epi8(mask, AllSquares);
    threat_squares         = _mm512_cvtepi8_epi32(_mm512_castsi512_si128(threat_squares));

    __m512i threat_pieces =
      _mm512_maskz_permutexvar_epi8(0x1111111111111111ULL, threat_squares, board);

    // Shift the piece and square into place
    threat_squares = _mm512_slli_epi32(threat_squares, SqShift);
    threat_pieces  = _mm512_slli_epi32(threat_pieces, PcShift);

    const __m512i dirties =
      _mm512_ternarylogic_epi32(template_v, threat_squares, threat_pieces, 254 /* A | B | C */);
    _mm512_storeu_si512(write, dirties);
}
#endif

constexpr bool can_slider_threat(Piece pc, Piece slider) {
    return type_of(pc) != QUEEN || type_of(slider) == QUEEN;
}

template<bool ComputeRay>
void Position::update_piece_threats(Piece               pc,
                                    bool                putPiece,
                                    Square              s,
                                    DirtyThreats* const dts,
                                    // Silence spurious warning on GCC 10
                                    [[maybe_unused]] Bitboard noRaysContaining) const {
    const Bitboard occupied         = pieces();
    const auto [bAttacks, rAttacks] = both_attacks_bb(s, occupied);
    const Bitboard  sliderAttacks   = bAttacks | rAttacks;
    const Bitboard  occupiedNoK     = occupied ^ pieces(KING);
    const PieceType pt              = type_of(pc);
    const Bitboard  sliders = (pieces(BISHOP, QUEEN) & bAttacks) | (pieces(ROOK, QUEEN) & rAttacks);

    auto process_sliders = [&](bool addDirectAttacks) {
        Bitboard b = sliders;
        while (b)
        {
            Square sliderSq = pop_lsb(b);
            Piece  slider   = piece_on(sliderSq);

            const Bitboard ray        = ray_pass_bb(sliderSq, s);
            const Bitboard discovered = ray & sliderAttacks & occupiedNoK;

            assert(!more_than_one(discovered));
            if (discovered && (ray & noRaysContaining) != noRaysContaining)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                if (can_slider_threat(threatenedPc, slider))
                    add_dirty_threat(dts, !putPiece, slider, threatenedPc, sliderSq, threatenedSq);
            }

            if (addDirectAttacks && can_slider_threat(pc, slider))
                add_dirty_threat(dts, putPiece, slider, pc, sliderSq, s);
        }
    };

    // Kings emit no direct threats
    if (pt == KING)
    {
        if constexpr (ComputeRay)
            process_sliders(false);
        return;
    }

    const Bitboard threatTargets = pt == PAWN                 ? pieces(KNIGHT, ROOK)
                                 : pt == BISHOP || pt == ROOK ? pieces(PAWN, KNIGHT, BISHOP, ROOK)
                                                              : occupiedNoK;
    Bitboard       threatened    = (pt == BISHOP  ? bAttacks
                                    : pt == ROOK  ? rAttacks
                                    : pt == QUEEN ? sliderAttacks
                                    : pt == PAWN  ? PseudoAttacks[color_of(pc)][s]
                                                  : PseudoAttacks[pt][s])
                        & threatTargets;
    Bitboard incomingThreats = PseudoAttacks[KNIGHT][s] & pieces(KNIGHT);

    if (pt == KNIGHT || pt == ROOK)
        incomingThreats |= (attacks_bb(PAWN, s, WHITE) & pieces(BLACK, PAWN))
                         | (attacks_bb(PAWN, s, BLACK) & pieces(WHITE, PAWN));

#ifdef USE_AVX512ICL
    write_multiple_dirties<DirtyThreat::ThreatenedSqOffset, DirtyThreat::ThreatenedPcOffset>(
      *this, threatened, {pc, NO_PIECE, s, Square(0), putPiece}, dts);

    const Bitboard directSliders = pt == QUEEN ? sliders & pieces(QUEEN) : sliders;
    write_multiple_dirties<DirtyThreat::PcSqOffset, DirtyThreat::PcOffset>(
      *this, directSliders | incomingThreats, {NO_PIECE, pc, Square(0), s, putPiece}, dts);

    // For ICL, direct threats were written above
    if constexpr (ComputeRay)
        process_sliders(false);
#else
    while (threatened)
    {
        Square threatenedSq = pop_lsb(threatened);
        Piece  threatenedPc = piece_on(threatenedSq);
        assert(threatenedSq != s);
        assert(threatenedPc != NO_PIECE);

        add_dirty_threat(dts, putPiece, pc, threatenedPc, s, threatenedSq);
    }

    if constexpr (ComputeRay)
        process_sliders(true);
    else
        incomingThreats |= pt == QUEEN ? sliders & pieces(QUEEN) : sliders;

    while (incomingThreats)
    {
        Square srcSq = pop_lsb(incomingThreats);
        Piece  srcPc = piece_on(srcSq);
        assert(srcSq != s);
        assert(srcPc != NO_PIECE);

        add_dirty_threat(dts, putPiece, srcPc, pc, srcSq, s);
    }
#endif
}

Key Position::prefetch_key(Move m) const {
    Key k = st->key ^ Zobrist::side;
    if (m.type_of() == NORMAL)
    {
        Piece pc = moved_piece(m), victim = piece_on(m.to_sq());
        k ^= Zobrist::psq[pc][m.from_sq()];
        k ^= victim && type_of(pc) != PAWN ? Zobrist::walls[m.to_sq()]
                                         : Zobrist::psq[pc][m.to_sq()];
        if (victim) k ^= Zobrist::psq[victim][m.to_sq()];
    }
    return adjust_key50<true>(k);
}



template<bool Do>
void Position::do_castling(Color               us,
                           Square              from,
                           Square&             to,
                           Square&             rfrom,
                           Square&             rto,
                           DirtyThreats* const dts,
                           DirtyPiece* const   dp) {

    bool kingSide = to > from;
    rfrom         = to;  // Castling is encoded as "king captures friendly rook"
    rto           = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
    to            = relative_square(us, kingSide ? SQ_G1 : SQ_C1);

    assert(!Do || dp);

    if (Do)
    {
        dp->to        = to;
        dp->remove_pc = dp->add_pc = make_piece(us, ROOK);
        dp->remove_sq              = rfrom;
        dp->add_sq                 = rto;
    }

    // Remove both pieces first since squares could overlap in Chess960
    remove_piece(Do ? from : to, dts);
    remove_piece(Do ? rfrom : rto, dts);
    put_piece(make_piece(us, KING), Do ? to : from, dts);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom, dts);
}


// Used to do a "null move": it flips
// the side to move without executing any move on the board.
void Position::do_null_move(StateInfo& newSt) {

    assert(!checkers());
    assert(&newSt != st);

    std::memcpy(&newSt, st, sizeof(StateInfo));

    newSt.previous = st;
    st             = &newSt;

    for (Bitboard b = st->epSquares; b;)
        st->key ^= make_key(0x455000 + pop_lsb(b));
    st->epSquare = SQ_NONE;
    st->epSquares = 0;

    st->key ^= Zobrist::side;

    st->pliesFromNull = 0;

    st->capturedPiece = NO_PIECE;

    sideToMove = ~sideToMove;

    set_extra_bitboards();

    st->repetition = 0;

    assert(pos_is_ok());
}


// Must be used to undo a "null move"
void Position::undo_null_move() {

    assert(!checkers());

    st         = st->previous;
    sideToMove = ~sideToMove;
}


// Tests if the SEE (Static Exchange Evaluation)
// value of the move is greater or equal to the given threshold. We'll use an
// algorithm similar to alpha-beta pruning with a null window.
bool Position::see_ge(Move m, int threshold) const {
    if (m.type_of() == CASTLING) return threshold <= 0;
    const Piece pc = moved_piece(m);
    int gain = m.type_of() == EN_PASSANT ? PieceValue[PAWN] : PieceValue[piece_on(m.to_sq())];
    if (game_of_points() && capture(m))
        gain += 100 * capture_points(m.type_of() == EN_PASSANT ? PAWN : type_of(piece_on(m.to_sq())));
    if (capture(m) && type_of(pc) != PAWN)
        return gain - PieceValue[pc] >= threshold;
    if (m.type_of() == PROMOTION)
        gain += PieceValue[m.promotion_type()] - PieceValue[PAWN];
    // A surviving pawn can be captured by a petrifying piece; use a lower bound.
    if (capture(m)) gain -= PieceValue[pc];
    return gain >= threshold;
}



bool Position::is_draw(int ply) const {

    // The pinned Walls variant has pseudo-royals, so its raw checkersBB is
    // always empty. Its optional 50-move claim is available even at mate.
    if (st->rule50 > 99)
        return true;

    return is_repetition(game_of_points() ? 0 : ply);
}

// Return a draw score if a position repeats once earlier but strictly
// after the root, or repeats twice before or at the root.
bool Position::is_repetition(int ply) const { return st->repetition && st->repetition < ply; }

// Tests whether there has been at least one repetition
// of positions since the last capture or pawn move.
bool Position::has_repeated() const {

    StateInfo* stc = st;
    int        end = std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}


// Tests if the position has a move which draws by repetition.
// This function accurately matches the outcome of is_draw() over all legal moves.
bool Position::upcoming_repetition(int ply) const {

    if (game_of_points()) return false; // Repetition settles points; it need not draw.

    int end = std::min(st->rule50, st->pliesFromNull);

    if (end < 3)
        return false;

    Key        originalKey = st->key;
    StateInfo* stp         = st->previous;
    Key        other       = originalKey ^ stp->key ^ Zobrist::side;

    for (int i = 3; i <= end; i += 2)
    {
        stp = stp->previous;
        other ^= stp->key ^ stp->previous->key ^ Zobrist::side;
        stp = stp->previous;

        if (other != 0)
            continue;

        Key moveKey = originalKey ^ stp->key;

        if (int j = H1(moveKey); cuckoo[j] == moveKey || (j = H2(moveKey), cuckoo[j] == moveKey))
        {
            Move   move = cuckooMove[j];
            Square s1   = move.from_sq();
            Square s2   = move.to_sq();

            if (!((between_bb(s1, s2) ^ s2) & occupied()))
            {
                if (ply > i)
                    return true;

                // For nodes before or at the root, check that the move is a
                // repetition rather than a move to the current position.
                if (stp->repetition)
                    return true;
            }
        }
    }
    return false;
}


// Flips position with the white and black sides reversed. This
// is only useful for debugging e.g. for finding evaluation symmetry bugs.
std::optional<PositionSetError> Position::flip() {

    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_8;; --r)  // Piece placement
    {
        std::getline(ss, token, r > RANK_1 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));

        if (r == RANK_1)
            break;
    }

    ss >> token;                        // Active color
    f += (token == "w" ? "B " : "W ");  // Will be lowercased later

    ss >> token;  // Castling availability
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](unsigned char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;  // En passant square
    if (token != "-")
        for (usize i = 1; i < token.size(); i += 2)
            token[i] = char('1' + ('8' - token[i]));
    f += token;

    int halfmove, fullmove;
    ss >> halfmove >> fullmove;
    f += " " + std::to_string(halfmove) + " " + std::to_string(fullmove);
    if (game_of_points())
        f += " points=" + std::to_string(points(BLACK)) + "," + std::to_string(points(WHITE));

    return set(f, is_chess960(), st);
}


bool Position::material_key_is_ok() const { return compute_material_key() == st->materialKey; }


// Performs some consistency checks for the position object
// and raise an assert if something wrong is detected.
// This is meant to be helpful when debugging.
bool Position::pos_is_ok() const {

    if ((sideToMove != WHITE && sideToMove != BLACK) || piece_on(square<KING>(WHITE)) != W_KING
        || piece_on(square<KING>(BLACK)) != B_KING
)
        assert(0 && "pos_is_ok: Default");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1
        || attackers_to(square<KING>(~sideToMove)) & pieces(sideToMove))
        assert(0 && "pos_is_ok: Kings");

    if ((pieces(PAWN) & (Rank1BB | Rank8BB)) || count<PAWN>(WHITE) > 8 || count<PAWN>(BLACK) > 8)
        assert(0 && "pos_is_ok: Pawns");


    assert(!(walls() & pieces()));
    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces()
        || popcount(pieces(WHITE)) > 16 || popcount(pieces(BLACK)) > 16)
        assert(0 && "pos_is_ok: Bitboards");

    for (PieceType p1 = PAWN; p1 <= KING; ++p1)
        for (PieceType p2 = PAWN; p2 <= KING; ++p2)
            if (p1 != p2 && (pieces(p1) & pieces(p2)))
                assert(0 && "pos_is_ok: Bitboards");

    for (Piece pc : Pieces)
        if (pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
            || pieceCount[pc] != std::count(board.begin(), board.end(), pc))
            assert(0 && "pos_is_ok: Pieces");

    for (Color c : {WHITE, BLACK})
        for (CastlingRights cr : {c & KING_SIDE, c & QUEEN_SIDE})
        {
            if (!can_castle(cr))
                continue;

            if (piece_on(castling_rook_square(cr)) != make_piece(c, ROOK)
                || castlingRightsMask[castlingRookSquare[cr]] != cr
                || (castlingRightsMask[square<KING>(c)] & cr) != cr)
                assert(0 && "pos_is_ok: Castling");
        }

    assert(material_key_is_ok() && "pos_is_ok: materialKey");

    return true;
}

}  // namespace Stockfish
