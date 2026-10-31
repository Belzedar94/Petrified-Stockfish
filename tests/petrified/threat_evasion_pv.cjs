// SPDX-License-Identifier: GPL-3.0-or-later
const fs=require('node:fs'), assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [searchFile,bundle,modules,output]=process.argv.slice(2);
const create=ref.load(bundle,modules);
const toChess=s=>String.fromCharCode(s.charCodeAt(0)+3)+(Number(s[1])+3);
let plies=0,quietResponses=0; const logs=[];
for(const row of JSON.parse(fs.readFileSync(searchFile,'utf8')).cases) {
  const e=create(); ref.setFen(e,row.fen); const transitions=[];
  assert(row.pv.length>=2);
  for(let i=0;i<row.pv.length;++i) {
    const move=row.pv[i]; assert(!e.gameOver(),'PV continues after settlement');
    if(i===1) {
      assert(!e.inCheck(),'fixture leaf must not be in check');
      let legal=0,captures=0;
      for(const ij of e.pieces(e.turn())) {
        const from=e.ij2sq(ij);
        for(const to of e.getDestinations(from)) { ++legal; captures+=e.isCapture(from,to); }
      }
      assert(legal>1 && captures===0,'fixture must have choices and no captures');
      assert(!e.isCapture(toChess(move.slice(0,2)),toChess(move.slice(2,4)))); ++quietResponses;
    }
    ref.play(e,move); ++plies; transitions.push({move,...ref.status(e)});
  }
  logs.push({fen:row.fen,transitions});
}
assert(quietResponses===logs.length && logs.length===24);
const report={result:'PASS',searches:logs.length,quiet_responses:quietResponses,
 independent_plies:plies,referee_sha256:ref.HASH,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,logs:undefined}));
