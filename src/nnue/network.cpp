/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "network.h"
#include "nnue_accumulator.h"
#include "legacy_sha256.h"
#include "../position.h"
#include "../misc.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#if defined(USE_AVX2)
#include <immintrin.h>
#endif

namespace Stockfish::Eval::NNUE {
namespace {
constexpr char Run4Sha[] = "345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933";
constexpr usize LegacyBytes=52046752;
constexpr usize AdapterBytes=8+3*4+16*4+16*16*2+16*2+4+64;
constexpr usize PointsHeadBytes=8+5*4+8*(16*4+16*1024+32*4+32*32+4+32+16*32)+64;
std::array<u8,32> score_inputs(const Position& pos) {
    const Color us=pos.side_to_move();
    const int difference=std::clamp(pos.points(us)-pos.points(~us),-200,200);
    std::array<u8,32> x{};
    x[0]=2*std::clamp(difference,0,63); x[1]=2*std::clamp(-difference,0,63);
    x[2]=2*std::clamp(pos.points(us),0,63); x[3]=2*std::clamp(pos.points(~us),0,63);
    x[4]=std::clamp(pos.rule50_count(),0,100); x[5]=2*popcount(pos.walls());
    constexpr int thresholds[26]={-40,-32,-24,-22,-21,-20,-19,-16,-12,-8,-4,-2,
                                 0,2,4,8,12,16,19,20,21,22,24,32,40,64};
    for(int i=0;i<26;++i) x[6+i]=std::clamp(64+16*(difference-thresholds[i]),0,127);
    return x;
}
template<typename T> void read_le(std::istream& in, T* out, usize count) {
    for (usize i=0;i<count;++i) {
        u64 v=0;
        for(usize b=0;b<sizeof(T);++b) v |= u64(static_cast<u8>(in.get())) << (8*b);
        out[i] = static_cast<T>(v);
    }
}
u32 read_u32(std::istream& in) { u32 v; read_le(in,&v,1); return v; }
int feature(Color perspective, Square king, Square sq, Piece pc) {
    const int flip = perspective == BLACK ? 56 : 0;
    int plane = 11;
    if (pc) {
        const int order[] = {0,4,0,1,2,3,5};
        plane = type_of(pc) == KING ? 10 : 2*order[type_of(pc)] + (color_of(pc)!=perspective);
    }
    return (int(king)^flip)*768 + plane*64 + (int(sq)^flip);
}
}
int legacy_material(const Position& pos, Color c) {
    // Preserve the pinned Walls evaluator, including its custom-piece material
    // asymmetry: PSQT::init computes the white custom pawn value (214) but leaves
    // the black PieceValue entry zero. Commoners contribute 700. Trainer PSQT
    // initialization values (pawn=250) are not these runtime scaling values.
    return (c == WHITE ? 214 : 0)*pos.count<PAWN>(c)+781*pos.count<KNIGHT>(c)
           +825*pos.count<BISHOP>(c)+1276*pos.count<ROOK>(c)+2538*pos.count<QUEEN>(c)
           +700*pos.count<KING>(c);
}
bool Network::read_file(const std::filesystem::path& path, EvalFile& file) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input || (input.tellg()!=LegacyBytes && input.tellg()!=LegacyBytes+AdapterBytes
                  && input.tellg()!=LegacyBytes+PointsHeadBytes)) return false;
    const usize fileBytes=usize(input.tellg());
    input.seekg(0);
    std::vector<unsigned char> bytes(fileBytes);
    input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if (!input) return false;
    const std::string digest=sha256(bytes);
    std::copy(digest.begin(),digest.end(),networkSha.begin());
    adapter=PointsAdapter{};
    pointsHeadActive=false;
    if (fileBytes!=LegacyBytes) {
        const std::string payloadDigest(bytes.end()-64,bytes.end());
        if (sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()-64))!=payloadDigest) return false;
        std::string extension(bytes.begin()+LegacyBytes,bytes.end()-64);
        std::istringstream extra(extension,std::ios::binary);
        char magic[8]; extra.read(magic,8);
        if (fileBytes==LegacyBytes+PointsHeadBytes) {
            if (std::string(magic,8)!="PTHD0001" || read_u32(extra)!=1 || read_u32(extra)!=32
                || read_u32(extra)!=16 || read_u32(extra)!=32 || read_u32(extra)!=8) return false;
            for(auto& pointHead:pointsHeads) {
                auto& head=pointHead.board;
                read_le(extra,head.bias1,16); read_le(extra,&head.weight1[0][0],16*1024);
                read_le(extra,head.bias2,32); read_le(extra,&head.weight2[0][0],32*32);
                read_le(extra,&head.bias3,1); read_le(extra,head.weight3,32);
                read_le(extra,&pointHead.scoreWeight[0][0],16*32);
                // Safe scalar/SIMD dot products, including malicious checksummed files.
                for(auto b:head.bias1) if(b < -(1<<24) || b > (1<<24)) return false;
                for(auto b:head.bias2) if(b < -(1<<24) || b > (1<<24)) return false;
                if(head.bias3 < -(1<<20) || head.bias3 > (1<<20)) return false;
                for(auto& row:head.weight2) for(int j=16;j<32;++j) if(row[j]!=0) return false;
            }
            pointsHeadActive=true;
        } else {
        if (std::string(magic,8)!="PTADPT01" || read_u32(extra)!=1 || read_u32(extra)!=16 || read_u32(extra)!=16) return false;
        read_le(extra,adapter.biases.data(),16);
        for (auto& row:adapter.weights) read_le(extra,row.data(),16);
        read_le(extra,adapter.output.data(),16); read_le(extra,&adapter.outputBias,1);
        if (!extra || extra.peek()!=std::char_traits<char>::eof()) return false;
        for (auto b:adapter.biases) if (b < -(1<<25) || b > (1<<25)) return false;
        for (auto& row:adapter.weights) for (auto w:row) if (std::abs(int(w))>4096) return false;
        for (auto w:adapter.output) if (std::abs(int(w))>8192) return false;
        if (adapter.outputBias < -8192 || adapter.outputBias > 8192) return false;
        adapter.active=true;
        }
        if (!extra || extra.peek()!=std::char_traits<char>::eof()) return false;
        bytes.resize(LegacyBytes);
    }
    if (sha256(bytes)!=Run4Sha) return false;
    std::string storage(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    std::istringstream in(storage,std::ios::binary);
    if(read_u32(in)!=0x7af32f20 || read_u32(in)!=0xfd695ed9) return false;
    const u32 len=read_u32(in);
    if(len>4096) return false;
    std::string description(len,'\0'); in.read(description.data(),len);
    if(read_u32(in)!=0x9e5a2813) return false;
    read_le(in,biases,512);
    read_le(in,&weights[0][0],49152*512);
    read_le(in,&psqtWeights[0][0],49152*8);
    for(auto& head:heads) {
        if(read_u32(in)!=1664317130) return false;
        read_le(in,head.bias1,16); read_le(in,&head.weight1[0][0],16*1024);
        read_le(in,head.bias2,32); read_le(in,&head.weight2[0][0],32*32);
        read_le(in,&head.bias3,1); read_le(in,head.weight3,32);
    }
    if (!in || in.peek()!=std::char_traits<char>::eof()) return false;
    file.netDescription=description;
    contentHash=std::stoull(digest.substr(0,16),nullptr,16);
    initialized=true;
    return true;
}
void Network::load(const std::filesystem::path& root, std::filesystem::path path, EvalFile& file) {
    if(path.empty()) path=EvalFile::defaultName;
    initialized=false; file.current.reset(); file.source.clear(); file.netDescription.clear();
    for(const auto& dir : {std::filesystem::path{}, root})
        if(read_file(dir/path,file)) {
            // Keep the successfully opened path. NODERAWFS can open native
            // Windows paths which libc++'s POSIX absolute() cannot normalize.
            file.current=path; file.source=dir/path; return;
        }
}
bool Network::save(const EvalFile& file, const std::optional<std::filesystem::path>& path) const {
    if(!initialized || !file.current || !path) return false;
    // Only export the verified source bytes. Never silently export a different format.
    std::ifstream in(file.source,std::ios::binary|std::ios::ate);
    if(!in) return false;
    const auto size=in.tellg();
    if(size!=LegacyBytes && size!=LegacyBytes+AdapterBytes && size!=LegacyBytes+PointsHeadBytes) return false;
    in.seekg(0);
    auto bytes=std::vector<unsigned char>(usize(size));
    in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!in || sha256(bytes)!=networkSha.data()) return false;
    in.close();
    std::error_code ec;
    if(std::filesystem::equivalent(file.source,*path,ec) && !ec) return true;
    std::ofstream out(*path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    return bool(out);
}
void Network::verify(const std::function<void(std::string_view)>& f, const EvalFile& file,
                     std::filesystem::path path) const {
    if(path.empty()) path=EvalFile::defaultName;
    if(!initialized || file.current!=path) {
        if(f) f("ERROR: valid Legacy Walls run4test is required; search aborted. EvalFile="+path.string());
        std::exit(EXIT_FAILURE);
    }
    if(f) {
        if (pointsHeadActive) f("NNUE PointsHead v1 loaded; SHA256="+std::string(networkSha.data())+"; base="+Run4Sha+"; 1056/16/32/1, 8 buckets");
        else if (adapter.active) f("NNUE PointsAdapter v1 loaded; SHA256="+std::string(networkSha.data())+"; base="+Run4Sha+"; 16/16/1");
        else f("NNUE Legacy Walls run4test loaded; SHA256="+std::string(Run4Sha)+"; 49152/512x2/16/32/1, 8 buckets");
    }
}
void Network::update(const Position& pos, Color side, AccumulatorCaches& cache, bool reference) const {
    const Square king=pos.square<KING>(side);
    auto& e=cache.entries[side][king];
    if(reference || !e.valid) {
        std::copy(std::begin(biases),std::end(biases),e.sums.begin());
        e.psqt.fill(0); e.board.fill(NO_PIECE); e.walls=0; e.valid=true;
    }
    auto add=[&](Square s,Piece pc,int sign) {
        const int idx=feature(side,king,s,pc);
#if defined(USE_AVX2)
        if (!reference) {
            for(int j=0;j<512;j+=16) {
                auto old=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(&e.sums[j]));
                auto w=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(&weights[idx][j]));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&e.sums[j]),sign>0?_mm256_add_epi16(old,w):_mm256_sub_epi16(old,w));
            }
        } else
