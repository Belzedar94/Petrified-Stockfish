// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movegen_reference.h"
#include "search_settlement.h"
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
#include <vector>
using namespace Stockfish;
static void require(bool ok,const std::string& why) {
    if(!ok) {std::cerr<<why<<'\n';std::exit(1);}
}
struct Harness {
    std::unique_ptr<Eval::NNUE::Network> net=std::make_unique<Eval::NNUE::Network>();
    std::unique_ptr<Eval::NNUE::AccumulatorCaches> cache;
    std::unique_ptr<Eval::NNUE::AccumulatorStack> stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    std::ofstream out; bool weighted=false;
    size_t positions=0,children=0,events=0,probes=0,wins=0,quietChildren=0,
           nulls=0,nnue=0,ep=0,promotions=0,castles=0,terminals=0,exports=0,exclusions=0;
    void eval(const Position& p) {
        if(weighted) {require(net->evaluate(p,*stack,*cache)==net->evaluate_reference(p),"NNUE differs");++nnue;}
    }
    std::vector<Move> legal(const Position& p) {
        Move all[MAX_MOVES];auto end=reference_generate<ALL>(p,all);MoveList<ALL> fast(p);
        require(fast.size()==size_t(end-all) && std::equal(fast.begin(),fast.end(),all),"legal reference mismatch");
        return {all,end};
    }
    void array(const std::vector<Move>& moves) {
        out<<'[';for(size_t i=0;i<moves.size();++i) out<<(i?",":"")<<'"'<<UCI::move(moves[i])<<'"';out<<']';
    }
    // Exhaustive reference follows independent legal generation, real make/undo
    // and the existing rule oracle. The exported sets are checked by Chess.com.
    std::vector<Move> checkProbe(Position& p, bool expected, bool emit) {
        const auto fen=p.fen();const auto key=p.key();auto* saved=p.state();
        unsigned char bytes[sizeof(StateInfo)];std::memcpy(bytes,saved,sizeof(StateInfo));
        const auto end=Petrified::outcome(p,true);std::vector<Move> winning;
        require(Search::settlement_probe_eligible(p)==expected,"event context mismatch");
        if(end.terminal()) return winning;
        const auto legalMoves=legal(p);Value best=-VALUE_INFINITE;Move bestMove=Move::none();
        eval(p);
        if(expected) for(Move m:legalMoves) if(!p.capture_stage(m)) {
            StateInfo child;auto& dirty=stack->push(p);p.do_move(m,child,p.gives_check(m),dirty);
            const auto result=Petrified::outcome(p,true,8);++quietChildren;eval(p);
            p.undo_move(m);stack->pop();
            if(result.terminal() && is_win(-result.value)) {
                winning.push_back(m);
                if(-result.value>best) {best=-result.value;bestMove=m;}
            }
        }
        const auto actual=Search::winning_quiet_settlement(p,7);++probes;
        require(actual.move==bestMove && actual.value==best,"terminal probe differs "+fen);
        require(p.fen()==fen && p.key()==key && p.state()==saved && !std::memcmp(bytes,saved,sizeof(StateInfo)),"probe changed state");
        eval(p);wins+=actual.move!=Move::none();
        if(actual.move!=Move::none()) {
            Value other=-VALUE_INFINITE;Move otherMove=Move::none();
            for(Move m:winning) if(m!=actual.move) {
                StateInfo child;p.do_move(m,child);const auto o=Petrified::outcome(p,true,8);p.undo_move(m);
                if(-o.value>other) {other=-o.value;otherMove=m;}
            }
            const auto excluded=Search::winning_quiet_settlement(p,7,actual.move);++exclusions;
            require(excluded.move==otherMove && excluded.value==other,"excluded move escaped");
        }
        if(expected) {
            Position rebuilt;StateInfo rs;require(!rebuilt.set(fen,false,&rs),"FEN rebuild");
            require(rebuilt.key()==key && !Search::settlement_probe_eligible(rebuilt),"FEN invented event");
            require(Search::winning_quiet_settlement(rebuilt,7).move==Move::none(),"context-free probe activated");
        }
        if(!p.checkers()) {
            StateInfo pass;p.do_null_move(pass);
            require(!Search::settlement_probe_eligible(p) && Search::winning_quiet_settlement(p,7).move==Move::none(),"null inherited event");
            p.undo_null_move();++nulls;
            require(p.fen()==fen && p.key()==key && p.state()==saved && !std::memcmp(bytes,saved,sizeof(StateInfo)),"null undo changed state");
            eval(p);
        }
        if(emit && out) {
            out<<",\"eligible\":"<<(expected?"true":"false")<<",\"winning_quiets\":";array(winning);
            out<<",\"proof_move\":\""<<UCI::move(actual.move)<<"\",\"proof_value\":"<<actual.value;
        }
        return winning;
    }
    void inspect(Position& p,const std::string& initial,const std::vector<std::string>& history,bool emit) {
        if(Petrified::outcome(p,true).terminal()) return;
        ++positions;const auto fen=p.fen();const auto key=p.key();auto* saved=p.state();
        unsigned char bytes[sizeof(StateInfo)];std::memcpy(bytes,saved,sizeof(StateInfo));
        const auto moves=legal(p);
        const bool context=Search::settlement_probe_eligible(p);
        checkProbe(p,context,false);
        if(emit && out) {
            out<<"{\"initial\":\""<<initial<<"\",\"history\":[";
            for(size_t i=0;i<history.size();++i)out<<(i?",":"")<<'"'<<history[i]<<'"';
            out<<"],\"fen\":\""<<fen<<"\",\"legal\":";array(moves);out<<",\"children\":[";
        }
        for(size_t i=0;i<moves.size();++i) {
            const Move m=moves[i];const auto walls=p.walls();const int count=popcount(p.pieces());
            const Piece mover=p.moved_piece(m);const bool capture=p.capture(m);
            ep+=m.type_of()==EN_PASSANT;promotions+=m.type_of()==PROMOTION;castles+=m.type_of()==CASTLING;
            StateInfo child;auto& dirty=stack->push(p);p.do_move(m,child,p.gives_check(m),dirty);++children;
            const bool newWall=p.walls()!=walls;
            require(newWall==(capture && type_of(mover)!=PAWN),"wall transition mismatch");
            if(newWall)require(popcount(p.pieces())==count-2,"petrification piece count");
            const bool expected=p.game_of_points() && newWall && !p.checkers();events+=expected;
            const auto end=Petrified::outcome(p,true);terminals+=end.terminal();
            if(emit && out)out<<(i?",":"")<<"{\"move\":\""<<UCI::move(m)<<"\",\"fen\":\""<<p.fen()
                <<"\",\"terminal\":"<<(end.terminal()?"true":"false")<<",\"result\":\""<<end.result()<<"\"";
            checkProbe(p,expected,emit);
            if(emit && out)out<<'}';
            p.undo_move(m);stack->pop();
            require(p.fen()==fen && p.key()==key && p.state()==saved && !std::memcmp(bytes,saved,sizeof(StateInfo)),"child undo changed state");
            if(expected)eval(p);
        }
        if(emit && out) {out<<"]}\n";++exports;}
    }
};
int main(int argc,char** argv) {
    Attacks::init();Position::init();Harness h;
    if(argc>1) {h.out.open(argv[1]);require(bool(h.out),"output");}
    if(argc>2) {Eval::NNUE::EvalFile f;h.net->load({},argv[2],f);h.net->verify([](auto s){std::cout<<s<<'\n';},f,argv[2]);h.weighted=true;}
    h.cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*h.net);
    const std::vector<std::string> fixtures={
        "7k/8/8/8/8/3q4/1*5p/K*5R w - - 0 1",
        "k*5r/1*5P/3Q4/8/8/8/8/7K b - - 0 1",
        "7k/7n/8/8/8/3q2P1/1*5p/K*5R w - - 0 1",
        "8/7k/8/3p4/3Q4/8/8/K7 w - - 0 1",
        "7k/8/8/3q4/2P5/8/8/K7 w - - 0 1",
        "k6r/6P1/8/8/8/8/8/K7 w - - 0 1",
        "7k/8/8/8/8/8/6p1/K6R b - - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "7k/8/8/3n4/8/3R4/8/K7 w - - 99 1",
        "***r***k/*******1/********/********/********/********/1*******/K**R**** w - - 99 1"};
    for(const auto& board:fixtures)for(const auto& score:{""," points=0,0"," points=19,0"," points=0,19"," points=21,0"," points=0,21"}) {
        const std::string initial=board+score;Position p;std::deque<StateInfo> states(1);h.stack->reset();
        require(!p.set(initial,false,&states.back()),"fixture "+initial);
        h.inspect(p,initial,{},p.game_of_points());
        for(Move m:MoveList<ALL>(p))if(p.capture(m)) {
            StateInfo child;auto& dirty=h.stack->push(p);p.do_move(m,child,p.gives_check(m),dirty);
            h.inspect(p,initial,{UCI::move(m)},p.game_of_points());p.undo_move(m);h.stack->pop();
        }
    }
    std::mt19937 rng(20261101);
    for(int game=0;game<32;++game) {
        const std::string start="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"+std::string(game%8?" points=0,0":"");
        Position p;std::deque<StateInfo> states(1);std::vector<Move> played;std::vector<std::string> history;h.stack->reset();
        require(!p.set(start,false,&states.back()),"random start");
        for(int ply=0;ply<144 && !Petrified::outcome(p,true).terminal();++ply) {
            h.inspect(p,start,history,game<5 && p.game_of_points() && ply%11==0);
            MoveList<ALL> moves(p);if(!moves.size())break;Move m=moves.begin()[rng()%moves.size()];
            history.push_back(UCI::move(m));played.push_back(m);states.emplace_back();
            auto& dirty=h.stack->push(p);p.do_move(m,states.back(),p.gives_check(m),dirty);
        }
        while(!played.empty()) {p.undo_move(played.back());h.stack->pop();played.pop_back();states.pop_back();h.eval(p);}
        require(p.fen()==start,"walk undo failed");
    }
    require(h.events && h.wins && h.exclusions && h.ep && h.promotions && h.castles && h.terminals,"coverage missing");
    std::cout<<"{\"result\":\"PASS\",\"positions\":"<<h.positions<<",\"reference_children\":"<<h.children
        <<",\"events\":"<<h.events<<",\"rule_probes\":"<<h.probes<<",\"winning_probes\":"<<h.wins
        <<",\"quiet_reference_children\":"<<h.quietChildren<<",\"null_checks\":"<<h.nulls<<",\"exact_nnue\":"<<h.nnue
        <<",\"exclusions\":"<<h.exclusions<<",\"en_passant\":"<<h.ep<<",\"promotions\":"<<h.promotions
        <<",\"castles\":"<<h.castles<<",\"terminal_children\":"<<h.terminals<<",\"exported\":"<<h.exports<<"}\n";
}
