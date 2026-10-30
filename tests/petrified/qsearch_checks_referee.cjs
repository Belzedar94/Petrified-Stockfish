// SPDX-License-Identifier: GPL-3.0-or-later
// Independent full quiet-check lists and point settlements; vendor stays external.
const fs=require('node:fs'),assert=require('node:assert/strict');
const ref=require('./chesscom_reference.cjs');
const [casesFile,bundle,modules,output,searchFile]=process.argv.slice(2);
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
let checked=0,plies=0,terminal=0,quietMates=0,castles=0,promotions=0,pvPlies=0;
const logs=[],mateResults=new Set();
for(const [index,row] of rows.entries()) {
  const e=create();setPosition(e,row.fen);
  assert(!e.gameOver(),'Unexpected terminal parent');
  assert.equal(e.inCheck(),row.in_check);
  const moves=legal(e);assert.deepEqual(moves,[...row.legal].sort());
  const before=e.fen(),player=e.turn(),actual=[],children=[];
  if(!row.in_check) for(const m of moves) {
    const from=toChess(m.slice(0,2)),to=toChess(m.slice(2,4));
    if(e.isCapture(from,to)||m.endsWith('q')) continue;
    assert(e.play({from,to,...(m.length===5?{promoteTo:m[4].toUpperCase()}:{})}));++plies;
    const status=ref.status(e);
    // Settlement removes the mated player's live king and clears inCheck.
    // Use the independent termination reason for that terminal transition;
    // the winner still comes from points, not from the side that gave mate.
    const mate=status.terminal && status.reason.startsWith('Checkmate');
    const check=e.inCheck(player^2) || mate;
    if(check) {
      actual.push(m);
      const expected=row.children.find(c=>c.move===m);assert(expected,'Unexpected independent check '+m);
      assert.equal(status.terminal,expected.terminal,'Terminal '+index);
      assert.equal(status.result,expected.result,'Points result '+index);
      assert.equal(mate,expected.mate,'Mate reason '+index);
      if(expected.mate) {
        assert(status.terminal,'Checkmate not settled');++quietMates;mateResults.add(status.result);
      }
      terminal+=status.terminal;promotions+=m.length===5;
      castles+=(['e1g1','e1c1','e8g8','e8c8'].includes(m) && pieceAfterBefore(from));
      children.push({move:m,status,check});
    }
    e.position(before);
    function pieceAfterBefore(sq) { const c=create();c.position(before);return piece(c,sq).piece==='K'; }
  }
  assert.deepEqual(actual.sort(),[...row.quiet_checks].sort(),'Quiet-check list '+index+' '+row.fen);
  logs.push({index,fen:row.fen,quiet_checks:actual,children});++checked;
}
if(searchFile) for(const row of JSON.parse(fs.readFileSync(searchFile,'utf8')).cases) {
  const e=create();setPosition(e,row.fen);
  assert.equal(legal(e).length,1,'Root corridor must be forced');
  for(const [i,m] of row.pv.entries()) {
    assert(!e.gameOver());assert(legal(e).includes(m));
    if(i===1) {
      assert(legal(e).length>1,'Quiet mate must be voluntary');
      assert(!e.isCapture(toChess(m.slice(0,2)),toChess(m.slice(2,4))));
    }
    ref.play(e,m);++pvPlies;
  }
  assert(ref.status(e).terminal);assert.equal(ref.status(e).result,row.result);
}
assert(checked>100 && plies>100 && quietMates>=6 && mateResults.size===3 && castles>0 && promotions>0);
const report={result:'PASS',positions:checked,independent_quiet_plies:plies,quiet_mates:quietMates,
  mate_results:[...mateResults],terminal_children:terminal,castling_checks:castles,underpromotion_checks:promotions,
  independent_search_plies:pvPlies,reconstructed_ep:reconstructedEp,referee_sha256:ref.HASH,logs};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,logs:undefined}));