#endif
        for(int j=0;j<512;++j) e.sums[j]=i16(e.sums[j]+sign*weights[idx][j]);
        for(int j=0;j<8;++j) e.psqt[j]+=sign*psqtWeights[idx][j];
    };
    for(Square s=SQ_A1;s<=SQ_H8;++s) {
        const Piece now=pos.piece_on(s), old=e.board[s];
        if(now!=old) { if(old) add(s,old,-1); if(now) add(s,now,1); e.board[s]=now; }
    }
    for(Bitboard b=e.walls^pos.walls();b;) {
        Square s=pop_lsb(b); add(s,NO_PIECE,(pos.walls()&s)?1:-1);
    }
    e.walls=pos.walls();
}
Value Network::forward(const Position& pos, const AccumulatorCaches& cache, bool adjusted, [[maybe_unused]] bool scalar) const {
    const Color us=pos.side_to_move();
    const auto& a=cache.entries[us][pos.square<KING>(us)];
    const auto& b=cache.entries[~us][pos.square<KING>(~us)];
    const int bucket=std::min((pos.count<ALL_PIECES>()-1)/4,7);
    const bool scored=pointsHeadActive && pos.game_of_points();
    const auto& head=scored ? pointsHeads[bucket].board : heads[bucket];
    const auto scores=scored ? score_inputs(pos) : std::array<u8,32>{};
    u8 input[1024], hidden1[32]={},hidden2[32];
    for(int j=0;j<512;++j) { input[j]=std::clamp<int>(a.sums[j],0,127); input[j+512]=std::clamp<int>(b.sums[j],0,127); }
    for(int i=0;i<16;++i) {
        i32 sum=head.bias1[i];
        if(scored) for(int j=0;j<32;++j) sum+=scores[j]*pointsHeads[bucket].scoreWeight[i][j];
#if defined(USE_AVX2)
        if(!scalar) {
            auto total=_mm256_setzero_si256();
            for(int j=0;j<1024;j+=32) {
                auto x=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[j]));
                auto w=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(&head.weight1[i][j]));
                total=_mm256_add_epi32(total,_mm256_madd_epi16(_mm256_maddubs_epi16(x,w),_mm256_set1_epi16(1)));
            }
            i32 lanes[8]; _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes),total);
            for(int j=0;j<8;++j) sum+=lanes[j];
        } else
