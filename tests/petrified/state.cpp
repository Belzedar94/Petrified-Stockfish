// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "nnue/legacy_sha256.h"
#include "ucioption.h"
#include "api/petrified_outcome.h"
#include "score.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;
static void require(bool ok, const char* what) {
    if (!ok) { std::cerr << what << '\n'; std::exit(1); }
}
static std::size_t queryChecks = 0;
static std::size_t rebuiltChecks = 0;
static std::size_t specialChecks[4] = {};
static std::size_t wallCaptures = 0;

// set() still uses the unchanged full-board recomputation from the champion.
// Compare all search/history keys, not just the main transposition-table key.
static void check_rebuilt(const Position& pos) {
    Position rebuilt; StateInfo state;
    require(!rebuilt.set(pos.fen(), pos.is_chess960(), &state), "FEN rebuild rejected");
    const auto* actual = pos.state();
    require(actual->key == state.key && pos.key() == rebuilt.key(), "main key mismatch");
    require(actual->materialKey == state.materialKey, "material key mismatch");
    require(actual->pawnKey == state.pawnKey, "pawn key mismatch");
    require(actual->minorPieceKey == state.minorPieceKey, "minor key mismatch");
    require(actual->checkersBB == state.checkersBB, "rebuilt checkers mismatch");
    require(actual->castlingRights == state.castlingRights
            && actual->epSquares == state.epSquares && actual->epSquare == state.epSquare,
            "rights/EP mismatch");
    for (Color side : {WHITE, BLACK}) {
        require(actual->nonPawnKey[side] == state.nonPawnKey[side], "non-pawn key mismatch");
        require(actual->nonPawnMaterial[side] == state.nonPawnMaterial[side], "material value mismatch");
        require(actual->points[side] == state.points[side], "score mismatch");
        require(actual->blockersForKing[side] == state.blockersForKing[side]
                && actual->pinners[side] == state.pinners[side], "blocker/pinner mismatch");
    }
    for (PieceType pt = PAWN; pt <= KING; ++pt)
        require(actual->checkSquares[pt] == state.checkSquares[pt], "check squares mismatch");
    ++rebuiltChecks;
}

static void check_children(Position& pos, int depth) {
    check_rebuilt(pos);
    if (!depth) return;
    const std::string before = pos.fen();
    const Key key = pos.key();
    for (Move move : MoveList<ALL>(pos)) {
        ++specialChecks[int(move.type_of()) >> 14];
        if (pos.capture(move) && type_of(pos.moved_piece(move)) != PAWN) ++wallCaptures;
        StateInfo child;
        pos.do_move(move, child);
        check_children(pos, depth - 1);
        pos.undo_move(move);
        require(pos.fen() == before && pos.key() == key, "special move undo mismatch");
        check_rebuilt(pos);
    }
}

