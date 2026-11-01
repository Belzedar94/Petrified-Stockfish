// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "movegen.h"
#include "uci.h"
#include "misc.h"
#include "api/petrified_outcome.h"
#include <sstream>
#include <stdexcept>

namespace Stockfish {
namespace {
std::string measures(const Position& p, const Eval::NNUE::Network& n) {
    const int raw=n.evaluate_reference(p), rawPlain=n.evaluate_reference(p,false);
    const int material=Eval::NNUE::legacy_material(p,WHITE)+Eval::NNUE::legacy_material(p,BLACK);
    const int scale=903+32*material/1024;
    const int symMaterial=material+107*(p.count<PAWN>(BLACK)-p.count<PAWN>(WHITE));
    const int symScale=903+32*symMaterial/1024;
    const int clock=std::min(p.rule50_count(),100), lead=p.points(p.side_to_move())-p.points(~p.side_to_move());
    const int skip=100*std::clamp(lead,-200,200);
    const int board=(raw*scale/1024)*(100-clock)/100;
    const int base=std::clamp(board+skip+n.points_adjustment(p,board),VALUE_TB_LOSS_IN_MAX_PLY+1,VALUE_TB_WIN_IN_MAX_PLY-1);
    std::ostringstream o;
    o<<"{\"raw\":"<<raw<<",\"raw_unadjusted\":"<<rawPlain<<",\"scale\":"<<scale
     <<",\"symmetric_scale\":"<<symScale<<",\"clock\":"<<clock<<",\"lead\":"<<lead
     <<",\"board\":"<<board<<",\"skip\":"<<skip<<",\"value\":"<<base
     <<",\"symmetric_value\":"<<(raw*symScale/1024)*(100-clock)/100+skip
     <<",\"unattenuated_value\":"<<raw*scale/1024+skip
     <<",\"in_check\":"<<(p.checkers()?"true":"false")<<"}";
    return o.str();
}
}
void Engine::diagnose() {
    wait_for_search_finished();verify_network();
    const std::string before=pos.fen();const Key key=pos.key();
    Position flipped;StateInfo flipState{};
    if (flipped.set(before,false,&flipState) || flipped.flip()) throw std::runtime_error("flip failed");
    const auto end=Petrified::outcome(pos,true,0);
    const auto root=measures(pos,*network);
    std::ostringstream out;
    out<<"{\"fen\":\""<<before<<"\",\"root\":"<<root<<",\"mirror_fen\":\""<<flipped.fen()
       <<"\",\"mirror\":"<<measures(flipped,*network)<<",\"terminal\":"<<(end.terminal()?"true":"false")
       <<",\"result\":\""<<end.result()<<"\",\"claim\":"<<Petrified::resignation_value(pos)
       <<",\"children\":[";
    bool first=true;
    for (Move m:MoveList<ALL>(pos)) {
        if (!first) out<<',';
        first=false;
        const bool capture=pos.capture_stage(m); const std::string uci=UCIEngine::move(m,false);
        StateInfo childState{};pos.do_move(m,childState);
        const auto child=Petrified::outcome(pos,true,0);
        out<<"{\"move\":\""<<uci<<"\",\"capture_stage\":"<<(capture?"true":"false")
           <<",\"fen\":\""<<pos.fen()<<"\",\"terminal\":"<<(child.terminal()?"true":"false")
           <<",\"result\":\""<<child.result()<<"\",\"reason\":\""<<child.reason()
           <<"\",\"terminal_value\":"<<child.value<<",\"claim\":"<<Petrified::resignation_value(pos)
           <<",\"evaluation\":"<<measures(pos,*network)<<"}";
        pos.undo_move(m);
        if (pos.fen()!=before || pos.key()!=key) throw std::runtime_error("diagnostic undo mismatch");
    }
    if (root!=measures(pos,*network)) throw std::runtime_error("evaluation changed after undo");
    out<<"]}";sync_cout<<"diagnostic "<<out.str()<<sync_endl;
}
}
