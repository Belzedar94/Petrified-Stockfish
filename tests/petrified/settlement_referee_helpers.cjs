// SPDX-License-Identifier: GPL-3.0-or-later
// Independent legal-choice and forced-quiescence PV audit. Vendor stays external.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const ref = require('./chesscom_reference.cjs');
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

module.exports={setPosition,legal,piece,toChess,fromChess};
