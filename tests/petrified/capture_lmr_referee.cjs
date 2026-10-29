// SPDX-License-Identifier: GPL-3.0-or-later
// Independent immediate capture/promotion transaction audit for depth allocation.
// The public client is external, hash-pinned, and never distributed here.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const ref = require('./chesscom_reference.cjs');
const [casesFile, bundle, modules, output] = process.argv.slice(2);
const create = ref.load(bundle, modules);
const values = {P:208, N:781, B:825, R:1276, Q:2538, K:0};
const toChess = s => String.fromCharCode(s.charCodeAt(0)+3)+(Number(s.slice(1))+3);
const rows = fs.readFileSync(casesFile,'utf8').trim().split('\n').map(JSON.parse);
let plies = 0, recaptures = 0, promotions = 0, ep = 0;
const logs = [], results = [];
class TerminalSettlement extends Error {}

function material(e, player) {
  let score = 0;
  for (const file of e.board()) for (const pc of file)
    if (pc && !pc.dead && (pc.player === 0 || pc.player === 2)) {
      assert(pc.piece in values, 'Unexpected live piece');
      score += (pc.player === player ? 1 : -1)*values[pc.piece];
    }
  return score;
}
function piece(e, sq) { const [x,y] = e.sq2ij(sq); return e.board()[x][y]; }
function capturedSquare(e, move) {
  if (!e.isEnPassantCapture(move.from,move.to)) return move.to;
  const [x,y] = e.sq2ij(move.to);
  return e.ij2sq([x,y+(e.turn()===0?-1:1)]);
}
function apply(e, move) {
  const player = e.turn(), before = material(e,player), beforeFen = e.fen();
  const capture = e.isCapture(move.from,move.to);
  const victim = capture ? piece(e,capturedSquare(e,move)) : null;
  const earned = victim ? e.getPointValue(victim,player) : 0;
  if (e.isEnPassantCapture(move.from,move.to)) ++ep;
  if (move.promoteTo) ++promotions;
  assert(e.play(move), 'Referee rejected '+JSON.stringify(move));
  ++plies;
  const delta = material(e,player)-before+100*earned;
  logs.push({before:beforeFen, move, earned_capture_points:earned,
    material_points_delta:delta, after:e.fen(), terminal:e.gameOver(), reason:e.termination()});
  if (e.gameOver()) throw new TerminalSettlement(e.termination());
  return delta;
}
for (let i=0;i<rows.length;++i) {
  try {
  const row=rows[i], e=create();
  if (row.history) {
    let terminal=false;
    for (const move of row.history) {
      if (e.gameOver()) { terminal=true; break; }
      ref.play(e,move);
    }
    if (terminal || e.gameOver()) {
      results.push({case:i,status:'SKIPPED_TERMINAL_HISTORY'});continue;
    }
  } else ref.setFen(e,row.fen);
  const move={from:toChess(row.move.slice(0,2)),to:toChess(row.move.slice(2,4))};
  if(row.move.length===5)move.promoteTo=row.move[4].toUpperCase();
  const pc=piece(e,move.from);
  const actual=apply(e,move);
  const result={case:i,fen:row.fen,move:row.move,expected:row.value,actual,status:actual===row.value?'PASS':'MISMATCH'};
  results.push(result);
  if(actual!==row.value) {
    fs.writeFileSync(output,JSON.stringify({result:'FAIL',results,logs},null,2)+'\n');
    throw Error(JSON.stringify(result));
  }
  } catch (error) {
    if (!(error instanceof TerminalSettlement)) throw error;
    results.push({case:i,status:'SKIPPED_TERMINAL_SETTLEMENT',reason:error.message});
  }
}
const report={result:'PASS',referee_sha256:ref.HASH,
  cases_sha256:crypto.createHash('sha256').update(fs.readFileSync(casesFile)).digest('hex'),
  compared:results.filter(r=>r.status==='PASS').length,
  skipped_terminal_settlements:results.filter(r=>r.status==='SKIPPED_TERMINAL_SETTLEMENT').length,
  skipped_terminal_histories:results.filter(r=>r.status==='SKIPPED_TERMINAL_HISTORY').length,
  independent_transaction_plies:plies,recaptures,promotions,en_passant:ep,
  objective:'Immediate live material plus capture points; no replies, terminal bonuses or positional wall value',
  results,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,results:undefined,logs:undefined}));
