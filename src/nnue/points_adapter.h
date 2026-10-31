// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental score-conditioned head; version 1 has 16 inputs and 16 units.
#ifndef PETRIFIED_POINTS_ADAPTER_H
#define PETRIFIED_POINTS_ADAPTER_H
#include <algorithm>
#include <array>
#include "../position.h"
namespace Stockfish::Eval::NNUE {
inline std::array<i16,16> points_features(const Position& pos, Value boardValue) {
    std::array<i16,16> x{};
    const Color us=pos.side_to_move();
    x[0]=i16(std::clamp(int(boardValue),-8192,8192)/4);
    x[1]=i16(std::clamp(pos.points(us)-pos.points(~us),-40,40)*256/20);
    const int scale[]={0,32,64,64,64,128};
    for (int pt=PAWN;pt<=QUEEN;++pt)
        for (int side=0;side<2;++side)
            x[2+2*(pt-1)+side]=i16(std::min(popcount(pos.pieces(side ? ~us:us,PieceType(pt)))*scale[pt],2048));
    x[12]=i16(popcount(pos.walls())*16);
    x[13]=i16(std::min(pos.rule50_count(),100)*256/100);
    for (int side=0;side<2;++side) {
        const Color c=side ? ~us:us;
        const bool insufficient=pos.count<ALL_PIECES>(c)==1
          || (pos.count<ALL_PIECES>(c)==2 && !pos.walls()
              && (pos.count<KNIGHT>(c)+pos.count<BISHOP>(c)==1));
        x[14+side]=insufficient ? 0:256;
    }
    return x;
}
struct PointsAdapter {
    bool active=false;
    std::array<i32,16> biases{};
    std::array<std::array<i16,16>,16> weights{};
    std::array<i16,16> output{};
    i32 outputBias=0;
    Value evaluate(const Position& pos, Value boardValue) const {
        if (!active || !pos.game_of_points()) return VALUE_ZERO;
        const auto x=points_features(pos,boardValue);
        i32 sum=0;
        for (int i=0;i<16;++i) {
            i32 hidden=biases[i];
            for (int j=0;j<16;++j) hidden+=weights[i][j]*x[j];
            sum+=output[i]*std::clamp(hidden/256,0,1024);
        }
        return std::clamp(sum/256+outputBias,-4000,4000);
    }
};
}
#endif