static void check_incremental_fixtures() {
    for (bool points : {false, true})
        for (const char* fen : {
            "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
            "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
            "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
            "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
            "4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1",
            "4k3/8/8/8/4p3/8/3P4/4K3 w - - 0 1",
            "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",
            "4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1",
            "4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1",
            "3rk3/8/8/8/3N4/8/8/4K3 b - - 0 1",
            "4k3/8/2*5/3n4/5*2/8/8/3RK3 w - - 99 1"
        }) {
            Position pos; StateInfo state;
            require(!pos.set(std::string(fen) + (points ? " points=7,4" : ""), false, &state),
                    "incremental fixture FEN");
            check_children(pos, 2);
        }
    for (const char* fen : {
        "4k3/8/8/8/8/8/8/2R2K1R w HC - 0 1",
        "4k3/8/8/8/8/8/8/6KR w H - 0 1",
        "4k3/8/8/8/8/8/8/4KR2 w F - 0 1"
    }) {
        Position pos; StateInfo state;
        require(!pos.set(fen, true, &state), "Chess960 incremental fixture FEN");
        check_children(pos, 2);
    }
    for (bool points : {false, true}) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set(std::string("4k3/8/8/8/8/8/8/4K3 w - - 0 1")
                         + (points ? " points=7,4" : ""), false, &states.back()), "repetition FEN");
        for (int cycle = 0; cycle < 2; ++cycle)
            for (Move move : {Move(SQ_E1, SQ_D1), Move(SQ_E8, SQ_D8),
                              Move(SQ_D1, SQ_E1), Move(SQ_D8, SQ_E8)}) {
                require(pos.legal(move), "repetition move legality");
                states.emplace_back(); pos.do_move(move, states.back());
                check_rebuilt(pos);
            }
        require(pos.is_repetition(0), "third occurrence lost after incremental updates");
    }
    require(specialChecks[1] && specialChecks[2] && specialChecks[3] && wallCaptures,
            "special move coverage absent");
}
static std::size_t safetyFixturePositions = 0;
template<GenType Type>
static void check_move_order(const Position& pos) {
    Move expected[MAX_MOVES];
    Move* end = reference_generate<Type>(pos, expected);
    const auto actual = MoveList<Type>(pos);
    require(actual.size() == std::size_t(end - expected)
            && std::equal(actual.begin(), actual.end(), expected), "frozen move list/order mismatch");
}
static void check_queries(const Position& pos, unsigned salt, bool exhaustive = false) {
    Move expected[MAX_MOVES];
    Move* end = reference_generate<ALL>(pos, expected);
    auto contains = [&](Move m) { return std::find(expected, end, m) != end; };
    require(has_legal_move(pos) == (end != expected), "legal-existence query mismatch");
    require(unique_legal_move(pos) == (end == expected + 1 ? expected[0] : Move::none()),
            "unique legal-move query mismatch");
    check_move_order<ALL>(pos); check_move_order<CAPTURES>(pos);
    check_move_order<QUIETS>(pos); check_move_order<EVASIONS>(pos);
    for (const Move* m = expected; m != end; ++m) {
        require(pos.legal(*m), "generated legal move rejected by query"); ++queryChecks;
    }
    for (unsigned i = 0; i < (exhaustive ? 65536U : 24U); ++i) {
        const Move m(u16(exhaustive ? i : ((i * 40503U + salt * 79U) & 65535U)));
        require(pos.legal(m) == (m.is_ok() && contains(m)), "move-membership query mismatch");
        ++queryChecks;
    }
    require(!pos.legal(Move::none()) && !pos.legal(Move::null()), "sentinel move accepted");
}

// reference_generate retains a full safe_after() call for every candidate.
// Descendants cover changed pins, capture-created walls and undo, not just roots.
static void check_safety_tree(Position& pos, int depth) {
    check_queries(pos, unsigned(++safetyFixturePositions));
    if (!depth) return;
    const std::string before = pos.fen();
    const Key key = pos.key();
    Move moves[MAX_MOVES];
    Move* end = reference_generate<ALL>(pos, moves);
    for (Move* move = moves; move != end; ++move) {
        StateInfo child;
        pos.do_move(*move, child);
        check_safety_tree(pos, depth - 1);
        pos.undo_move(*move);
        require(pos.fen() == before && pos.key() == key, "safety fixture undo mismatch");
    }
}

static void check_safety_fixtures() {
    for (bool points : {false, true})
        for (const char* fen : {
            "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1",
            "4r1k1/8/8/8/8/8/4P3/4K3 w - - 0 1",
            "4k3/4p3/8/8/8/8/8/4R1K1 b - - 0 1",
            "6k1/8/8/8/1b6/8/3R4/4K3 w - - 0 1",
            "4r1k1/8/8/8/8/8/4*3/3RK3 w - - 0 1",
            "4r1k1/8/8/8/4r3/8/4R3/4K3 w - - 0 1",
            "6k1/8/8/3n4/8/8/8/3RK3 w - - 0 1",
            "4r1k1/8/8/8/8/8/3R4/4K3 w - - 0 1",
            "4r1k1/8/8/8/1b6/8/3R4/4K3 w - - 0 1",
            "6k1/8/8/8/8/5n2/3R4/4K3 w - - 0 1",
            "7k/8/8/r4pPK/8/8/8/8 w - f6 0 1",
            "8/8/8/8/R4Ppk/8/8/7K b - f3 0 1",
            "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
            "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",
            "4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1",
            "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
            "4k3/8/8/8/8/8/4K3/3R4 w - - 0 1"
        }) {
            Position pos; StateInfo state;
            require(!pos.set(std::string(fen) + (points ? " points=7,4" : ""), false, &state),
                    "king-safety fixture FEN");
            check_safety_tree(pos, 2);
        }
    for (const char* fen : {
        "4k3/8/8/8/8/8/8/2R2K1R w HC - 0 1",
        "4k3/8/8/8/8/8/8/6KR w H - 0 1",
        "4k3/8/8/8/8/8/8/4KR2 w F - 0 1"
    }) {
        Position pos; StateInfo state;
        require(!pos.set(fen, true, &state), "Chess960 king-safety fixture FEN");
        check_safety_tree(pos, 2);
    }
}

