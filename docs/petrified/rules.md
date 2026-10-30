# Rules contract

Authority: Fairy-Stockfish Walls commit
`5b13915ff334edb8568d76630268bdfd3f639784`, built independently. No Chess.com
points or alternative stalemate scoring is used.

- Bishops, knights, rooks and queens capturing a live enemy piece disappear too;
  their destination becomes a neutral, permanent, uncapturable wall.
- The king is FSF's pseudo-royal commoner. It cannot capture, can be adjacent to
  the other king and must avoid attacks by enemy non-king pieces.
- Pawns move forward or sideways without capturing, have the initial double
  step and capture diagonally or en passant. Pawn captures survive. Capturing
  promotions also survive as the promoted piece.
- Walls use `*` in FEN, block movement and sliding attacks, and do not count as
  live material. They contribute to position and evaluation-history keys.
- Checkmate loses; stalemate draws. Repetition and the 50-move rule follow FSF.
  Orthodox insufficient-material rules are deliberately absent; the pinned FSF
  API returns sufficient material even for the pseudo-royal commoner alone.

Two non-orthodox state details are essential: a lateral pawn move is reversible
and increments the halfmove clock; a double pawn step stores the passed square
**and destination** in the EP field, even with no available capturer. For example
`h2h4` produces `h3h4`, and `e7e5` produces `e5e6`. Only an empty eligible square
can actually be used for en passant. FEN import/export preserves this FSF state.

FSF SAN treats the sideways pawn as a custom piece, so its name is `P`, including
quiet moves. It uses normal piece disambiguation (not orthodox pawn shorthand).
It never appends `+` or `#`: the pinned SAN implementation asks for checks of
orthodox KING pieces, whereas Petrified uses COMMONER. Public `isCheck` and
`gives_check` report attacks on the commoner, including an adjacent commoner.
Legal safety excludes opposing commoners, but the initial/intermediate castling
squares include them. With no legal moves, a reported check (including adjacency)
loses; otherwise the position is stalemate. The optional 50-move claim is reported
even on a terminal board; Board.result resolves mate/stalemate before a claim.
Search uses actual non-king attacks for check extensions and evasions, and the
same terminal contract as Walls. These distinctions are covered by fixtures.

# Legacy evaluation contract

The pinned file is accepted only when its full SHA-256 matches the baseline.
Missing, truncated, incompatible, modified or trailing-byte files cannot search.
Rules and perft do not need a network. The initial backend has 49,152 features,
512 values per perspective, eight PSQT buckets and eight 1024→16→32→1 heads.
Its features are N, B, R, Q, sideways pawn, commoner and a neutral wall plane;
the black perspective flips ranks, without horizontal mirroring.

The archived trainer's pawn value 250 is a training initialization value, not
the runtime material scale. The pinned Walls code computes a white custom-pawn
value of 214 but leaves its black `PieceValue` entry at zero; commoners contribute
700. At the start position its `nonPawnMaterial` is 10714/9002. This asymmetry is
preserved for exact compatibility. Correcting it is a separate future experiment.
No SFNNv19 evaluation scaling is applied to the Legacy backend.
UCI centipawns use Walls' `100 * value / 208` conversion. An orthodox fitted WDL
model is not exposed as a Petrified probability estimate.

The unmodified Walls executable is the match opponent. A separately named build
with `tests/petrified/walls-evalraw.patch` only exposes integers and rule results
for differential testing. The patch changes no rules or evaluator.
