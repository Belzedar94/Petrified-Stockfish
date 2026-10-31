"""Add read-only diagnostics to an isolated checkout of the pinned Walls oracle.

Does not change move generation, evaluation, search or adjudication. The original
unmodified binary remains the strength opponent. Never run in a shared checkout.
"""
import argparse
import difflib
import subprocess
from pathlib import Path

PIN='5b13915ff334edb8568d76630268bdfd3f639784'
ap=argparse.ArgumentParser()
ap.add_argument('checkout',type=Path)
ap.add_argument('--patch',type=Path,required=True)
args=ap.parse_args()
head=subprocess.check_output(['git','-C',str(args.checkout),'rev-parse','HEAD'],text=True).strip()
if head!=PIN: raise SystemExit('Refusing a checkout that is not the pinned Walls commit')
original=subprocess.check_output(['git','-C',str(args.checkout),'show',f'{PIN}:src/uci.cpp'],text=True)
modified=original.replace('#include "evaluate.h"','#include "evaluate.h"\n#include "apiutil.h"')
diagnostic='''      else if (token == "evalraw") {
          Eval::NNUE::verify();
          sync_cout << "Legacy raw adjusted: " << Eval::NNUE::evaluate(pos, true)
                    << "\\nLegacy raw unadjusted: " << Eval::NNUE::evaluate(pos, false)
                    << "\\nLegacy final: " << Eval::evaluate(pos)
                    << "\\nLegacy material: " << pos.non_pawn_material(WHITE) << " " << pos.non_pawn_material(BLACK)
                    << sync_endl;
      }
      else if (token == "rulesraw") {
          const MoveList<LEGAL> legal(pos);
          Value immediateValue = VALUE_ZERO, optionalValue = VALUE_ZERO;
          const bool immediate = pos.is_immediate_game_end(immediateValue);
          const bool optional = pos.is_optional_game_end(optionalValue);
          const Value terminal = legal.size() ? VALUE_ZERO : pos.checkers() ? pos.checkmate_value() : pos.stalemate_value();
          sync_cout << "Check: " << bool(checked(pos))
                    << "\\nImmediate: " << immediate << " " << immediateValue
                    << "\\nOptional: " << optional << " " << optionalValue
                    << "\\nTerminal: " << (legal.size() == 0) << " " << terminal
                    << "\\nInsufficient: " << has_insufficient_material(WHITE, pos)
                    << " " << has_insufficient_material(BLACK, pos) << sync_endl;
          for (const auto& entry : legal) {
              const Move move = entry;
              const std::string san = SAN::move_to_san(pos, move, NOTATION_SAN);
              sync_cout << "Move: " << UCI::move(pos, move) << " " << san << sync_endl;
          }
      }
'''
marker='      else if (token == "eval")     trace_eval(pos);'
if original.count(marker)!=1: raise SystemExit('Pinned diagnostic anchor changed')
modified=modified.replace(marker,diagnostic+marker)
(args.checkout/'src/uci.cpp').write_text(modified,newline='\n')
args.patch.write_text(''.join(difflib.unified_diff(original.splitlines(True),modified.splitlines(True),fromfile='a/src/uci.cpp',tofile='b/src/uci.cpp')),newline='\n')
print('Read-only diagnostics prepared; build a separately named referee executable.')
