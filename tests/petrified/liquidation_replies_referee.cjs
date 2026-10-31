// SPDX-License-Identifier: GPL-3.0-or-later
// Full real histories and board transitions, using the pinned independent client.
const fs=require('node:fs'),assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const {setPosition,legal,piece,toChess}=require('./liquidation_referee_helpers.cjs');
const [cases,bundle,modules,out]=process.argv.slice(2),create=ref.load(bundle,modules);
const rows=fs.readFileSync(cases,'utf8').trim().split('\n').map(JSON.parse);
let histories=0,historyPlies=0,children=0,events=0,lists=0,terminals=0;
const logs=[];
function reconstruct(row) {
  const e=create();setPosition(e,row.initial);
  for(const m of row.history) {assert(!e.gameOver(),'history after settlement');ref.play(e,m);++historyPlies;}
  assert(!e.gameOver(),'terminal parent exported');return e;
}
for(const row of rows) {
  const p=reconstruct(row);++histories;
  assert.deepEqual(legal(p),[...row.legal].sort(),'parent legal list '+row.fen);++lists;
  for(const child of row.children) {
    const e=reconstruct(row),m=child.move,from=toChess(m.slice(0,2)),to=toChess(m.slice(2,4));
    const before=ref.status(e),fatal=!!e.isCapture(from,to) && piece(e,from).piece!=='P';
    ref.play(e,m);++children;
    // Derive the obligation from the independent move and resulting check.
    const expected=fatal && !e.inCheck();assert.equal(expected,child.reply,'event predicate '+m+' '+row.fen);
    if(fatal) {const wall=piece(e,to);assert(wall && (wall.dead || wall.player===4),'capture did not form wall');}
    const actual=ref.status(e);assert.equal(actual.terminal,child.terminal,'terminal '+m+' '+row.fen);
    assert.equal(actual.result,child.result,'settlement '+m+' '+row.fen);
    if(child.terminal) ++terminals;
    if(expected) {
      ++events;
      if(!actual.terminal) {assert.deepEqual(legal(e),[...child.legal_replies].sort(),'reply set');++lists;}
    }
    logs.push({initial:row.initial,history:row.history,move:m,reply:expected,before,after:actual});
  }
}
assert(histories>40 && children>500 && events>20 && terminals>0);
const report={result:'PASS',positions:histories,legal_lists:lists,reference_children:children,
  real_history_plies:historyPlies,petrification_replies:events,terminal_settlements:terminals,
  terminal_exclusions:0,referee_sha256:ref.HASH,logs};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
