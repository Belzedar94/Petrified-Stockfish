// SPDX-License-Identifier: GPL-3.0-or-later
// Independent child safety guards; local exchange values are audited separately.
const fs=require('node:fs'), assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [casesFile,bundle,modules,output]=process.argv.slice(2);
const create=ref.load(bundle,modules);
const toChess = s => String.fromCharCode(s.charCodeAt(0)+3)+(Number(s.slice(1))+3);
const fromChess = s => String.fromCharCode(s.charCodeAt(0)-3)+(Number(s.slice(1))-3);
let reconstructedEp = 0;
function piece(e, sq) { const [x,y] = e.sq2ij(sq); return e.board()[x][y]; }
function importFen(e,fen) {
  ref.setFen(e,fen);
  // Seed repetition with the client's canonical wall encoding, not raw 4X.
  e.position(e.makeFen());
}
function setPosition(e, fen) {
  const fields = fen.split(' ');
  const board = fields[0].split('/').map(rank => [...rank].flatMap(pc =>
    /[1-8]/.test(pc) ? Array(Number(pc)).fill('') : [pc]));
  if (fields[3] === '-') importFen(e, fen);
  else {
    // Import the position before the double push and let the independent client
    // create its own en-passant state by playing that legal move.
    const previous = board.map(rank => [...rank]);
    const whiteToMove = fields[1] === 'w';
    const targets=fields[3].match(/../g);
    const passed=targets.filter(s=>s[1]===(whiteToMove?'6':'3'));
    assert.equal(passed.length,1,'One passed square expected');
    const file=passed[0].charCodeAt(0)-97, rank=Number(passed[0][1]);
    assert(targets.every(s=>s[0]===passed[0][0] && [rank,rank+(whiteToMove?-1:1)].includes(Number(s[1]))));
    const destination = rank + (whiteToMove ? -1 : 1);
    const origin = rank + (whiteToMove ? 1 : -1);
    assert.equal(rank, whiteToMove ? 6 : 3);
    assert.equal(previous[8-destination][file], whiteToMove ? 'p' : 'P');
    assert.equal(previous[8-origin][file], '');
    previous[8-origin][file] = previous[8-destination][file];
    previous[8-destination][file] = '';
    const placement = previous.map(row => row.map(pc => pc || '1').join('')
      .replace(/1+/g, empty => String(empty.length))).join('/');
    const pre = [...fields];
    pre[0] = placement; pre[1] = whiteToMove ? 'b' : 'w'; pre[3] = '-'; pre[4] = '0';
    importFen(e, pre.join(' '));
    const letter = fields[3][0];
    ref.play(e, letter + origin + letter + destination);
    assert(!e.gameOver(), 'Double push unexpectedly settled the fixture');
    ++reconstructedEp;
  }
  assert.equal(e.turn(), fields[1] === 'w' ? 0 : 2);
  const scores = fields[6].slice(7).split(',').map(Number);
  assert.equal(e.points()[0], scores[0]); assert.equal(e.points()[2], scores[1]);
  for (let r=0;r<8;++r) for (let f=0;f<8;++f) {
    const expected = board[r][f];
    const actual = piece(e, toChess(String.fromCharCode(97+f)+(8-r)));
    if (!expected) assert(!actual, 'Unexpected piece in imported fixture');
    else if (expected === '*') assert(actual && actual.player === 4 && actual.piece === 'X', 'Missing neutral wall');
    else {
      assert(actual && !actual.dead, 'Missing live piece');
      assert.equal(actual.piece, expected.toUpperCase());
      assert.equal(actual.player, expected === expected.toUpperCase() ? 0 : 2);
    }
  }
}

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

const rows=fs.readFileSync(casesFile,'utf8').trim().split('\n').map(JSON.parse);
const logs=[]; let terminal=0,pruned=0,forced=0,plies=0;
for (const [index,row] of rows.entries()) {
  const e=create();
  if (row.history) for (const m of row.history) { assert(!e.gameOver()); ref.play(e,m); }
  else setPosition(e,row.fen);
  assert(!e.gameOver(),'Parent unexpectedly terminal');
  assert.deepEqual(legal(e),[...row.parent_legal].sort());
  const before=e.fen(); ref.play(e,row.move); ++plies;
  const status=ref.status(e);
  assert.equal(status.terminal,!!row.child_terminal,'Terminal guard '+index);
  assert.equal(status.result,row.child_result,'Settlement result '+index);
  const live=[0,0];
  for(const file of e.board()) for(const pc of file)
    if(pc && !pc.dead && [0,2].includes(pc.player)) ++live[pc.player/2];
  const bare=live.some(n=>n<=1);
  // Terminal liquidation can turn the stalemated player into dead pieces.
  // Its pre-liquidation material guard is not comparable to that final board.
  if(!status.terminal) assert.equal(bare,!!row.child_bare,'Bare king guard '+index);
  else assert(!row.prune,'Terminal branch must be searched');
  let choices=null,claim=null;
  if(!status.terminal) {
    choices=legal(e); assert.deepEqual(choices,[...row.child_legal].sort());
    const copy=create(); copy.position(e.fen()); const player=copy.turn();
    copy.resign(); const resigned=ref.status(copy);
    assert(resigned.terminal,'Independent resignation did not settle');
    claim=resigned.result==='1/2-1/2' || resigned.result===(player===0?'1-0':'0-1');
    assert.equal(claim,!!row.child_claim,'Resignation guard '+index);
    forced+=choices.length===1;
  } else ++terminal;
  if(row.prune) {
    assert(row.value<0 && row.candidate && !status.terminal && !bare && !claim && choices.length>1);
    ++pruned;
  }
  logs.push({index,fen:row.fen,move:row.move,before,after:e.fen(),status,live,choices,claim,pruned:!!row.prune});
}
assert(plies>100 && pruned>0 && terminal>0 && forced>0);
const report={result:'PASS',cases:rows.length,independent_guard_plies:plies,pruned_cases:pruned,
  terminal_children_protected:terminal,forced_children_protected:forced,
  terminal_material_comparison:'Excluded only after settlement changes dead-piece state; terminal/result and non-pruning are checked for every case',
  referee_sha256:ref.HASH,reconstructed_ep:reconstructedEp,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,logs:undefined}));
