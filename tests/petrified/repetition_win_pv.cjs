// SPDX-License-Identifier: GPL-3.0-or-later
const fs=require('node:fs'),assert=require('node:assert/strict'),ref=require('./chesscom_reference.cjs');
const [searchFile,bundle,modules,out]=process.argv.slice(2),create=ref.load(bundle,modules);
let plies=0,historyPlies=0;const logs=[];
for(const row of JSON.parse(fs.readFileSync(searchFile,'utf8')).cases){
  const e=create();ref.setFen(e,row.initial);e.position(e.makeFen());
  for(const m of row.history){assert(!e.gameOver());ref.play(e,m);++historyPlies;}
  const transitions=[];
  for(const m of row.pv){assert(!e.gameOver());ref.play(e,m);++plies;transitions.push({move:m,...ref.status(e)});}
  const end=ref.status(e);assert(end.terminal);assert.equal(end.result,row.result);assert(/repetition/i.test(String(end.reason)));
  logs.push({initial:row.initial,history:row.history,transitions});
}
assert(plies>0);
const report={result:'PASS',searches:logs.length,independent_pv_plies:plies,independent_history_plies:historyPlies,referee_sha256:ref.HASH,logs};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
