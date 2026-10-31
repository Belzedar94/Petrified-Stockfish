// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"
#include "movegen.h"
#include "movegen_reference.h"
#include "position.h"
#include "uci_move.h"
#include "search_threat.h"
#include "movepick.h"
#include "history.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "api/petrified_outcome.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
using namespace Stockfish;

static void require(bool ok, const std::string& message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

// Slow reference: use real legal move generation and make/undo; measure the
// complete board and scoreboard before/after, rather than duplicate the fast
// bitboard exchange algorithm. This is a restricted material/points game:
// declining is always allowed, and terminal bonuses are deliberately absent.
static int balance(const Position& pos, Color us) {
    int value = 100 * (pos.points(us) - pos.points(~us));
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
        if (const Piece pc = pos.piece_on(s))
            value += (color_of(pc) == us ? 1 : -1) * PieceValue[pc];
    return value;
}

static int reference_reply(Position& pos, Square target) {
    if (!pos.piece_on(target) || type_of(pos.piece_on(target)) == KING)
        return 0;
    const Color us = pos.side_to_move();
    const int before = balance(pos, us);
    int best = 0;
    Move legal[MAX_MOVES];
    Move* end = reference_generate<ALL>(pos, legal);
    for (Move* it = legal; it != end; ++it) {
        const Move move = *it;
        const Square captured = move.type_of() == EN_PASSANT
                              ? move.to_sq() - pawn_push(us) : move.to_sq();
        if (!pos.capture(move) || captured != target) continue;
        StateInfo state;
        pos.do_move(move, state);
        const int gain = balance(pos, us) - before - reference_reply(pos, move.to_sq());
        pos.undo_move(move);
        best = std::max(best, gain);
    }
    return best;
}

static int reference_see(Position& pos, Move move) {
    if (move.type_of() == CASTLING || type_of(pos.moved_piece(move)) == KING) return 0;
    const Color us = pos.side_to_move();
    const int before = balance(pos, us);
    StateInfo state;
    pos.do_move(move, state);
    const int gain = balance(pos, us) - before - reference_reply(pos, move.to_sq());
    pos.undo_move(move);
    return gain;
}


static std::size_t positions=0, threats=0, exchanges=0, children=0, nnueChecks=0, pickers=0;
static std::size_t epPositions=0, terminalExports=0;
static std::unique_ptr<Eval::NNUE::Network> network;
static std::unique_ptr<Eval::NNUE::AccumulatorCaches> cache;
static std::unique_ptr<Eval::NNUE::AccumulatorStack> stack;
static std::unique_ptr<ButterflyHistory> mainHist;
static std::unique_ptr<LowPlyHistory> lowHist;
static std::unique_ptr<CapturePieceToHistory> captureHist;
static std::unique_ptr<PieceToHistory> continuation;
static SharedHistories* shared = nullptr;
static bool weighted=false;
static std::ofstream cases;

static void exact_eval(const Position& pos) {
    if (!weighted) return;
    require(network->evaluate(pos,*stack,*cache)==network->evaluate_reference(pos),
            "threat probe changed NNUE/cache");
    ++nnueChecks;
}

static void inspect(Position& pos, bool emit=false, int expected=-1) {
    const std::string fen=pos.fen();
    const StateInfo* original=pos.state();
    unsigned char snapshot[sizeof(StateInfo)]; std::memcpy(snapshot,original,sizeof(StateInfo));
    exact_eval(pos);
    const bool actual=Search::quiet_capture_threat(pos);
    require(pos.state()==original && std::memcmp(snapshot,original,sizeof(StateInfo))==0,
            "probe changed state bytes/history pointer: "+fen);
    require(pos.fen()==fen && pos.pos_is_ok(),"probe changed FEN/board");
    exact_eval(pos);
    Move legal[MAX_MOVES]; const auto end=reference_generate<ALL>(pos,legal);
    bool captureStage=false;
    for (auto it=legal;it!=end;++it) captureStage|=pos.capture_stage(*it);
    const bool eligible=pos.game_of_points() && !pos.checkers() && end-legal>1 && !captureStage;
    bool reference=false;
    std::vector<std::pair<std::string,int>> captures;
    std::string passFen;
    if (pos.game_of_points() && !pos.checkers()) {
        // Independent reconstruction, not do_null_move or the optimized SEE.
        std::istringstream in(fen); std::vector<std::string> fields; std::string f;
        while(in>>f) fields.push_back(f);
        fields[1]=fields[1]=="w"?"b":"w"; fields[3]="-";
        for (const auto& field:fields) passFen+=(passFen.empty()?"":" ")+field;
        Position pass; StateInfo state;
        require(!pass.set(passFen,false,&state),"opposite-side reference FEN");
        Move enemy[MAX_MOVES]; auto enemyEnd=reference_generate<ALL>(pass,enemy);
        for (auto it=enemy;it!=enemyEnd;++it) if(pass.capture(*it)) {
            const int gain=reference_see(pass,*it); ++exchanges;
            require(pass.points_see(*it)==gain,"reference capture economics mismatch");
            captures.emplace_back(UCI::move(*it),gain);
            reference|=gain>0;
        }
    }
    reference&=eligible;
    require(actual==reference,"threat predicate differs from full make/undo reference: "+fen);
    if(expected>=0) require(actual==bool(expected),"hand threat expectation: "+fen);
    require(pos.state()==original && std::memcmp(snapshot,original,sizeof(StateInfo))==0,
            "reference changed state");
    // The inherited full picker may legitimately populate the lazy threats
    // cache. Check the probe on both cold and warm caches, then compare picker
    // state against the warmed snapshot rather than reject cache population.
    for(PieceType pt=PAWN;pt<=KING;++pt) (void)pos.threats_by(pt);
    std::memcpy(snapshot,original,sizeof(StateInfo));
    require(Search::quiet_capture_threat(pos)==actual && pos.state()==original
            && std::memcmp(snapshot,original,sizeof(StateInfo))==0,"warm-cache probe changed state");
    if(actual) {
        const PieceToHistory* cont[6]={continuation.get(),continuation.get(),continuation.get(),
                                      continuation.get(),continuation.get(),continuation.get()};
        for (Move ttMove : {Move::none(),legal[0]}) {
            MovePicker mp(pos,ttMove,1,mainHist.get(),lowHist.get(),captureHist.get(),cont,shared,2);
            std::vector<unsigned> got,want;
            for(Move m=mp.next_move();m!=Move::none();m=mp.next_move()) got.push_back(m.raw());
            for(auto it=legal;it!=end;++it) want.push_back(it->raw());
            std::sort(got.begin(),got.end()); std::sort(want.begin(),want.end());
            require(got==want,"full threat picker lost or duplicated legal replies"); ++pickers;
        }
    }
    require(pos.state()==original && std::memcmp(snapshot,original,sizeof(StateInfo))==0,
            "reference/picker changed state");
    epPositions+=bool(pos.ep_squares()); ++positions; threats+=actual;
    if(emit && cases && pos.game_of_points()) {
        if(pos.ep_squares()) return; // EP is covered in C++; fixture converter cannot encode it.
        if(Petrified::outcome(pos,true).terminal()) { ++terminalExports; return; }
        cases<<"{\"fen\":\""<<fen<<"\",\"pass_fen\":\""<<passFen<<"\",\"threat\":"<<(actual?"true":"false")
             <<",\"in_check\":"<<(pos.checkers()?"true":"false")<<",\"legal\":[";
        for(auto it=legal;it!=end;++it) cases<<(it==legal?"":",")<<'"'<<UCI::move(*it)<<'"';
        cases<<"],\"capture_stage\":"<<(captureStage?"true":"false")<<",\"captures\":[";
        for(std::size_t i=0;i<captures.size();++i) cases<<(i?",":"")<<"{\"move\":\""<<captures[i].first<<"\",\"value\":"<<captures[i].second<<'}';
        cases<<"]}\n";
    }
}

int main(int argc,char** argv) {
    Attacks::init(); Position::init();
    if(argc>1) { cases.open(argv[1]); require(bool(cases),"cases output"); }
    network=std::make_unique<Eval::NNUE::Network>();
    if(argc>2) { Eval::NNUE::EvalFile file; network->load({},argv[2],file);
        network->verify([](auto s){std::cout<<s<<'\n';},file,argv[2]); weighted=true; }
    cache=std::make_unique<Eval::NNUE::AccumulatorCaches>(*network);
    stack=std::make_unique<Eval::NNUE::AccumulatorStack>();
    mainHist=std::make_unique<ButterflyHistory>(); mainHist->fill(0);
    lowHist=std::make_unique<LowPlyHistory>(); lowHist->fill(0);
    captureHist=std::make_unique<CapturePieceToHistory>(); captureHist->fill(0);
    continuation=std::make_unique<PieceToHistory>(); continuation->fill(0);
    // Local lifetime: release large pages before memory.cpp's registry exits.
    auto sharedOwner=std::make_unique<SharedHistories>(1); shared=sharedOwner.get();
    shared->pawnHistory.clear_range(0,0,1);
    require(Search::threat_qsearch_depth(true,false)<Search::threat_qsearch_depth(true,true)
            && Search::threat_qsearch_depth(true,false)>DEPTH_UNSEARCHED,"TT search-domain separation");
    require(Search::threat_qsearch_depth(false,false)==DEPTH_QS
            && Search::threat_qsearch_depth(false,true)==DEPTH_QS,"legacy TT depth");
    struct Fixture { const char* fen; int expected; };
    const std::vector<Fixture> fixtures={
        {"8/7k/7p/5n2/3Q4/P7/8/K7 w - - 0 1",1},
        {"7k/p7/5b2/8/8/8/1R5P/K7 w - - 0 1",1},
        {"7k/p7/5b2/8/3*4/8/1R5P/K7 w - - 0 1",0},
        {"7k/8/8/5q2/8/8/2P5/K7 w - - 0 1",0},
        {"8/7k/8/3p1n2/3Q4/8/8/K7 w - - 0 1",0},
        {"***r***k/*******1/********/********/********/********/1*******/K**R**** w - - 0 1",0},
        {"2k5/8/8/2p5/3Q4/8/8/K1R5 w - - 0 1",0},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",0},
        {"7k/8/8/3pP3/8/8/8/K7 w - d6 0 1",0},
        {"8/P6k/8/5n2/3Q4/8/8/K7 w - - 0 1",0},
        {"7k/8/8/8/8/8/8/r2K4 w - - 0 1",0}
    };
    for(const auto& f:fixtures) for(const char* scores:{" points=0,0"," points=7,4"," points=19,0"," points=0,21",""}) {
        Position pos; StateInfo state;
        if(auto error=pos.set(std::string(f.fen)+scores,false,&state))
            require(false,std::string("fixture FEN: ")+f.fen+scores+": "+error->what());
        for(int flip=0;flip<2;++flip) {
            if(flip) require(!pos.flip(),"color flip");
            inspect(pos,true,pos.game_of_points()?f.expected:0);
            for(Move m:MoveList<ALL>(pos)) {
                StateInfo child; const auto before=pos.fen(); pos.do_move(m,child);
                inspect(pos); ++children; pos.undo_move(m); require(pos.fen()==before,"child undo");
            }
        }
    }
    std::mt19937 rng(20261031);
    for(int game=0;game<40;++game) {
        Position pos; std::deque<StateInfo> states(1); std::vector<Move> history;
        const std::string initial="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 points=7,4";
        require(!pos.set(initial,false,&states.back()),"walk FEN");
        for(int ply=0;ply<160;++ply) {
            inspect(pos,game<16 && ply%11==0);
            if(Petrified::outcome(pos,true).terminal()) break;
            const auto legal=MoveList<ALL>(pos); if(!legal.size()) break;
            if(ply%16==0) for(Move m:legal) {
                StateInfo child; pos.do_move(m,child); inspect(pos); ++children; pos.undo_move(m);
            }
            Move m=legal.begin()[rng()%legal.size()]; history.push_back(m); states.emplace_back();
            pos.do_move(m,states.back());
        }
        while(!history.empty()) { pos.undo_move(history.back()); history.pop_back(); states.pop_back(); inspect(pos); }
        require(pos.fen()==initial,"walk full restoration");
    }
    require(threats && exchanges && children && epPositions && pickers,"missing threat/state coverage");
    std::cout<<"{\"positions\":"<<positions<<",\"threats\":"<<threats<<",\"reference_captures\":"<<exchanges
             <<",\"children\":"<<children<<",\"full_pickers\":"<<pickers<<",\"nnue_exact\":"<<nnueChecks
             <<",\"ep_positions\":"<<epPositions<<",\"terminal_exports_omitted\":"<<terminalExports<<"}\n";
}
