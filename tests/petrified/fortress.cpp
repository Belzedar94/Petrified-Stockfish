// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "petrified_fortress.h"
#include "position.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "uci_move.h"
#include "api/petrified_outcome.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
using namespace Stockfish;
using Attacks::attacks_bb;
static unsigned checked=0, accepted=0, transitions=0, nnue=0, emitted=0;
static std::ofstream output;
static std::unique_ptr<Eval::NNUE::Network> network;
static std::unique_ptr<Eval::NNUE::AccumulatorCaches> cache;
static std::unique_ptr<Eval::NNUE::AccumulatorStack> stack;
static void require(bool ok, const std::string& why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
using Board = std::array<char,64>;
static Board board() {
    Board b; b.fill('.');
    for (auto [s,p] : std::initializer_list<std::pair<Square,char>>{
        {SQ_H1,'K'},{SQ_E2,'N'},{SQ_C4,'P'},{SQ_A7,'k'},{SQ_A6,'q'},
        {SQ_G7,'r'},{SQ_F6,'r'},{SQ_D6,'b'},{SQ_E6,'b'},{SQ_A3,'n'},{SQ_F5,'p'}}) b[s]=p;
    for (Square s : {SQ_B3,SQ_C3,SQ_D3,SQ_E3,SQ_F3,SQ_G3}) b[s]='P';
    for (Square s : {SQ_A4,SQ_B4,SQ_D4,SQ_E4,SQ_G4,SQ_H4}) b[s]='*';
    return b;
}
static std::string fen(Board b, bool black, int transform=0, int lead=10, int half=0) {
    Board mapped; mapped.fill('.');
    for (int s=0;s<64;++s) {
        int t=s ^ ((transform&1) ? 7 : 0) ^ ((transform&2) ? 56 : 0);
        char p=b[s];
        if ((transform&2) && std::isalpha(p)) p=std::islower(p)?std::toupper(p):std::tolower(p);
        mapped[t]=p;
    }
    std::ostringstream f;
    for (int r=7;r>=0;--r) {
        if(r!=7) f << '/'; int empty=0;
        for(int c=0;c<8;++c) {
            char p=mapped[r*8+c];
            if(p=='.') ++empty;
            else { if(empty) f << empty; empty=0; f << p; }
        }
        if(empty) f << empty;
    }
    f << ((black ^ bool(transform&2)) ? " b" : " w") << " - - " << half << " 118 points=";
    if(transform&2) f << "1," << lead; else f << lead << ",1";
    return f.str();
}
static void state_check(Position& pos) {
    Position rebuilt; StateInfo s;
    require(!rebuilt.set(pos.fen(),false,&s),"FEN rebuild");
    require(pos.key()==rebuilt.key() && pos.material_key()==rebuilt.material_key()
            && pos.pawn_key()==rebuilt.pawn_key() && pos.checkers()==rebuilt.checkers()
            && pos.pos_is_ok(),"state/key rebuild "+pos.fen());
    Move moves[MAX_MOVES]; const Move* end=reference_generate<ALL>(pos,moves);
    auto legal=MoveList<ALL>(pos);
    require(legal.size()==std::size_t(end-moves) && std::equal(legal.begin(),legal.end(),moves),"full legal reference");
    if(network) {
        require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),"NNUE reference"); ++nnue;
    }
}
static void ensure(Position& pos, Color defender) {
    auto end=Petrified::outcome(pos,true);
    if(end.terminal()) require(end.winner==defender,"losing settlement "+pos.fen());
    else {
        auto next=Petrified::fortress(pos);
        require(next && next->defender==defender,"certificate not closed under policy: "+pos.fen());
    }
}
static void check(const std::string& f, int expected=-1, bool emit=false) {
    Position pos; StateInfo s;
    require(!pos.set(f,false,&s),"fixture set "+f);
    auto original=pos.fen(); auto key=pos.key();
    auto proof=Petrified::fortress(pos); ++checked;
    require(pos.fen()==original && pos.key()==key,"recognizer mutated state");
    if(expected>=0) require(bool(proof)==bool(expected),"unexpected recognition "+f);
    if(emit) state_check(pos);
    if(emit && output) {
        ++emitted;
        output << "{\"fen\":\"" << pos.fen() << "\",\"accepted\":" << (proof?"true":"false")
               << ",\"defender\":" << (proof?int(proof->defender):-1)
               << ",\"margin\":" << (proof?proof->pointMargin:0)
               << ",\"defence\":\"" << (proof?UCI::move(proof->defence):"0000") << "\",\"legal\":[";
        bool first=true; for(Move m:MoveList<ALL>(pos)) { if(!first)output<<','; first=false;output<<'"'<<UCI::move(m)<<'"'; }
        output << "],\"lines\":[";
    }
    bool first=true;
    if(proof) {
        ++accepted;
        if(!Petrified::outcome(pos,true).terminal())
            for(Move m:MoveList<ALL>(pos)) {
                if(pos.side_to_move()==proof->defender && m!=proof->defence) continue;
                StateInfo child,reply; pos.do_move(m,child); ++transitions;
                ensure(pos,proof->defender);
                Move response=Move::none();
                if(!Petrified::outcome(pos,true).terminal() && pos.side_to_move()==proof->defender) {
                    response=Petrified::fortress(pos)->defence;
                    require(contains_legal_move(pos,response),"illegal policy response");
                    pos.do_move(response,reply); ++transitions; ensure(pos,proof->defender);
                }
                if(emit) state_check(pos);
                if(emit && output) {
                    const auto end=Petrified::outcome(pos,true);
                    if(!first)output<<',';first=false;
                    output << "{\"moves\":[\"" << UCI::move(m) << '"';
                    if(response!=Move::none())output<<",\""<<UCI::move(response)<<'"';
                    output << "],\"after\":\"" << pos.fen() << "\",\"result\":\""
                           << end.result() << "\",\"points\":["
                           << (end.terminal()?end.points[WHITE]:pos.points(WHITE)) << ','
                           << (end.terminal()?end.points[BLACK]:pos.points(BLACK)) << "]}";
                }
                if(response!=Move::none())pos.undo_move(response);
                pos.undo_move(m);
                require(pos.fen()==original && pos.key()==key,"policy undo");
                if(emit)state_check(pos);
            }
    }
    if(emit && output) output << "]}\n";
}
int main(int argc,char**argv) {
    Attacks::init(); Position::init();
    if(argc>1){output.open(argv[1]);require(bool(output),"case output");}
    if(argc>2){
        network=std::make_unique<Eval::NNUE::Network>(); Eval::NNUE::EvalFile file;
        network->load({},argv[2],file);network->verify([](auto s){std::cout<<s<<'\n';},file,argv[2]);
        cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*network);
        stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    }
    // Every king/knight placement in an otherwise empty 2x2 refuge.
    for(Square k:{SQ_G1,SQ_H1,SQ_G2,SQ_H2}) for(int n=0;n<64;++n) if(n!=k) {
        auto box=square_bb(SQ_G1)|SQ_H1|SQ_G2|SQ_H2;
        require(bool(box & ~square_bb(k) & ~square_bb(Square(n)) & ~attacks_bb<KNIGHT>(Square(n))),"one knight seals refuge");
    }
    for(int transform=0;transform<4;++transform) {
        for(bool black:{false,true}) check(fen(board(),black,transform),1,true);
        for(int lead:{0,1,9})check(fen(board(),false,transform,lead),0,true);
        for(int mask=0;mask<128;++mask) {
            Board b=board(); int i=0;
            for(Square s:{SQ_B3,SQ_C3,SQ_D3,SQ_E3,SQ_F3,SQ_G3,SQ_C4}) if(mask&(1<<i++)) b[s]='*';
            for(bool black:{false,true})check(fen(b,black,transform),1,mask%16==0);
        }
        for(int n=0;n<64;++n) for(Square k:{SQ_G1,SQ_H1,SQ_G2,SQ_H2}) {
            Board b=board();b[SQ_A3]='.';b[SQ_H1]='.';
            if(b[n]!='.' || n==k)continue;
            b[n]='n';b[k]='K';
            for(bool black:{false,true})check(fen(b,black,transform),-1,n%13==0);
        }
        // Near misses: an actual breach, a second invader, an unsupported own
        // piece, double pushes, an unsafe capture, promotion and insufficient lead.
        for(int kind=0;kind<9;++kind) {
            Board b=board();
            if(kind==0)b[SQ_A4]='.';
            if(kind==1)b[SQ_B1]='n';
            if(kind==2)b[SQ_H5]='p';
            if(kind==3){b[SQ_E2]='.';b[SQ_E5]='N';}
            if(kind==4)b[SQ_E2]='B';
            if(kind==5){b[SQ_F5]='.';b[SQ_F7]='p';}
            if(kind==6){b[SQ_F5]='.';b[SQ_F2]='p';}
            if(kind==7){b[SQ_F5]='.';b[SQ_B5]='p';}
            if(kind==8){b[SQ_F5]='.';b[SQ_A2]='p';b[SQ_A3]='.';}
            check(fen(b,kind==7,transform),0,true);
        }
        // Pawn pre-emption closes the gate before it captures anything.
        for(Square p:{SQ_F4,SQ_B5,SQ_D5}) {
            Board b=board();b[SQ_F5]='.';b[p]='p';
            check(fen(b,false,transform),1,true);check(fen(b,true,transform),0,true);
        }
        check(fen(board(),false,transform,10,99),1,true);
    }
    // Unsupported normal play must not change the state or invent a win.
    std::mt19937 rng(20261030);
    for(int game=0;game<12;++game) {
        Position pos; std::deque<StateInfo> states(1);
        require(!pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=0,0",false,&states.back()),"start");
        for(int ply=0;ply<160 && !Petrified::outcome(pos,true).terminal();++ply){
            check(pos.fen(),-1,ply%80==0);
            auto legal=MoveList<ALL>(pos);Move m=legal.begin()[rng()%legal.size()];
            states.emplace_back();pos.do_move(m,states.back());
        }
    }
    require(accepted>1000 && transitions>10000,"coverage");
    std::cout << "{\"result\":\"PASS\",\"positions\":"<<checked<<",\"certificates\":"<<accepted
              <<",\"policy_transitions\":"<<transitions<<",\"nnue_exact\":"<<nnue<<",\"exported\":"<<emitted<<"}\n";
}
