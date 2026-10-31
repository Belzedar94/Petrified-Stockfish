// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movegen_reference.h"
#include "search_liquidation.h"
#include "movepick.h"
#include "history.h"
#include "uci_move.h"
#include "api/petrified_outcome.h"
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
static void require(bool ok, const std::string& why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
struct Harness {
    std::unique_ptr<Eval::NNUE::Network> net=std::make_unique<Eval::NNUE::Network>();
    std::unique_ptr<Eval::NNUE::AccumulatorCaches> cache;
    std::unique_ptr<Eval::NNUE::AccumulatorStack> stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    std::unique_ptr<ButterflyHistory> main=std::make_unique<ButterflyHistory>();
    std::unique_ptr<LowPlyHistory> low=std::make_unique<LowPlyHistory>();
    std::unique_ptr<CapturePieceToHistory> cap=std::make_unique<CapturePieceToHistory>();
    std::unique_ptr<PieceToHistory> cont=std::make_unique<PieceToHistory>();
    std::unique_ptr<SharedHistories> shared=std::make_unique<SharedHistories>(1);
    std::ofstream out;
    bool weighted=false;
    size_t positions=0, children=0, events=0, pickers=0, nulls=0, nnue=0,
           ep=0, promotions=0, castles=0, rearms=0, terminals=0, exports=0;
    void eval(const Position& p) {
        if (weighted) {
            require(net->evaluate(p,*stack,*cache)==net->evaluate_reference(p),"NNUE differs");++nnue;
        }
    }
    std::vector<Move> legal(const Position& p) {
        Move all[MAX_MOVES];auto end=reference_generate<ALL>(p,all);
        MoveList<ALL> fast(p);
        require(fast.size()==size_t(end-all) && std::equal(fast.begin(),fast.end(),all),"legal reference mismatch");
        return {all,end};
    }
    void array(const std::vector<Move>& moves) {
        out<<'[';for(size_t i=0;i<moves.size();++i) out<<(i?",":"")<<'"'<<UCI::move(moves[i])<<'"';out<<']';
    }
    void picker(const Position& p,const std::vector<Move>& expected) {
        const PieceToHistory* ch[]={cont.get(),cont.get(),cont.get(),cont.get(),cont.get(),cont.get()};
        // Cold, legal quiet/capture TT moves and an invalid TT move. Every
        // complete picker must emit exactly the independent legal set once.
        std::vector<Move> tt={Move::none(),Move(SQ_A1,SQ_A8)};
        if(!expected.empty()) {tt.push_back(expected.front());tt.push_back(expected.back());}
        auto sorted=expected;auto less=[](Move a,Move b){return a.raw()<b.raw();};std::sort(sorted.begin(),sorted.end(),less);
        for(Move t:tt) {
            MovePicker mp(p,t,Depth(1),main.get(),low.get(),cap.get(),ch,shared.get(),3);
            std::vector<Move> actual;for(Move m=mp.next_move();m!=Move::none();m=mp.next_move()) actual.push_back(m);
            std::sort(actual.begin(),actual.end(),less);require(actual==sorted,"full reply picker mismatch");++pickers;
        }
    }
    void inspect(Position& p,const std::string& initial,const std::vector<std::string>& history,bool emit) {
        if(Petrified::outcome(p,true).terminal()) return;
        const auto fen=p.fen();const auto key=p.key();const auto* saved=p.state();
        unsigned char bytes[sizeof(StateInfo)];std::memcpy(bytes,saved,sizeof(StateInfo));
        const auto moves=legal(p);const bool previousEvent=Search::liquidation_reply(p);eval(p);++positions;
        if(emit && out) {
            out<<"{\"initial\":\""<<initial<<"\",\"history\":[";
            for(size_t i=0;i<history.size();++i) out<<(i?",":"")<<'"'<<history[i]<<'"';
            out<<"],\"fen\":\""<<fen<<"\",\"legal\":";array(moves);out<<",\"children\":[";
        }
        for(size_t i=0;i<moves.size();++i) {
            const Move m=moves[i];const auto oldWalls=p.walls();const int oldCount=popcount(p.pieces());
            const Piece mover=p.moved_piece(m);const bool capture=p.capture(m);
            ep+=m.type_of()==EN_PASSANT;promotions+=m.type_of()==PROMOTION;castles+=m.type_of()==CASTLING;
            StateInfo child;auto& dirty=stack->push(p);p.do_move(m,child,p.gives_check(m),dirty);++children;
            // Board-delta reference, independent of the saved petrified flag.
            const bool newWall=(p.walls()!=oldWalls);
            require(newWall==(capture && type_of(mover)!=PAWN),"wall transition mismatch");
            if(newWall) require(popcount(p.pieces())==oldCount-2,"petrification must consume two live pieces");
            const bool expected=p.game_of_points() && newWall && !p.checkers();
            require(Search::liquidation_reply(p)==expected,"reply obligation mismatch "+p.fen());
            require(Search::liquidation_qsearch_key(p,false)==p.key(),"ordinary key changed");
            require(Search::liquidation_qsearch_key(p,true)!=p.key(),"reply TT domains collide");
            const auto result=Petrified::outcome(p,true);terminals+=result.terminal();
            std::vector<Move> replies;
            if(expected) {
                ++events;rearms+=previousEvent;
                if(!result.terminal()) { replies=legal(p);picker(p,replies); }
                eval(p);
                Position rebuilt;StateInfo rs;require(!rebuilt.set(p.fen(),false,&rs),"FEN rebuild");
                require(rebuilt.key()==p.key() && !Search::liquidation_reply(rebuilt),"FEN invented last move");
                eval(rebuilt);eval(p);
            }
            if(!p.checkers()) {
                const auto cf=p.fen();const auto ck=p.key();const auto* cs=p.state();
                StateInfo pass;p.do_null_move(pass);
                require(!Search::liquidation_reply(p),"null inherited wall obligation");
                p.undo_null_move();++nulls;
                require(p.fen()==cf && p.key()==ck && p.state()==cs && Search::liquidation_reply(p)==expected,"null undo failed");
            }
            if(emit && out) {
                out<<(i?",":"")<<"{\"move\":\""<<UCI::move(m)<<"\",\"fen\":\""<<p.fen()
                   <<"\",\"reply\":"<<(expected?"true":"false")<<",\"terminal\":"<<(result.terminal()?"true":"false")
                   <<",\"result\":\""<<result.result()<<"\",\"reason\":\""<<result.reason()<<"\",\"legal_replies\":";
                array(replies);out<<'}';
            }
            p.undo_move(m);stack->pop();
            require(p.fen()==fen && p.key()==key && p.state()==saved && !std::memcmp(bytes,saved,sizeof(StateInfo)),"child/undo changed state");
            if(expected) eval(p);
        }
        if(emit && out) {out<<"]}\n";++exports;}
        require(Search::liquidation_reply(p)==previousEvent,"probe changed event context");
    }
};
int main(int argc,char** argv) {
    Attacks::init();Position::init();Harness h;
    h.main->fill(0);h.low->fill(0);h.cap->fill(0);h.cont->fill(0);
    h.shared->pawnHistory.fill(0);
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
    for(const auto& board:fixtures) for(const auto& score:{"", " points=0,0"," points=19,0"," points=0,19"," points=21,0"," points=0,21"}) {
        const std::string start=board+score;Position p;std::deque<StateInfo> states(1);h.stack->reset();
        require(!p.set(start,false,&states.back()),"fixture "+start);
        h.inspect(p,start,{},p.game_of_points());
        // Inspect every actual capture child too, exercising rearm and quiet
        // discharge without manufacturing event bits or changing the rules.
        for(Move m:MoveList<ALL>(p)) if(p.capture(m)) {
            StateInfo child;auto& dirty=h.stack->push(p);p.do_move(m,child,p.gives_check(m),dirty);
            h.inspect(p,start,{UCI::move(m)},p.game_of_points());p.undo_move(m);h.stack->pop();
        }
    }
    std::mt19937 rng(20261031);
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
    require(h.events && h.pickers && h.rearms && h.ep && h.promotions && h.castles && h.terminals,"coverage missing");
    std::cout<<"{\"result\":\"PASS\",\"positions\":"<<h.positions<<",\"reference_children\":"<<h.children
             <<",\"events\":"<<h.events<<",\"full_pickers\":"<<h.pickers<<",\"null_checks\":"<<h.nulls
             <<",\"exact_nnue\":"<<h.nnue<<",\"rearms\":"<<h.rearms<<",\"en_passant\":"<<h.ep
             <<",\"promotions\":"<<h.promotions<<",\"castles\":"<<h.castles<<",\"terminal_children\":"<<h.terminals
             <<",\"exported\":"<<h.exports<<"}\n";
}
