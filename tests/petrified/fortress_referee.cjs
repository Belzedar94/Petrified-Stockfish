// SPDX-License-Identifier: GPL-3.0-or-later
// Coordinate/set reference plus legal moves and actual play in the pinned client.
const fs=require('node:fs'), assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [casesFile,bundle,modules,output]=process.argv.slice(2);
const create=ref.load(bundle,modules);
const xy=s=>[s%8,Math.floor(s/8)];
const square=s=>String.fromCharCode(97+s%8)+(1+Math.floor(s/8));
const toChess=s=>String.fromCharCode(s.charCodeAt(0)+3)+(Number(s[1])+3);
const fromChess=s=>String.fromCharCode(s.charCodeAt(0)-3)+(Number(s.slice(1))-3);
function decode(fen){
  const f=fen.split(' '),b=Array(64).fill('.');
  f[0].split('/').forEach((row,r)=>{let c=0;for(const p of row){if(/[1-8]/.test(p))c+=Number(p);else b[(7-r)*8+c++]=p;}});
  return {b,turn:f[1]==='w'?0:1,points:f[6].slice(7).split(',').map(Number),rights:f[2],ep:f[3]};
}
const owner=p=>p==='.'||p==='*'?-1:p===p.toUpperCase()?0:1;
function neighbours(s){const [x,y]=xy(s),a=[];for(let dx=-1;dx<=1;dx++)for(let dy=-1;dy<=1;dy++)if((dx||dy)&&x+dx>=0&&x+dx<8&&y+dy>=0&&y+dy<8)a.push((y+dy)*8+x+dx);return a;}
function flood(k,blocked){const set=new Set([k]),q=[k];while(q.length)for(const t of neighbours(q.shift()))if(!blocked.has(t)&&!set.has(t)){set.add(t);q.push(t);}return set;}
const knight=(a,b)=>{const [x,y]=xy(a),[u,v]=xy(b);return Math.abs(x-u)*Math.abs(y-v)===2;};
const pawnhits=(a,b,side)=>{const[x,y]=xy(a),[u,v]=xy(b);return Math.abs(x-u)===1&&v-y===(side===0?1:-1);};
function legal(e){
  const out=[];
  for(let s=0;s<64;s++){const from=toChess(square(s));for(const to of e.getDestinations(from)){
    const dest=fromChess(to),m=square(s)+dest;
    if(e.isPromotion(from,to))for(const p of 'qrbn')out.push(m+p);else out.push(m);
  }}return [...new Set(out)].sort();
}
function importFen(e,fen){ref.setFen(e,fen);e.position(e.makeFen());}
function slow(fen,moves){
  const {b,turn,points,rights,ep}=decode(fen);
  if(rights!=='-'||ep!=='-'||!b.includes('*')||b.some((p,s)=>p.toLowerCase()==='p'&&(s<8||s>=56)))return null;
  const at=(color,type)=>b.flatMap((p,s)=>owner(p)===color&&(!type||type.includes(p.toLowerCase()))?[s]:[]);
  for(const d of [0,1]){
    const a=1-d,ownP=at(d,'p'),ownN=at(d,'n'),oppN=at(a,'n'),oppP=at(a,'p');
    if(at(d,'qrb').length||ownN.length>1||oppN.length>1||oppP.length>1)continue;
    const margin=points[d]-points[a]-at(a,'qrb').length-oppN.length*(ownN.length?3:1);
    if(margin<=0)continue;
    const k=at(d,'k')[0],blocked=new Set(b.flatMap((p,s)=>p==='*'||ownP.includes(s)?[s]:[])),inside=flood(k,blocked);
    if(at(a,'qrbk').some(s=>inside.has(s))||ownN.some(s=>!inside.has(s)))continue;
    const boxes=[];for(let y=0;y<7;y++)for(let x=0;x<7;x++){
      const box=[8*y+x,8*y+x+1,8*(y+1)+x,8*(y+1)+x+1];
      if(box.includes(k)&&box.every(s=>inside.has(s)&&(b[s]==='.'||s===k||oppN.includes(s))))boxes.push(box);
    }
    if(!boxes.length)continue;
    let safe=true,interceptors=[];
    if(oppP.length){
      const start=oppP[0],dir=a===0?1:-1;
      if(Math.floor(start/8)===(a===0?1:6))continue;
      const reached=new Set([start]),queue=[start];
      while(queue.length){const s=queue.shift(),[x,y]=xy(s);
        for(const[u,v]of[[x-1,y],[x+1,y],[x,y+dir]])if(u>=0&&u<8&&v>=0&&v<8){
          const t=8*v+u;if(!blocked.has(t)&&!reached.has(t)){reached.add(t);queue.push(t);}
        }
      }
      for(const to of reached){
        if(inside.has(to)||Math.floor(to/8)===(a===0?7:0)){safe=false;break;}
        for(const from of ownP.filter(s=>pawnhits(to,s,a))){
          const after=new Set(blocked);after.delete(from);after.add(to);
          if(Math.floor(to/8)===(d===0?7:0)||[...flood(k,after)].some(s=>!inside.has(s)&&s!==from)){safe=false;break;}
        }
      }
      const attacked=ownP.filter(s=>pawnhits(start,s,a));
      if(attacked.length){
        interceptors=attacked.map(s=>square(s)+square(start)).filter(m=>moves.includes(m));
        if(turn!==d||!interceptors.length)safe=false;
      }
    }
    if(!safe||(turn===a&&oppN.some(n=>knight(n,k))))continue;
    if(turn===d&&!interceptors.length&&!boxes.some(box=>moves.some(m=>m.slice(0,2)===square(k)&&box.some(s=>m.slice(2)===square(s)))))continue;
    return {defender:d,margin,interceptors};
  }return null;
}
let positions=0,certificates=0,plies=0,terminal=0;const logs=[];
for(const row of fs.readFileSync(casesFile,'utf8').trim().split('\n').map(JSON.parse)){
  // Random walk exports can contain EP/castles; inherited tests validate these.
  // The certificate rejects both, and their importer is not used here.
  const fields=row.fen.split(' ');
  if(fields[2]!=='-'||fields[3]!=='-'){assert(!row.accepted);continue;}
  const e=create();importFen(e,row.fen);const moves=legal(e);
  assert.deepEqual(moves,[...row.legal].sort(),row.fen);
  const proof=slow(row.fen,moves);assert.equal(!!proof,row.accepted,row.fen);++positions;
  if(proof){assert.equal(proof.defender,row.defender);assert.equal(proof.margin,row.margin);++certificates;}
  for(const line of row.lines){
    const oracle=create();importFen(oracle,row.fen);const steps=[];
    for(const move of line.moves){assert(!oracle.gameOver());assert(legal(oracle).includes(move));ref.play(oracle,move);++plies;steps.push({move,...ref.status(oracle)});}
    const end=ref.status(oracle);assert.equal(end.result,line.result);assert.deepEqual(end.points,line.points);
    if(end.terminal){++terminal;assert.equal(end.result,row.defender===0?'1-0':'0-1');}
    else {
      const child=slow(line.after,legal(oracle));assert(child&&child.defender===row.defender,'independent invariant closure');
      const board=decode(line.after).b;
      for(let s=0;s<64;s++){
        const [x,y]=oracle.sq2ij(toChess(square(s))),actual=oracle.board()[x][y],wanted=board[s];
        if(wanted==='.')assert(!actual,'empty square');
        else if(wanted==='*')assert(actual&&actual.player===4&&actual.piece==='X','wall');
        else assert(actual&&!actual.dead&&actual.player===(owner(wanted)===0?0:2)&&actual.piece===wanted.toUpperCase(),'live piece');
      }
    }
    logs.push({fen:row.fen,steps});
  }
}
assert(positions>100&&certificates>50&&plies>1000&&terminal>0);
const report={result:'PASS',positions,certificates,independent_plies:plies,terminal_settlements:terminal,referee_sha256:ref.HASH,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:'see report'}));
