// SPDX-License-Identifier: GPL-3.0-or-later
// Play histories and all possible next moves in the independent pinned client.
const fs=require('node:fs'),assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [casesFile,bundle,modules,out]=process.argv.slice(2);
const create=ref.load(bundle,modules);
const to=s=>String.fromCharCode(s.charCodeAt(0)+3)+(Number(s.slice(1))+3);
const from=s=>String.fromCharCode(s.charCodeAt(0)-3)+(Number(s.slice(1))-3);
function position(row) {
  const e=create();ref.setFen(e,row.initial);e.position(e.makeFen());
  for(const m of row.history){assert(!e.gameOver());ref.play(e,m);}
  return e;
}
function legal(e) {
  const moves=[];
  for(let r=1;r<=8;r++)for(let f=0;f<8;f++){
    const origin=String.fromCharCode(97+f)+r,sq=to(origin);
    const [x,y]=e.sq2ij(sq),pc=e.board()[x][y];
    for(const dest of e.getDestinations(sq)){
      let d=from(dest);
      if(pc && pc.piece==='K' && (origin==='e1'||origin==='e8')){
        if(d==='h'+r)d='g'+r;if(d==='a'+r)d='c'+r;
      }
      if(e.isPromotion(sq,dest))for(const p of 'qrbn')moves.push(origin+d+p);
      else moves.push(origin+d);
    }
  }
  return [...new Set(moves)].sort();
}
let lists=0,children=0,historyPlies=0,wins=0,terminal=0;
const logs=[];
for(const row of fs.readFileSync(casesFile,'utf8').trim().split('\n').map(JSON.parse)){
  const e=position(row),before=ref.status(e);assert(!before.terminal);
  const moves=legal(e);assert.deepEqual(moves,[...row.legal].sort(),row.fen);++lists;
  const expected=[];
  for(const move of moves){
    const child=position(row);ref.play(child,move);const end=ref.status(child);++children;historyPlies+=row.history.length+1;
    if(end.terminal){
      ++terminal;
      // Only genuine independent repetition settlements can certify this policy.
      const repetition=/repetition/i.test(String(end.reason));
      if(repetition && end.result===(before.side==='w'?'1-0':'0-1'))expected.push(move);
      logs.push({initial:row.initial,history:row.history,move,result:end.result,reason:end.reason,points:end.points});
    }
  }
  assert.deepEqual(expected.sort(),[...row.winning_repetitions].sort(),row.fen);
  wins+=expected.length;
}
assert(lists>100 && wins>0 && terminal>0);
const report={result:'PASS',positions:lists,legal_lists:lists,children,independent_history_plies:historyPlies,winning_repetitions:wins,terminal_settlements:terminal,referee_sha256:ref.HASH,terminal_exclusions:0,logs};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
