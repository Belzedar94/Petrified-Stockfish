// SPDX-License-Identifier: GPL-3.0-or-later
const fs=require('node:fs'),assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const {setPosition,legal,piece,toChess}=require('./settlement_referee_helpers.cjs');
const [input,bundle,modules,out]=process.argv.slice(2),create=ref.load(bundle,modules);
const rows=JSON.parse(fs.readFileSync(input)).cases;let plies=0,quietTerminals=0;const logs=[];
for(const row of rows) {
  const e=create();setPosition(e,row.fen);const trace=[];
  for(let i=0;i<row.pv.length;++i) {
    const m=row.pv[i],f=toChess(m.slice(0,2)),t=toChess(m.slice(2,4));
    assert(!e.gameOver());assert(legal(e).includes(m),'illegal PV');
    if(i===0) {assert(e.isCapture(f,t));assert.notEqual(piece(e,f).piece,'P');}
    else {assert(legal(e).length>1,'reply must be branching');assert(!e.isCapture(f,t),'reply must be quiet');}
    ref.play(e,m);++plies;trace.push({move:m,...ref.status(e)});
    if(i===0) assert(!e.gameOver(),'capture itself settled the fixture');
  }
  assert(e.gameOver());assert.equal(ref.status(e).result,row.result);++quietTerminals;
  logs.push({fen:row.fen,threads:row.threads,depth:row.depth,mode:row.mode,trace});
}
const report={result:'PASS',searches:rows.length,independent_pv_plies:plies,quiet_terminal_replies:quietTerminals,
  terminal_exclusions:0,referee_sha256:ref.HASH,logs};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
