// SPDX-License-Identifier: GPL-3.0-or-later
// All histories are replayed from start; clone preserves the client's repetition map.
const fs=require('fs'),path=require('path'),assert=require('node:assert/strict'),crypto=require('crypto');
const [root,phase]=process.argv.slice(2),task=path.join(root,'.local/pr18-diagnosis-v1');
const ref=require(path.join(root,'tests/petrified/chesscom_reference.cjs'));
const create=ref.load(path.join(root,'.local/points-repetition-win-v1-inputs/variants.js'),path.join(root,'.local/fairyground/node_modules'));
const read=p=>JSON.parse(fs.readFileSync(p,'utf8'));
const bank=read(path.join(task,'bank.json')),histories=read(path.join(task,'histories.json'));
const to=s=>String.fromCharCode(s.charCodeAt(0)+3)+(Number(s.slice(1))+3),from=s=>String.fromCharCode(s.charCodeAt(0)-3)+(Number(s.slice(1))-3);
function legal(e){const a=[];for(let r=1;r<=8;r++)for(let f=0;f<8;f++){
 const origin=String.fromCharCode(97+f)+r,sq=to(origin),[x,y]=e.sq2ij(sq),pc=e.board()[x][y];
 for(const dest of e.getDestinations(sq)){let d=from(dest);if(pc&&pc.piece==='K'&&(origin==='e1'||origin==='e8')){if(d==='h'+r)d='g'+r;if(d==='a'+r)d='c'+r;}
 if(e.isPromotion(sq,dest))for(const p of 'qrbn')a.push(origin+d+p);else a.push(origin+d);}}
 return [...new Set(a)].sort();}
function compareBoard(e,fen,terminal=false){const f=fen.split(' '),board=f[0].split('/').map(rank=>[...rank].flatMap(c=>/[1-8]/.test(c)?Array(+c).fill(''):[c]));
 for(let r=0;r<8;r++)for(let c=0;c<8;c++){const [x,y]=e.sq2ij(to(String.fromCharCode(97+c)+(8-r))),p=e.board()[x][y],expected=board[r][c];
  if(!expected)assert(!p);else if(expected==='*')assert(p&&(p.dead||(p.player===4&&p.piece==='X')),'Expected permanent dead wall');else{assert(p&&(terminal||!p.dead),"Missing piece before settlement");assert.equal(p.piece,expected.toUpperCase());assert.equal(p.player,expected===expected.toUpperCase()?0:2);}}
 const status=ref.status(e);if(!terminal){assert.equal(status.side,f[1]);assert.deepEqual(status.points,f[6].slice(7).split(',').map(Number));}
 if(!terminal)assert.equal(Number(status.fen4.split('-')[6]),Number(f[4]));}
let positions=0,children=0,historyPlies=0,pvPlies=0,searches=0,terminalChildren=0,cloneReplayChecks=0;
const log=fs.openSync(path.join(task,phase+'-004-independent.jsonl'),'wx');
function note(x){fs.writeSync(log,JSON.stringify(x)+'\n');}
for(const h of histories){let e=create();const rows=bank.filter(r=>r.history_id===h.id).sort((a,b)=>a.ply-b.ply);let ply=0;
 for(const row of rows){while(ply<row.ply){assert(!e.gameOver());ref.play(e,h.moves[ply++]);historyPlies++;}
  assert(!e.gameOver());compareBoard(e,row.fen);const data=read(path.join(task,'extract',row.id+'.json')).data;
  if(phase==='extract'){
   assert.deepEqual(legal(e),data.children.map(c=>c.move).sort(),row.id);positions++;
   const clone=e.cloneFPC();assert.deepEqual(ref.status(clone),ref.status(e));assert.deepEqual(clone.getRepetitions(),e.getRepetitions());
   for(const child of data.children){const ce=e.cloneFPC();ref.play(ce,child.move);const end=ref.status(ce);children++;
    assert.equal(!!end.terminal,child.terminal,row.id+' '+child.move);assert.equal(end.result,child.result,row.id+' '+child.move);compareBoard(ce,child.fen,!!end.terminal);
    if(end.terminal){terminalChildren++;const f=child.fen.split(' '),points=f[6].slice(7).split(',').map(Number);if(child.reason==='checkmate')points[f[1]==='w'?1:0]+=20;else{assert(['stalemate','insufficient-material','fifty-move-rule','threefold-repetition'].includes(child.reason));points[0]+=10;points[1]+=10;}assert.deepEqual(end.points,points);if(child.reason==='fifty-move-rule')assert.equal(+f[4],100);note({id:row.id,move:child.move,native_fen:child.fen,native_reason:child.reason,...end});}}
   // One full-history child replay per original history checks clone semantics.
   if(row===rows[0]&&data.children.length){const direct=create();for(const m of row.history)ref.play(direct,m);ref.play(direct,data.children[0].move);const cloned=e.cloneFPC();ref.play(cloned,data.children[0].move);assert.deepEqual(ref.status(cloned),ref.status(direct));cloneReplayChecks++;}
  }else{
   let queries=read(path.join(task,'baseline',row.id+'.json')).searches;
   if(row.controls)queries=queries.concat(read(path.join(task,'controls',row.id+'.json')).searches);
   for(const q of queries){const ce=e.cloneFPC();const transitions=[];
    if(q.bestmove==='0000'){assert(q.action==='claim-win'||q.action==='claim-draw');ce.resign();assert.equal(ref.status(ce).result,q.action==='claim-draw'?'1/2-1/2':ref.status(e).side==='w'?'1-0':'0-1');}
    else {assert(data.children.some(c=>c.move===q.bestmove));if(q.pv.length)assert.equal(q.pv[0],q.bestmove);for(const m of q.pv){assert(!ce.gameOver(),row.id+' PV after terminal');ref.play(ce,m);pvPlies++;transitions.push({move:m,...ref.status(ce)});}}
    searches++;note({id:row.id,mode:q.mode,budget:q.budget,bestmove:q.bestmove,action:q.action,transitions});}
   positions++;
  }
 }
 if(phase==='extract'){while(ply<h.moves.length){assert(!e.gameOver());ref.play(e,h.moves[ply++]);historyPlies++;}if(!e.gameOver())e.resign();assert.equal(ref.status(e).result,h.result,h.id);}
}
fs.closeSync(log);assert.equal(positions,2048);
const result={result:'PASS',histories:histories.length,positions,children,terminalChildren,cloneReplayChecks,historyPlies,pvPlies,searches,terminal_exclusions:0,terminal_presentation:"Post-settlement side and dead flags are cosmetic; placement, earned-plus-settlement points and result checked; post-settlement clock/turn/dead flags are presentation and both FENs retained",referee_sha256:ref.HASH,log_sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(task,phase+'-004-independent.jsonl'))).digest('hex')};
fs.writeFileSync(path.join(task,phase+'-004-audit.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
