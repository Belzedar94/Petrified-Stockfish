// SPDX-License-Identifier: GPL-3.0-or-later
// Independent legal threat/pass reconstruction and restricted SEE audit.
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
function reply(e, target) {
  const occupant = piece(e,target);
  if (!occupant || occupant.player===4 || occupant.piece==='K') return 0;
  // The model does not value settlement bonuses. A terminal board has no legal
  // continuation in the external client; report rather than silently truncate
  // any case for which the local material model would need one.
  if (e.gameOver()) return 0;
  let best = 0;
  const captures = [];
  for (const ij of e.pieces(e.turn())) {
    const from = e.ij2sq(ij);
    for (const to of e.getDestinations(from)) {
      const move = {from,to};
      if (!e.isCapture(from,to) || capturedSquare(e,move)!==target) continue;
      if (e.isPromotion(from,to))
        for (const promoteTo of ['Q','R','B','N']) captures.push({...move,promoteTo});
      else captures.push(move);
    }
  }
  const before = e.fen();
  for (const move of captures) {
    const delta = apply(e,move); ++recaptures;
    const gain = delta-reply(e,move.to);
    e.position(before);
    best = Math.max(best,gain);
  }
  return best;
}


const fromChess = s => String.fromCharCode(s.charCodeAt(0)-3)+(Number(s.slice(1))-3);
function legal(e) {
  const moves=[];
  for (let rank=1;rank<=8;++rank) for (let file=0;file<8;++file) {
    const from=toChess(String.fromCharCode(97+file)+rank);
    for (const to of e.getDestinations(from)) {
      let destination=fromChess(to); const origin=fromChess(from);
      // The client accepts both king-destination and rook-square castle aliases.
      if ((origin==='e1' || origin==='e8') && piece(e,from).piece==='K') {
        if(destination==='h'+origin[1]) destination='g'+origin[1];
        if(destination==='a'+origin[1]) destination='c'+origin[1];
      }
      const base=origin+destination;
      if(e.isPromotion(from,to)) for(const p of 'qrbn') moves.push(base+p);
      else moves.push(base);
    }
  }
  return [...new Set(moves)].sort();
}

let legalLists=0, predicateChecks=0, positives=0, skipped=0, capturesCompared=0;
for(let i=0;i<rows.length;++i) {
  const row=rows[i], current=create(); ref.setFen(current,row.fen);
  assert.equal(current.inCheck(),row.in_check,'check guard');
  const currentMoves=legal(current);
  assert.deepEqual(currentMoves,[...row.legal].sort(),row.fen); ++legalLists;
  const captureStage=currentMoves.some(m=>current.isCapture(toChess(m.slice(0,2)),toChess(m.slice(2,4))) || m[4]==='q');
  assert.equal(captureStage,row.capture_stage,'own capture-stage guard');
  const eligible=!current.inCheck() && currentMoves.length>1 && !captureStage;
  if(current.inCheck()) { assert(!row.threat); ++predicateChecks; continue; }
  const fields=row.fen.split(' '); fields[1]=fields[1]==='w'?'b':'w'; fields[3]='-';
  assert.equal(fields.join(' '),row.pass_fen,'explicit pass semantics');
  const e=create(); ref.setFen(e,fields.join(' '));
  const enemy=legal(e).filter(m=>e.isCapture(toChess(m.slice(0,2)),toChess(m.slice(2,4))));
  assert.deepEqual(enemy,[...row.captures].map(c=>c.move).sort(),'enemy legal captures'); ++legalLists;
  let profitable=false, complete=true;
  const before=e.fen(), audit=[];
  for(const expected of row.captures) {
    const move={from:toChess(expected.move.slice(0,2)),to:toChess(expected.move.slice(2,4))};
    if(expected.move[4]) move.promoteTo=expected.move[4].toUpperCase();
    try {
      const actual=apply(e,move)-reply(e,move.to);
      assert.equal(actual,expected.value,row.fen+' '+expected.move);
      profitable ||= actual>0; ++capturesCompared;
      audit.push({move:expected.move,actual,status:'PASS'});
    } catch(error) {
      if(!(error instanceof TerminalSettlement)) throw error;
      complete=false; ++skipped; audit.push({move:expected.move,status:'TERMINAL_SETTLEMENT_EXCLUDED',reason:error.message});
    } finally { e.position(before); }
  }
  if(!eligible || complete) {
    assert.equal(eligible && profitable,row.threat,row.fen);
    ++predicateChecks; positives+=Number(row.threat);
  }
  results.push({case:i,fen:row.fen,eligible,captures:audit,full_predicate_compared:!eligible || complete});
}
assert(legalLists>100 && capturesCompared>20 && positives>0 && predicateChecks>positives);
const report={result:'PASS',referee_sha256:ref.HASH,
 cases_sha256:crypto.createHash('sha256').update(fs.readFileSync(casesFile)).digest('hex'),
 cases:rows.length,legal_lists:legalLists,predicates:predicateChecks,positive_threats:positives,
 captures_compared:capturesCompared,independent_exchange_plies:plies,recaptures,promotions,
 skipped_terminal_capture_exchanges:skipped,
 objective:'Hypothetical opposite side with EP cleared, not a legal pass. Optional legal local recaptures; material and capture points, no settlement bonuses. Terminal capture exchanges explicitly excluded, never zero-filled.',results,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,results:undefined,logs:undefined}));