#endif
        for(int j=0;j<1024;++j) sum+=input[j]*head.weight1[i][j];
        hidden1[i]=std::clamp(sum>>6,0,127);
    }
    for(int i=0;i<32;++i) {
        i32 sum=head.bias2[i]; for(int j=0;j<16;++j) sum+=hidden1[j]*head.weight2[i][j];
        hidden2[i]=std::clamp(sum>>6,0,127);
    }
    i32 output=head.bias3; for(int j=0;j<32;++j) output+=hidden2[j]*head.weight3[j];
    const int psqt=(a.psqt[bucket]-b.psqt[bucket])/2;
    const int entertainment=adjusted && std::abs(legacy_material(pos,WHITE)-legacy_material(pos,BLACK))<=44 ? 7 : 0;
    return ((128-entertainment)*psqt+(128+entertainment)*output)/128/16;
}
Network::HeadInput Network::head_input(const Position& pos) const {
    auto cache=std::make_unique<AccumulatorCaches>(*this);
    update(pos,WHITE,*cache,true); update(pos,BLACK,*cache,true);
    const Color us=pos.side_to_move();
    const auto& a=cache->entries[us][pos.square<KING>(us)];
    const auto& b=cache->entries[~us][pos.square<KING>(~us)];
    HeadInput row{};
    for(int i=0;i<512;++i) {
        row.input[i]=std::clamp<int>(a.sums[i],0,127);
        row.input[i+512]=std::clamp<int>(b.sums[i],0,127);
    }
    const auto scores=score_inputs(pos);
    std::copy(scores.begin(),scores.end(),row.input.begin()+1024);
    row.bucket=std::min((pos.count<ALL_PIECES>()-1)/4,7);
    row.psqt=(a.psqt[row.bucket]-b.psqt[row.bucket])/2;
    row.entertainment=std::abs(legacy_material(pos,WHITE)-legacy_material(pos,BLACK))<=44 ? 7 : 0;
    row.scale=903+32*(legacy_material(pos,WHITE)+legacy_material(pos,BLACK))/1024;
    row.rule50=std::min(pos.rule50_count(),100);
    row.difference=std::clamp(pos.points(us)-pos.points(~us),-200,200);
    return row;
}
Value Network::evaluate(const Position& pos, AccumulatorStack&, AccumulatorCaches& cache) const {
    update(pos,WHITE,cache); update(pos,BLACK,cache);
    return forward(pos,cache,true);
}
Value Network::evaluate_reference(const Position& pos, bool adjusted) const {
    auto cache=std::make_unique<AccumulatorCaches>(*this);
    update(pos,WHITE,*cache,true); update(pos,BLACK,*cache,true);
    return forward(pos,*cache,adjusted,true);
}
} // namespace Stockfish::Eval::NNUE
