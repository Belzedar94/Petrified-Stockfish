// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic-only runtime switches; absent from the frozen champion binary.
#pragma once
namespace Stockfish::Diagnostic {
inline bool reduced = false;
inline int scale = 0; // 0 champion; 1 symmetric outer scale; 2 no clock attenuation
}
