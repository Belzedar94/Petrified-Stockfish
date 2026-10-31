// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "position.h"
#include "movegen_reference.h"
#include "search_repetition.h"
#include "uci_move.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <vector>
using namespace Stockfish;
static void require(bool ok, const std::string& what) {
    if (!ok) { std::cerr << what << '\n'; std::exit(1); }
}
static std::string canonical(const Position& p) {
    std::istringstream in(p.fen()); std::string field, result;
    for(int i=0;in>>field;++i) if(i!=4 && i!=5) result+=field+" ";
    return result;
}
static std::unique_ptr<Eval::NNUE::Network> net;
static std::unique_ptr<Eval::NNUE::AccumulatorCaches> cache;
static std::unique_ptr<Eval::NNUE::AccumulatorStack> stack;
static std::ofstream output;
static bool weighted=false;
static size_t queries=0,children=0,wins=0,excludedChecks=0,nulls=0,nnue=0,exported=0;
static void eval(const Position& p) {
    if(weighted) {
        require(net->evaluate(p,*stack,*cache)==net->evaluate_reference(p),"NNUE mismatch");
        ++nnue;
    }
}
static void inspect(Position& p,const std::vector<std::string>& history,
                    const std::string& initial,const std::vector<std::string>& moves,bool emit) {
    if(Petrified::outcome(p,true).terminal()) return;
    eval(p);
    const auto fen=p.fen();const auto key=p.key();const auto* saved=p.state();
    unsigned char state[sizeof(StateInfo)];std::memcpy(state,saved,sizeof(StateInfo));
    Move list[MAX_MOVES];const Move* end=reference_generate<ALL>(p,list);
    std::vector<Move> expected;
    // Independent prefilter reference: count full canonical board/side/rights/EP/
    // points strings in the played history, without StateInfo repetition flags.
    for(const Move* m=list;m!=end;++m) {
        StateInfo child;p.do_move(*m,child);++children;
        const bool third=std::count(history.begin(),history.end(),canonical(p))>=2;
        const bool repetition=p.is_repetition(0);
        require(third==repetition,"canonical history disagrees with third repetition");
        const auto result=Petrified::outcome(p,true,8);
        if(third && result.termination==Petrified::Termination::ThreefoldRepetition && result.value<0)
            expected.push_back(*m);
        p.undo_move(*m);
    }
    const auto found=points_repetition_win(p,7);
    require((found.move!=Move::none())==!expected.empty(),"winner query mismatch: "+fen);
    if(found.move!=Move::none()) {
        require(std::find(expected.begin(),expected.end(),found.move)!=expected.end(),"unproven winning move");
        StateInfo child;p.do_move(found.move,child);
        require(found.value==-Petrified::outcome(p,true,8).value,"wrong distance/score");p.undo_move(found.move);
        const auto excluded=points_repetition_win(p,7,found.move);++excludedChecks;
        require(excluded.move!=found.move,"excluded singular-search move accepted");
        require((excluded.move!=Move::none())==(expected.size()>1),"excluded fallback mismatch");
        ++wins;
    }
    require(points_repetition_win(p,MAX_PLY).move==Move::none(),"max ply not guarded");
    require(p.fen()==fen && p.key()==key && p.state()==saved
            && !std::memcmp(state,saved,sizeof(StateInfo)),"probe changed parent state");
    eval(p);++queries;
    if(!p.checkers()) {
        StateInfo pass;p.do_null_move(pass);
        require(points_repetition_win(p,7).move==Move::none(),"null crossed real history");
        p.undo_null_move();++nulls;
        require(p.fen()==fen && p.key()==key && p.state()==saved
                && !std::memcmp(state,saved,sizeof(StateInfo)),"null undo changed state");
        eval(p);
    }
    if(emit && output) {
        output<<"{\"initial\":\""<<initial<<"\",\"history\":[";
        for(size_t i=0;i<moves.size();++i) output<<(i?",":"")<<'"'<<moves[i]<<'"';
        output<<"],\"fen\":\""<<fen<<"\",\"legal\":[";
        for(auto m=list;m!=end;++m) output<<(m==list?"":",")<<'"'<<UCI::move(*m)<<'"';
        output<<"],\"winning_repetitions\":[";
        for(size_t i=0;i<expected.size();++i) output<<(i?",":"")<<'"'<<UCI::move(expected[i])<<'"';
        output<<"]}\n";++exported;
    }
}
static void play(Position& p,std::deque<StateInfo>& states,std::vector<std::string>& history,
                 std::vector<std::string>& moves,const std::string& uci) {
    const auto m=UCI::to_move(p,uci);require(m!=Move::none(),"illegal fixture move "+uci+" "+p.fen());
    states.emplace_back();auto& dirty=stack->push(p);p.do_move(m,states.back(),p.gives_check(m),dirty);
    history.push_back(canonical(p));moves.push_back(uci);
}
int main(int argc,char** argv) {
    Attacks::init();Position::init();net=std::make_unique<Eval::NNUE::Network>();
    if(argc>1){output.open(argv[1]);require(bool(output),"case output");}
    if(argc>2){Eval::NNUE::EvalFile file;net->load({},argv[2],file);net->verify([](auto s){std::cout<<s<<'\n';},file,argv[2]);weighted=true;}
    cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*net);stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    const std::vector<std::string> boards={
        "3r3k/8/8/8/8/8/8/K2R4",
        "***r**1k/******11/********/********/********/********/1*******/K**R****",
        "3r3k/8/8/3*4/4*3/8/8/K2R4"};
    const std::vector<std::string> cycle={"a1a2","h8h7","a2a1","h7h8"};
    const std::vector<std::string> blackCycle={"h8h7","a1a2","h7h8","a2a1"};
    for(const auto& board:boards) for(const auto& side:{"w","b"})
      for(const auto& score:{"0,0","1,0","0,1","19,0","0,19","20,0","0,20","21,0","0,21"})
       for(int half:{0,80,92}) {
        std::string initial=board+" "+side+" - - "+std::to_string(half)+" 1 points="+score;
        Position p;std::deque<StateInfo> states(1);std::vector<std::string> moves;
        require(!p.set(initial,false,&states.back()),"fixture set");stack->reset();
        std::vector<std::string> history={canonical(p)};
        for(int i=0;i<8 && !Petrified::outcome(p,true).terminal();++i) {
            inspect(p,history,initial,moves,true);
            play(p,states,history,moves,(std::string(side)=="b"?blackCycle:cycle)[i%4]);
        }
        while(!moves.empty()) {
            // UCI::to_move requires the parent board; retain raw move from child squares.
            const std::string text=moves.back();
            Move m(Square(text[0]-'a'+8*(text[1]-'1')),Square(text[2]-'a'+8*(text[3]-'1')));
            p.undo_move(m);stack->pop();states.pop_back();moves.pop_back();history.pop_back();
            inspect(p,history,initial,moves,false);
        }
    }
    std::mt19937 rng(20261031);
    for(int game=0;game<24;++game) {
        const std::string initial="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points="+std::string(game%2?"1,0":"0,1");
        Position p;std::deque<StateInfo> states(1);std::vector<std::string> moves,history;
        std::vector<Move> raw;require(!p.set(initial,false,&states.back()),"walk set");stack->reset();history.push_back(canonical(p));
        for(int i=0;i<120 && !Petrified::outcome(p,true).terminal();++i) {
            inspect(p,history,initial,moves,i%24==0);
            Move legal[MAX_MOVES];auto end=reference_generate<ALL>(p,legal);const auto m=legal[rng()%(end-legal)];raw.push_back(m);
            play(p,states,history,moves,UCI::move(m));
        }
        while(!raw.empty()) {p.undo_move(raw.back());raw.pop_back();stack->pop();states.pop_back();moves.pop_back();history.pop_back();inspect(p,history,initial,moves,false);}
    }
    require(wins>0 && excludedChecks && nulls && queries>1000,"missing coverage");
    std::cout<<"{\"positions\":"<<queries<<",\"winning_repetitions\":"<<wins<<",\"reference_children\":"<<children
             <<",\"excluded_move_checks\":"<<excludedChecks<<",\"null_checks\":"<<nulls<<",\"nnue_exact\":"<<nnue<<",\"exported\":"<<exported<<"}\n";
}
