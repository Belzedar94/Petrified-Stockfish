// SPDX-License-Identifier: GPL-3.0-or-later
// Independent client board/capture economics and coordinate geometry audit.
const fs=require('node:fs'), assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [cases,bundle,modules,output]=process.argv.slice(2), create=ref.load(bundle,modules);
const values={P:208,N:781,B:825,R:1276,Q:2538,K:0}, types='PNBRQK';
const permitted={P:'P',N:'PNB',B:'PNB',R:'PNBR',Q:'PNBRQ',K:''};
const sq=(x,y)=>String.fromCharCode(100+x)+(4+y);
const pc=(e,x,y)=>{const [i,j]=e.sq2ij(sq(x,y));return e.board()[i][j];};
const live=p=>p&&!p.dead&&(p.player===0||p.player===2);
function material(e,who){let total=0;for(const row of e.board())for(const p of row)
 if(live(p))total+=(p.player===who?1:-1)*values[p.piece];return total;}
function placement(board){return board.slice().reverse().map(row=>row.map(p=>p||'1').join('').replace(/1+/g,x=>String(x.length))).join('/');}
let positions=0,mapChecks=0,plies=0,terminalExcluded=0,epFieldsOmitted=0;
const logs=[];
// Actual legal capture of every attacker/victim combination in both colors.
for(const a of 'PNBRQ')for(const v of 'PNBRQ')for(const black of [false,true]){
 const b=Array.from({length:8},()=>Array(8).fill(''));
 b[0][0]='K';b[6][7]='k';b[2][0]='P';b[5][7]='p';
 const x=a==='N'?1:a==='P'||a==='B'?2:3,y=3;
 b[y][x]=a;b[4][3]=v.toLowerCase();
 const board=black?b.slice().reverse().map(row=>row.map(p=>p===p.toUpperCase()?p.toLowerCase():p.toUpperCase())):b;
 const fen=placement(board)+' '+(black?'b':'w')+' - - 0 1 points=0,0';
 const e=create();ref.setFen(e,fen);const who=e.turn();
 const from=sq(x,black?7-y:y),to=sq(3,black?3:4), [i,j]=e.sq2ij(to);
 assert(e.getDestinations(from).includes(to),'Matrix capture not legal: '+fen);
 const victim=e.board()[i][j],earned=e.getPointValue(victim,who),before=material(e,who);
 assert(e.play({from,to}));++plies;
 if(e.gameOver()){++terminalExcluded;logs.push({fen,from,to,excluded:e.termination()});continue;}
 const delta=material(e,who)-before+100*earned;
 assert.equal(delta>0,permitted[v].includes(a),`Capture economy ${a}/${v}`);
 logs.push({fen,from,to,delta,profitable:delta>0,after:e.fen()});
}
assert.equal(terminalExcluded,0,'Matrix must exercise all 50 transactions');
for(const row of fs.readFileSync(cases,'utf8').trim().split('\n').map(JSON.parse)){
 const fields=row.fen.split(' ');if(fields[3]!=='-'){fields[3]='-';++epFieldsOmitted;}
 // EP is irrelevant to these geometric attack maps. No legal EP claim is made.
 const e=create();ref.setFen(e,fields.join(' '));const own=e.turn();
 const board=Array.from({length:8},(_,y)=>Array.from({length:8},(_,x)=>pc(e,x,y)));
 for(let vi=0;vi<types.length;++vi){let mask=0n;
  for(let y=0;y<8;++y)for(let x=0;x<8;++x){const p=board[y][x];
   if(!live(p)||p.player===own||!permitted[types[vi]].includes(p.piece))continue;
   for(let ty=0;ty<8;++ty)for(let tx=0;tx<8;++tx){const dx=tx-x,dy=ty-y;
    if(!dx&&!dy)continue;let hits=false;
    if(p.piece==='P')hits=Math.abs(dx)===1&&dy===(p.player===0?1:-1);
    else if(p.piece==='N')hits=Math.abs(dx)*Math.abs(dy)===2;
    else {const diagonal=Math.abs(dx)===Math.abs(dy),straight=!dx||!dy;
     hits=p.piece==='B'?diagonal:p.piece==='R'?straight:diagonal||straight;
     if(hits){const sx=Math.sign(dx),sy=Math.sign(dy);let ix=x+sx,iy=y+sy;
      while(ix!==tx||iy!==ty){const blocker=board[iy][ix];
       if(blocker&&!(live(blocker)&&blocker.player===own&&blocker.piece==='K')){hits=false;break;}
       ix+=sx;iy+=sy;}
     }
    }
    if(hits)mask|=1n<<BigInt(ty*8+tx);
   }
  }
  assert.equal(mask.toString(),row.maps[vi],`Client-board geometry ${row.fen} ${types[vi]}`);++mapChecks;
 }
 ++positions;
}
const report={result:'PASS',positions,map_comparisons:mapChecks,independent_capture_plies:plies,
 terminal_excluded:terminalExcluded,ep_fields_omitted_for_geometry:epFieldsOmitted,referee_sha256:ref.HASH,
 objective:'Pseudo-attack geometry with king x-rays and immediate capture profitability, not legal threat proof or SEE',logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:undefined}));
