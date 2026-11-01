// SPDX-License-Identifier: GPL-3.0-or-later
// Independently enumerate terminal quiet wins in real capture histories.
const fs=require('node:fs'),assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const {setPosition,legal,piece,toChess}=require('./settlement_referee_helpers.cjs');
const [cases,bundle,modules,out]=process.argv.slice(2),create=ref.load(bundle,modules);
const rows=fs.readFileSync(cases,'utf8').trim().split('\n').map(JSON.parse);
let histories=0,historyPlies=0,children=0,events=0,lists=0,terminals=0,quietChildren=0,winningProofs=0;
const logs=[];
function reconstruct(row,childMove) {
  const e=create();setPosition(e,row.initial);
  for(const m of row.history) {assert(!e.gameOver(),'history after settlement');ref.play(e,m);++historyPlies;}
  assert(!e.gameOver(),'terminal parent exported');
  if(childMove) ref.play(e,childMove);
  return e;
}
for(const row of rows) {
  const p=reconstruct(row);++histories;
  assert.deepEqual(legal(p),[...row.legal].sort(),'parent legal list '+row.fen);++lists;
  for(const child of row.children) {
    const e=reconstruct(row),m=child.move,from=toChess(m.slice(0,2)),to=toChess(m.slice(2,4));
    const fatal=!!e.isCapture(from,to) && piece(e,from).piece!=='P';
    ref.play(e,m);++children;
    if(fatal) {const wall=piece(e,to);assert(wall && (wall.dead || wall.player===4),'missing new wall');}
    const actual=ref.status(e);assert.equal(actual.terminal,child.terminal,'terminal '+m+' '+row.fen);
    assert.equal(actual.result,child.result,'settlement '+m+' '+row.fen);
    if(child.terminal) {++terminals;logs.push({initial:row.initial,history:row.history,move:m,after:actual});continue;}
    const eligible=fatal && !e.inCheck();assert.equal(eligible,child.eligible,'eligibility mismatch');
    const winning=[];
    if(eligible) {
      ++events;const player=e.turn(),replies=legal(e);++lists;
      for(const q of replies) {
        const f=toChess(q.slice(0,2)),t=toChess(q.slice(2,4));
        if(e.isCapture(f,t) || (q.length===5 && q[4]==='q'))continue;
        const next=reconstruct(row,m);ref.play(next,q);++quietChildren;const end=ref.status(next);
        if(end.terminal && end.result===(player===0?'1-0':'0-1'))winning.push(q);
      }
    }
    assert.deepEqual(winning.sort(),[...child.winning_quiets].sort(),'winning quiet set '+m+' '+row.fen);
    if(winning.length) {assert(winning.includes(child.proof_move),'proof move outside winning set');++winningProofs;}
    else assert.equal(child.proof_move,'0000','false terminal proof');
    logs.push({initial:row.initial,history:row.history,move:m,eligible,winning,proof:child.proof_move,after:actual});
  }
}
assert(histories>40 && children>500 && events>20 && terminals>0 && winningProofs>0);
const report={result:'PASS',positions:histories,legal_lists:lists,reference_children:children,
  quiet_reference_children:quietChildren,real_history_plies:historyPlies,petrification_events:events,
  terminal_settlements:terminals,winning_proofs:winningProofs,terminal_exclusions:0,referee_sha256:ref.HASH,logs};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
