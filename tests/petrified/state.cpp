// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "nnue/legacy_sha256.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
using namespace Stockfish;
static void require(bool ok, const char* what) {
    if (!ok) { std::cerr << what << '\n'; std::exit(1); }
}
int main(int argc, char** argv) {
    Attacks::init(); Position::init();
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
    std::size_t checked=0;
    for(int game=0;game<30;++game) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",false,&states.back()),"initial FEN");
        std::vector<Move> played;
        std::vector<std::string> fens;
        std::vector<Key> keys;
        for(int ply=0;ply<160;++ply) {
            require(pos.pos_is_ok(),"state invariants");
            if(argc>1) require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"incremental/reference mismatch");
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
    std::cout<<"State/undo checks: "<<checked<<"; NNUE: "<<(argc>1?"exact":"not requested")<<'\n';
}
