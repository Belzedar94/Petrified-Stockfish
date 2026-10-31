// SPDX-License-Identifier: GPL-3.0-or-later
// Offline frozen-transformer feature export; not an independent rules referee.
#include "attacks.h"
#include "position.h"
#include "nnue/network.h"
#include <fstream>
#include <iostream>
#include <memory>
using namespace Stockfish;
static void write32(std::ostream& out, i32 value) {
    for(int i=0;i<4;++i) out.put(char(u32(value)>>(8*i)));
}
int main(int argc,char** argv) {
    if(argc!=4) { std::cerr<<"Usage: head-export NETWORK FENS OUTPUT\n"; return 2; }
    Attacks::init(); Position::init();
    auto net=std::make_unique<Eval::NNUE::Network>(); Eval::NNUE::EvalFile file;
    net->load({},argv[1],file);
    net->verify([](auto message){std::cerr<<message<<'\n';},file,argv[1]);
    std::ifstream input(argv[2]); std::ofstream output(argv[3],std::ios::binary);
    if(!input || !output) return 2;
    std::string fen; unsigned count=0;
    while(std::getline(input,fen)) {
        Position pos; StateInfo state;
        if(pos.set(fen,false,&state) || !pos.game_of_points()) {
            std::cerr<<"Invalid scored FEN at row "<<count<<'\n'; return 1;
        }
        const auto row=net->head_input(pos);
        output.write(reinterpret_cast<const char*>(row.input.data()),row.input.size());
        for(i32 value:{row.bucket,row.psqt,row.entertainment,row.scale,row.rule50,row.difference}) write32(output,value);
        ++count;
    }
    if(!input.eof() || !output) return 1;
    std::cerr<<"Exported "<<count<<" rows, 1080 bytes each\n";
}
