import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
import path from 'node:path';
const [root, fixturesPath] = process.argv.slice(2);
const fixtures = JSON.parse(readFileSync(fixturesPath,'utf8'));
const require = createRequire(import.meta.url);
for (const mode of ['cjs','esm']) {
  const filename = path.resolve(root,mode,mode==='cjs'?'ffish.js':'ffish.mjs');
  const factory = mode==='cjs'?require(filename):(await import(pathToFileURL(filename))).default;
  const api = await factory();
  assert.equal(api.variants(),'petrified petrifiedpoints');
  for (const f of fixtures) {
    const b = new api.Board('petrified',f.fen);
    try {
      assert.equal(b.fen(),f.normalized);
      assert.deepEqual(b.legalMoves().trim().split(/\s+/).filter(Boolean).sort(),Object.keys(f.san).sort());
      assert.equal(b.isCheck(),f.check,f.fen);
      assert.equal(b.isGameOver(),f.terminal,f.fen);
      const side=f.fen.split(' ')[1];
      assert.equal(b.result(),!f.terminal?'*':f.value===0?'1/2-1/2':side==='w'?'0-1':'1-0');
      assert.equal(b.isInsufficientMaterial(),false);
      for (const [move,san] of Object.entries(f.san)) {
        assert.equal(b.sanMove(move),san,`${f.fen} ${move}`);
        assert.ok(b.push(move));
        b.pop();
        assert.equal(b.fen(),f.normalized);
        assert.ok(b.pushSan(san));
        b.pop();
      }
    } finally { b.delete(); }
  }
  const b=new api.Board();
  assert.equal(b.push('e2e5'),false);
  assert.throws(()=>b.setFen('invalid'));
  assert.equal(b.fen(),api.startingFen('petrified'));
  assert.throws(()=>new api.Board('chess'));
  const g=api.readGamePGN('[Variant "petrified"]\n\n1. Pe4 Pe5 2. Nf3 Nc6 *');
  assert.equal(g.mainlineMoves(),'e2e4 e7e5 g1f3 b8c6');
  g.delete(); b.delete();
  assert.equal(api.debugLiveBoards(),0);
  const game=JSON.parse(readFileSync(new URL('./fixtures/chesscom-111037628.json', import.meta.url),'utf8'));
  const points=new api.Board('petrifiedpoints');
  for (const row of game.records) {
    assert.ok(points.push(row.uci));
    assert.deepEqual(JSON.parse(points.pointsStatus()).final,row.points);
  }
  assert.equal(points.result(),'1-0');
  assert.equal(points.isGameOver(),true);
  for (const row of game.records) points.pop();
  assert.equal(points.fen(),api.startingFen('petrifiedpoints'));
  assert.equal(points.endResult('resign',true),'0-1');
  points.delete();
  const winning=JSON.parse(readFileSync(new URL('./fixtures/chesscom-winning-mate.json', import.meta.url),'utf8'));
  const mating=new api.Board('petrifiedpoints');
  try {
    for (const [ply,move] of winning.moves.entries()) {
      assert.ok(mating.push(move));
      assert.deepEqual(JSON.parse(mating.pointsStatus()).final,winning.points_after_each_ply[ply]);
    }
    assert.equal(mating.fen(),winning.fen);
    assert.equal(mating.sanMove(winning.move),winning.san);
    assert.ok(mating.push(winning.move));
    assert.equal(mating.result(),'1-0');
    assert.deepEqual(JSON.parse(mating.pointsStatus()).final,[40,4]);
    for (const testcase of winning.thresholds) {
      mating.setFen(winning.fen.replace('points=20,4','points='+testcase.earned.join(',')));
      assert.ok(mating.push(winning.move));
      assert.equal(mating.result(),testcase.result);
      assert.deepEqual(JSON.parse(mating.pointsStatus()).final,testcase.final);
    }
  } finally { mating.delete(); }
  const pins=JSON.parse(readFileSync(new URL('./fixtures/chesscom-pins.json', import.meta.url),'utf8'));
  for (const fixture of pins.cases) {
    const pinned=new api.Board('petrifiedpoints',fixture.fen);
    try {
      assert.deepEqual(pinned.legalMoves().trim().split(/\s+/).filter(Boolean).sort(),fixture.legal);
      if (fixture === pins.cases[0]) {
        assert.equal(pinned.push('f7e7'),false);
        assert.equal(pinned.push('f7g7'),false);
        assert.equal(pinned.fen(),fixture.fen);
      }
    } finally { pinned.delete(); }
  }
  assert.equal(api.debugLiveBoards(),0);
  console.log(`${mode}: ${fixtures.length} positions, SAN/push/pop/results/errors/PGN/lifecycle passed`);
}
