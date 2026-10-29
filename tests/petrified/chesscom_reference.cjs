// Audit-only adapter for the pinned public Chess.com client. No vendor source
// is distributed here. The caller supplies the downloaded, hash-checked bundle.
const fs = require('node:fs');
const crypto = require('node:crypto');
const vm = require('node:vm');
const path = require('node:path');
const HASH = '3e6c98bb6f95b124b3949778c4360603976a39c898424aa5d9ae7f441352582b';

function load(bundle, modules) {
  const source = fs.readFileSync(bundle, 'utf8');
  if (crypto.createHash('sha256').update(source).digest('hex') !== HASH)
    throw Error('Unrecognized Chess.com reference bundle');
  const parser = require(path.resolve(modules, '@babel/parser'));
  const traverse = require(path.resolve(modules, '@babel/traverse')).default;
  const ast = parser.parse(source, {sourceType: 'unambiguous'});
  let root;
  traverse(ast, {Program(p) { root = p; p.stop(); }});
  const selected = new Map();
  function visit(name) {
    if (selected.has(name)) return;
    const binding = root.scope.bindings[name];
    if (!binding) return;
    selected.set(name, binding.path);
    binding.path.traverse({ReferencedIdentifier(p) {
      const dependency = p.scope.getBinding(p.node.name);
      if (dependency && dependency.scope === root.scope) visit(p.node.name);
    }});
  }
  visit('zl');
  const code = [...selected.values()].sort((a,b) => a.node.start-b.node.start)
    .map(p => (p.isVariableDeclarator() ? 'var ' : '') + source.slice(p.node.start,p.node.end) + ';').join('\n');
  const context = {console: {log(...s) { process.stderr.write(s.join(' ')+'\n'); }, trace() {}}};
  vm.createContext(context);
  vm.runInContext(code+';this.createOracle=zl;', context, {timeout: 5000});
  return () => {
    const engine = context.createOracle();
    engine.reset();
    engine.setRuleVariants({q: {showPoints:true, whiteBlack:true, twoPlayer:true, startFen:engine.TWOPLAYER_FEN},
      ruleVariants: {deadWall:true, enPassant:true, fatalCapture:true, sideways:true}, applyStartFen:true});
    engine.position(engine.TWOPLAYER_FEN);
    return engine;
  };
}

const toChess = s => String.fromCharCode(s.charCodeAt(0)+3) + (Number(s.slice(1))+3);
const fromChess = s => String.fromCharCode(s.charCodeAt(0)-3) + (Number(s.slice(1))-3);
function play(engine, uci) {
  const move = {from:toChess(uci.slice(0,2)), to:toChess(uci.slice(2,4))};
  if (uci.length === 5) move.promoteTo = uci[4].toUpperCase();
  if (!engine.play(move)) throw Error('Chess.com rejected move '+uci);
}
function status(engine) {
  const terminal = engine.gameOver();
  const winner = engine.getWinner();
  return {points: [engine.points()[0], engine.points()[2]], terminal,
    result: !terminal ? '*' : winner === 0 ? '1-0' : winner === 2 ? '0-1' : '1/2-1/2',
    reason: engine.termination(), side: engine.turn() === 0 ? 'w' : 'b',
    canClaimWin: !terminal && !!engine.canClaim(), fen4:engine.fen()};
}
// Small fixture converter. Full match histories always start from 2PC.
function setFen(engine, fen) {
  const [board,side,rights,ep,half,,scores]=fen.split(' ');
  if(ep!=='-') throw Error('Fixture converter does not support en passant');
  const rows=Array.from({length:14},()=>Array(14).fill('x'));
  board.split('/').forEach((rank,r)=>{let file=3;for(const piece of rank){
    if(/[1-8]/.test(piece)) for(let i=0;i<Number(piece);i++) rows[3+r][file++]='';
    else rows[3+r][file++]=piece==='*'?'4X':(piece===piece.toUpperCase()?'0':'2')+piece.toUpperCase();
  }});
  const placement=rows.map(row=>{const out=[];let empty=0;for(const pc of row){
    if(!pc)empty++;else{if(empty)out.push(empty);empty=0;out.push(pc);}
  }if(empty)out.push(empty);return out.join(',');}).join('/');
  const [w,b]=scores.slice(7).split(',').map(Number);
  const fen4=[placement,side==='w'?0:2,'[false,true,false,true]',
    JSON.stringify([rights.includes('K'),false,rights.includes('k'),false]),
    JSON.stringify([rights.includes('Q'),false,rights.includes('q'),false]),
    JSON.stringify([w,0,b,0]),half,JSON.stringify({pawnBaseRank:5,wb:true,dim:'8x8'})].join('-');
  engine.position(fen4);
}
module.exports = {load, play, status, setFen, HASH};

if (require.main === module) {
  const create = load(process.argv[2], process.argv[3]);
  let engine = create();
  require('node:readline').createInterface({input:process.stdin}).on('line', line => {
    try {
      const request = JSON.parse(line);
      if (request.op === 'reset') {
        engine = create();
        for (const move of request.moves || []) play(engine, move);
      }
      else if (request.op === 'move') play(engine, request.move);
      else if (request.op === 'resign') engine.resign();
      else if (request.op === 'timeout') engine.timeout();
      else if (request.op !== 'status') throw Error('Unknown referee operation');
      process.stdout.write(JSON.stringify(status(engine))+'\n');
    } catch (error) { process.stdout.write(JSON.stringify({error:error.message})+'\n'); }
  });
}
