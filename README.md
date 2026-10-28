# Petrified-Stockfish

A dedicated Petrified engine derived from official Stockfish at
`ff7cee4244ec96ea488c402669e3674c0574dce6` (SFNNv19 infrastructure).
Rules follow Fairy-Stockfish Walls `5b13915ff334edb8568d76630268bdfd3f639784`;
the initial evaluator is the exact Legacy Walls run4test network.

**Development preview. Strength qualification and release acceptance are pending.**
This is not a claim of superiority or a version 1.0 release. See
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

UCI and XBoard/CECP share the same rules and search. UCI advertises only
`UCI_Variant=petrified`. Walls are `*` in FEN. No orthodox Syzygy or orthodox
insufficient-material adjudication is used. Searching requires the external file
`petrified_run4test_e19_l085.nnue`, SHA-256
`345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`.

```text
uci
setoption name EvalFile value /path/to/petrified_run4test_e19_l085.nnue
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
