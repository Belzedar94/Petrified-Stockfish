// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NNUE_SPARSE_HEAD_H_INCLUDED
#define NNUE_SPARSE_HEAD_H_INCLUDED

#include <cstring>
#include "../types.h"
#if defined(USE_AVX2)
#include <immintrin.h>
#endif

namespace Stockfish::Eval::NNUE {

// Runtime layout only. The network file and output-major reference weights
// remain unchanged. Each four-input block contains all sixteen outputs.
struct SparseHeadWeights {
    alignas(64) i8 blocks[256][16][4];

    void pack(const i8 (&source)[16][1024]) {
        for (int block = 0; block < 256; ++block)
            for (int output = 0; output < 16; ++output)
                for (int lane = 0; lane < 4; ++lane)
                    blocks[block][output][lane] = source[output][4 * block + lane];
    }
};

// Input bytes are clipped to [0,127], weights are signed bytes. A pairwise
// product sum is within [-32512,32258], so maddubs cannot saturate. Four-input
// sums and the complete dot products (including bounded biases) fit in int32.
inline void sparse_head_affine(const u8 (&input)[1024], const SparseHeadWeights& weights,
                               i32 (&output)[16]) {
#if defined(USE_AVX2)
    auto low = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(output));
    auto high = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(output + 8));
    const auto ones = _mm256_set1_epi16(1);
    for (int block = 0; block < 256; ++block) {
        i32 bytes;
        std::memcpy(&bytes, input + 4 * block, sizeof(bytes));
        if (!bytes) continue;
        const auto x = _mm256_set1_epi32(bytes);
        const auto w0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights.blocks[block][0]));
        const auto w1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights.blocks[block][8]));
        low = _mm256_add_epi32(low, _mm256_madd_epi16(_mm256_maddubs_epi16(x, w0), ones));
        high = _mm256_add_epi32(high, _mm256_madd_epi16(_mm256_maddubs_epi16(x, w1), ones));
    }
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(output), low);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(output + 8), high);
#else
    for (int block = 0; block < 256; ++block)
        for (int outputIndex = 0; outputIndex < 16; ++outputIndex)
            for (int lane = 0; lane < 4; ++lane)
                output[outputIndex] += input[4 * block + lane] * weights.blocks[block][outputIndex][lane];
#endif
}
}  // namespace Stockfish::Eval::NNUE
#endif
