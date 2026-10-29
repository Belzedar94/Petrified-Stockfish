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
static void check_sparse_head() {
    // Independent output-major dot products cover every packed lane, empty
    // blocks, fully dense inputs and the signed-byte saturation boundaries.
    i8 weights[16][1024];
    Eval::NNUE::SparseHeadWeights packed;
    std::mt19937 random(20261029);
    std::size_t comparisons = 0;
    for (int bank = 0; bank < 3; ++bank) {
        for (auto& row : weights)
            for (auto& weight : row)
                weight = bank == 0 ? -128 : bank == 1 ? 127 : int(random() % 256) - 128;
        packed.pack(weights);
        for (int sample = 0; sample < 1284; ++sample) {
            u8 input[1024] = {};
            if (sample >= 1 && sample <= 1024) input[sample - 1] = 127;
            else if (sample > 1024)
                for (int j = 0; j < 1024; ++j)
                    input[j] = sample == 1025 ? 127
                             : sample == 1026 ? (j % 2 ? 127 : 0)
                             : sample == 1027 ? (j % 4 ? 0 : 127)
                             : random() % 16 < unsigned(sample % 16) ? random() % 128 : 0;
            i32 output[16];
            for (int i = 0; i < 16; ++i) output[i] = i % 2 ? 1 << 24 : -(1 << 24);
            Eval::NNUE::sparse_head_affine(input, packed, output);
            for (int i = 0; i < 16; ++i) {
                i64 expected = i % 2 ? 1 << 24 : -(1 << 24);
                for (int j = 0; j < 1024; ++j) expected += input[j] * weights[i][j];
                require(expected == output[i], "sparse-head dot product mismatch");
                ++comparisons;
            }
        }
    }
    std::cout << "Sparse-head output comparisons: " << comparisons << '\n';
}

static void check_head_buckets(const Eval::NNUE::Network& network) {
    const std::string initial = "rnbqkbnrpppppppp" + std::string(32, '.') + "PPPPPPPPRNBQKBNR";
    auto cache = std::make_unique<Eval::NNUE::AccumulatorCaches>(network);
    auto stack = std::make_unique<Eval::NNUE::AccumulatorStack>();
    unsigned buckets = 0;
    std::size_t checked = 0;
    for (int count = 2; count <= 32; ++count)
        for (char side : {'w', 'b'})
            for (int score : {0, 19, 20, 21, 63, 200})
                for (int clock : {0, 99, 100}) {
                    int remaining = count - 2;
                    std::string fen;
                    for (int rank = 0; rank < 8; ++rank) {
                        int empty = 0;
                        for (int file = 0; file < 8; ++file) {
                            char piece = initial[rank * 8 + file];
                            if (piece != 'K' && piece != 'k' && piece != '.') {
                                if (remaining) --remaining;
                                else piece = '.';
                            }
                            if (piece == '.') { ++empty; continue; }
                            if (empty) { fen += char('0' + empty); empty = 0; }
                            fen += piece;
                        }
                        if (empty) fen += char('0' + empty);
                        if (rank != 7) fen += '/';
                    }
                    fen += std::string(" ") + side + " - - " + std::to_string(clock)
                         + " 200 points=" + std::to_string(score) + ",0";
                    Position pos; StateInfo state;
                    require(!pos.set(fen, false, &state), "head bucket FEN");
                    require(network.evaluate(pos, *stack, *cache) == network.evaluate_reference(pos),
                            "head bucket/scoring/reference mismatch");
                    buckets |= 1U << std::min((count - 1) / 4, 7);
                    ++checked;
                }
    require(buckets == 255, "missing head bucket");
    std::cout << "Head bucket/score/clock comparisons: " << checked << '\n';
}
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

int main(int argc, char** argv) {
    check_sparse_head();
    Option variant("petrified var petrified", "petrified", nullptr);
    variant = std::string("petrified");
    require(variant == "petrified", "combo default must also be an allowed value");
    variant = std::string("chess");
    require(variant == "petrified", "combo must reject unsupported variants");
    variant = std::string("var");
    require(variant == "petrified", "combo must reject the UCI separator");
    Attacks::init(); Position::init();
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
        check_head_buckets(*network);
    }
    auto cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*network);
    auto stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    std::mt19937 rng(20261028);
    std::size_t checked=0;
    for(int game=0;game<60;++game) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                        + (game >= 30 ? " points=7,4" : ""),false,&states.back()),"initial FEN");
        std::vector<Move> played;
        std::vector<std::string> fens;
        std::vector<Key> keys;
        for(int ply=0;ply<160;++ply) {
            require(pos.pos_is_ok(),"state invariants");
            if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"incremental/reference mismatch");
            check_queries(pos, unsigned(game * 160 + ply));
            const auto legal=MoveList<ALL>(pos);
            if(!legal.size()) break;
            Move m=legal.begin()[rng()%legal.size()];
            fens.push_back(pos.fen()); keys.push_back(pos.key()); played.push_back(m);
            const bool check=pos.gives_check(m);
            states.emplace_back(); pos.do_move(m,states.back());
            require(check==bool(pos.checkers()),"gives_check differs from resulting state");
            Position rebuilt; StateInfo rebuiltState;
            require(!rebuilt.set(pos.fen(),false,&rebuiltState),"FEN roundtrip rejected");
            require(rebuilt.key()==pos.key(),"incremental/rebuilt key mismatch");
            require(rebuilt.pawn_key()==pos.pawn_key(),"pawn key mismatch");
            ++checked;
        }
        while(!played.empty()) {
            pos.undo_move(played.back()); played.pop_back(); states.pop_back();
            require(pos.fen()==fens.back() && pos.key()==keys.back(),"undo did not restore position");
            if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"undo accumulator mismatch");
            fens.pop_back(); keys.pop_back();
        }
    }
    std::cout<<"Legal query comparisons: "<<queryChecks<<'\n';
    std::cout<<"State/undo checks: "<<checked<<"; NNUE: "<<(argc>1?"exact":"not requested")<<'\n';
}
