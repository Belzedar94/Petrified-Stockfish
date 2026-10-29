# Petrified-Stockfish

A dedicated Petrified engine derived from official Stockfish at
`ff7cee4244ec96ea488c402669e3674c0574dce6` (SFNNv19 infrastructure).
Legacy rules follow Fairy-Stockfish Walls `5b13915ff334edb8568d76630268bdfd3f639784`.
`petrifiedpoints` adds Chess.com Game of Points, including mates that lose by
score; see the [points contract](docs/petrified/chesscom-points.md).
Legacy evaluation preserves the exact Walls run4test network. The current local
points champion uses its frozen accumulators/PSQT with a trained scoreboard head.

**Development preview. Points-head-v1 passed the local three-control gate against
run4test + winning-mate-r2. Public release acceptance remains pending.**
This is not a world-leadership claim or a version 1.0 release. See
[implementation status](docs/petrified/STATUS.md),
[rule/evaluation contract](docs/petrified/rules.md), and
[local testing](docs/petrified/local-testing.md).

## Native engine

GCC/Clang and CMake 3.20+:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
```

For Windows MinGW BMI2, from `src` in the MSYS2 toolchain environment:

```sh
make -j2 build ARCH=x86-64-bmi2 COMP=mingw EXE=petrified-stockfish.exe
```

UCI and XBoard/CECP share the same rules and search. UCI advertises
`petrified` and `petrifiedpoints`. Walls are `*` in FEN. No orthodox Syzygy or orthodox
insufficient-material adjudication is used. The qualified points-head-v1 network
has SHA-256 `ab8bc1e4bb57be21eab8af061259c2bbcf065b6dfcd13c848e2a62b52739941c`.
It embeds the unchanged run4test base; the original external
`petrified_run4test_e19_l085.nnue` (SHA-256
`345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`)
also remains supported. Set the chosen network explicitly through EvalFile.

```text
uci
setoption name EvalFile value /path/to/points-head-v1.pnnue
setoption name UCI_Variant value petrifiedpoints
isready
position startpos
go depth 10
```

A missing or invalid net prevents search and a valid bench. Rules/perft work
without a net. `bench` uses a frozen Petrified corpus at depth 10, one thread,
Hash=16. Make supports `EXE`, `EVALFILE` and `GIT_SHA_FULL`; a profiling build must
log successful network verification. Network weights are not included here.

## Python

Use an isolated environment because the module name intentionally matches
`pyffish`; do not replace another project's installation.

```sh
python -m venv .venv
# Activate .venv with the command for your shell, then:
python -m pip install .
python -c "import pyffish; print(pyffish.legal_moves('petrified', pyffish.start_fen('petrified'), []))"
```

The distribution is `petrified-pyffish`; the stable-ABI extension exposes the
relevant rule, FEN, SAN, outcome and move-list functions. It uses the engine's
C++ rules. See `pyffish.pyi`. The Python binding does not include search/weights.

## JavaScript and WebAssembly

Activate an Emscripten environment and run:

```sh
python scripts/build_wasm.py
node build/wasm/engine/petrified-stockfish-nnue-node.mjs
```

The build produces Board/PGN modules in `build/wasm/cjs` and `build/wasm/esm`, plus
the complete UCI/NNUE engine in `build/wasm/engine`. Use the Node wrapper as the
engine entrypoint; it handles asynchronous search/stdin lifecycle. Supply the
external net through UCI. Browser search is outside this initial release's scope.
The lightweight Board modules can be used independently of search and NNUE.

```js
const createPetrifiedStockfish = require('./build/wasm/cjs/ffish.js');
createPetrifiedStockfish().then(api => {
  const board = new api.Board('petrified');
  console.log(board.legalMoves());
  board.delete();
});
```

Build manifests record source, compiler and artifact hashes. The original
orthodox GitHub workflows are preserved under `.github/upstream-workflows` and
are inactive. The Petrified validation workflow is manual while the project is
in its local qualification stage.

## License and references

GPLv3 or later; see [Copying.txt](Copying.txt), [AUTHORS](AUTHORS), and
[source attribution](docs/petrified/attribution.md). Original Stockfish history
is preserved. Frozen inputs and the modern trainer reference are recorded in
[baseline.json](docs/petrified/baseline.json).