int main(int argc, char** argv) {
    Option variant("petrified var petrified", "petrified", nullptr);
    variant = std::string("petrified");
    require(variant == "petrified", "combo default must also be an allowed value");
    variant = std::string("chess");
    require(variant == "petrified", "combo must reject unsupported variants");
    variant = std::string("var");
    require(variant == "petrified", "combo must reject the UCI separator");
    Attacks::init(); Position::init();
    check_incremental_fixtures();
    check_safety_fixtures();
    for (const char* fen : {
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1 points=0,0",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1 points=0,0",
        "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1 points=0,0",
        "4r1k1/8/8/8/8/8/4P3/4K3 w - - 0 1 points=0,0",
        "*4r2/2p1qpkQ/2*2*P1/1p2p3/1P2P1*1/8/2P5/1N2K1NR b K - 4 22 points=0,0"
    }) {
        Position pos; StateInfo state;
        require(!pos.set(fen,false,&state), "query fixture FEN");
        check_queries(pos,0,true);
    }
    for (bool whiteMates : {true, false}) {
        const std::string board = whiteMates
          ? "*4r2/2p1qpkQ/2*2*P1/1p2p3/1P2P1*1/8/2P5/1N2K1NR b K - 4 22"
          : "r3kb1r/3*4/4**2/6p1/p3*3/P5PP/1P*4K/2R1*1q1 w kq - 2 36";
        for (int deficit : {19,20,21}) {
            Position pos; StateInfo state;
            const auto scores = whiteMates ? "0," + std::to_string(deficit)
                                          : std::to_string(deficit) + ",0";
            require(!pos.set(board+" points="+scores,false,&state),"mate boundary FEN");
            require(MoveList<ALL>(pos).size()==0 && pos.checkers(),"boundary position is mate");
            for (int ply : {0,1,5,MAX_PLY-1}) {
                const auto end=Petrified::outcome(pos,true,ply);
                const Value expected=deficit==19 ? mated_in(ply)
                                    : deficit==20 ? VALUE_DRAW : VALUE_TB-ply;
                require(end.termination==Petrified::Termination::Checkmate,"mate reason");
                require(end.value==expected,"mate score must follow final points");
                require(pos.no_moves_value(ply)==expected,"terminal score paths agree");
                require(Score(expected,pos).is<Score::Mate>()==(deficit==19),"only winning mates use UCI mate scores");
            }
        }
    }
    for (int lead : {0,20,21}) {
        Position pos; StateInfo state;
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points="
                         +std::to_string(lead)+",0",false,&state),"claim FEN");
        require(Petrified::resignation_value(pos)==(lead==0 ? -VALUE_INFINITE
                  : lead==20 ? VALUE_DRAW : VALUE_TB),"only non-losing resignations are search claims");
    }
    require(Eval::NNUE::sha256({'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 known vector");
    auto network=std::make_unique<Eval::NNUE::Network>();
    Eval::NNUE::EvalFile file;
    if(argc>1) {
        network->load({},argv[1],file);
        network->verify([](auto s){std::cout<<s<<'\n';},file,argv[1]);
    }
    auto cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*network);
    auto stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    std::mt19937 rng(20261028);
    std::size_t checked=0, nullChecked=0, nullEpChecked=0;
    for(int game=0;game<60;++game) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                        + (game >= 30 ? " points=7,4" : ""),false,&states.back()),"initial FEN");
        std::vector<Move> played;
        std::vector<std::string> fens;
        std::vector<Key> keys;
        for(int ply=0;ply<160;++ply) {
            require(pos.pos_is_ok(),"state invariants");
            check_rebuilt(pos);
            if (ply % 16 == 0) check_children(pos, 1);
            if (ply % 16 == 0 && !pos.checkers()) {
                const std::string beforeNull = pos.fen(); const Key beforeKey = pos.key();
                StateInfo nullState; pos.do_null_move(nullState);
                check_rebuilt(pos);
                pos.undo_null_move();
                require(pos.fen() == beforeNull && pos.key() == beforeKey, "null restoration mismatch");
                check_rebuilt(pos);
            }
            if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"incremental/reference mismatch");
            check_queries(pos, unsigned(game * 160 + ply));
            if (!pos.checkers() && ply % 4 == 0) {
                const auto before = pos.fen();
                const auto beforeKey = pos.key();
                const auto side = pos.side_to_move();
                const int whitePoints = pos.points(WHITE), blackPoints = pos.points(BLACK);
                const int rule50 = pos.rule50_count();
                const auto beforeEp = pos.ep_squares();
                StateInfo nullState;
                pos.do_null_move(nullState);
                check_rebuilt(pos);
                check_queries(pos, unsigned(game * 160 + ply));
                require(pos.pos_is_ok() && pos.side_to_move() == ~side,"null side/state");
                require(pos.points(WHITE) == whitePoints && pos.points(BLACK) == blackPoints,
                        "null must preserve earned points");
                require(pos.rule50_count() == rule50 && !pos.ep_squares(),"null counters/ep");
                require(!pos.is_repetition(0),"null must isolate earlier repetition history");
                Position rebuilt; StateInfo rebuiltState;
                require(!rebuilt.set(pos.fen(),false,&rebuiltState),"null FEN roundtrip");
                require(rebuilt.key() == pos.key(),"null incremental/rebuilt key");
                if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),
                                   "null accumulator mismatch");
                const auto afterNull = pos.fen();
                const auto nullLegal = MoveList<ALL>(pos);
                if (nullLegal.size()) {
                    const Move child = *nullLegal.begin();
                    StateInfo childState;
                    pos.do_move(child,childState);
                    check_rebuilt(pos);
                    check_queries(pos, unsigned(game * 160 + ply));
                    require(pos.pos_is_ok(),"real move below null state");
                    if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),
                                       "real move below null accumulator");
                    pos.undo_move(child);
                    check_rebuilt(pos);
                    require(pos.fen() == afterNull,"undo real move below null");
                }
                pos.undo_null_move();
                check_rebuilt(pos);
                require(pos.fen() == before && pos.key() == beforeKey,"undo null must restore all state");
                if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),
                                   "undo null accumulator mismatch");
                ++nullChecked;
                nullEpChecked += bool(beforeEp);
            }
            const auto legal=MoveList<ALL>(pos);
            if(!legal.size()) break;
            Move m=legal.begin()[rng()%legal.size()];
            fens.push_back(pos.fen()); keys.push_back(pos.key()); played.push_back(m);
            const bool check=pos.gives_check(m);
            states.emplace_back(); pos.do_move(m,states.back());
            require(check==bool(pos.checkers()),"gives_check differs from resulting state");
            check_rebuilt(pos);
            ++checked;
        }
        while(!played.empty()) {
            pos.undo_move(played.back()); played.pop_back(); states.pop_back();
            require(pos.fen()==fens.back() && pos.key()==keys.back(),"undo did not restore position");
            check_rebuilt(pos);
            if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"undo accumulator mismatch");
            fens.pop_back(); keys.pop_back();
        }
    }
    std::cout<<"Legal query comparisons: "<<queryChecks<<'\n';
    require(nullChecked > 100 && nullEpChecked > 0,"null and en passant coverage");
    std::cout<<"State/undo checks: "<<checked<<"; null: "<<nullChecked<<"; null EP: "<<nullEpChecked
             <<"; NNUE: "<<(argc>1?"exact":"not requested")<<'\n';
    std::cout<<"Full-state rebuild comparisons: "<<rebuiltChecks
             <<"; promotions/EP/castles/wall captures: "<<specialChecks[1]<<'/'
             <<specialChecks[2]<<'/'<<specialChecks[3]<<'/'<<wallCaptures<<'\n';
    std::cout<<"King-safety fixture positions: "<<safetyFixturePositions<<'\n';
}
