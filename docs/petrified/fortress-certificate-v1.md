# Constructive fortress certificate, v1

This is one search candidate from PR18, authorized on 2026-10-30. It changes
neither the rules nor NNUE. PR19 SEE pruning and PR20 quiet checks are excluded.
The recognizer certifies **strict points wins only**; an equal/negative bound or
unsupported geometry means unknown, and ordinary search continues. It does not
claim to recognize all fortresses, draws, or losses.

## Invariant

The defending side has only a king, pawns and at most one stationary knight.
Treat all its pawns and all neutral walls as barriers. Flood-fill the king's
component with eight-neighbour steps, ignoring every other piece. This is an
overestimate of king, bishop, rook and queen reachability. Every opposing king
and slider must be outside this component; the defending knight, if present,
must be inside it. A fatal capture of a defending pawn replaces the pawn with a
wall and therefore cannot open the barrier.

The king must have a complete 2x2 refuge in that component, occupied only by
itself and possibly the single opposing knight. The defending knight is outside
the refuge. Enumeration of all 252 king/knight placements shows that at least
one of the three other refuge squares is empty and not attacked by the knight.
The policy keeps the king in the refuge. If the invading knight captures a pawn
or the stationary knight, it disappears and leaves a wall outside the refuge.
Opposing kings do not capture or attack in this variant.

There is at most one opposing pawn. En passant, castling rights, last-rank pawns
and opposing pawns on their double-push rank are outside this first family.
Enumerate all forward and lateral quiet pawn routes, ignoring other occupancy,
but respecting walls and defending pawns. None may enter the king's component
or reach promotion. For every reachable square that attacks a defending pawn,
**each** such defending pawn must be able to capture it without connecting the
king's component to any previously external square except its vacated origin.
This checks all remaining choices, rather than depending on one interceptor
which an earlier fatal capture might remove. Replacing any subset of defending
pawns by walls leaves the barrier unchanged and only reduces reachable routes
and capture targets.

The defender intercepts immediately when the opposing pawn attacks one of its
pawns; otherwise it makes a legal king move in the refuge. A position where the
attacking pawn already threatens a capture with the attacker to move is unknown.
An actual legal interception is required on the defender's turn: a separate
check cannot be ignored. Only one attacking piece moves each turn; a quiet pawn
move cannot newly uncover a knight attack on a previously safe king. Sliders
cannot give check across the certified separation. Thus the pawn is captured
before it can collect points, or remains confined without a capture. The
defending knight need not win a tactical race: its possible loss is budgeted.

## Points and termination

Each opposing queen, rook or bishop can earn at most one point by fatally
capturing a defending pawn. The opposing knight can earn at most three points
if a defending knight remains, otherwise at most one. The opposing pawn earns
zero under the pre-emption policy. Subtract this conservative total from the
defender's current points lead. Only a strictly positive remainder is certified.
The owner's final position has a remainder of **one**: 10 - 1 - 5 - 3.

Defensive captures can only increase that remainder. Fatal enemy captures spend
their budget and disappear. Pawn advances/captures and new walls are finite;
lateral pawn moves do not reset this engine's existing fifty-move counter.
Remaining reversible play reaches repetition or fifty moves, awarding the same
bonus to both players. A favorable earlier settlement remains favorable.

## Search contract

After the existing terminal/claim checks, internal main-search and quiescence
nodes may return the certified non-mate points WDL value. Excluded-move searches
do not use the main-search certificate. At a winning defender root the engine
returns the certificate's legal policy move, respecting `searchmoves` and the
`infinite`/`ponder` stop contract. It never emits a false `claim-win` or `0000`.
The ordinary rules API still reports an ongoing position until a real ending.
No mate distance or Syzygy tablebase proof is claimed by the non-mate score.

## Verification

The test suite checks color/file symmetries, all 128 pawn-to-wall substitutions
of the supplied barrier, knight/refuge placements, actual policy continuations
against **every** legal opponent move, full-reference legality, state/key undo,
and exact NNUE values. Broken barriers, second invaders, unsafe pawns, unsupported
material and insufficient point margins must remain unknown. A separately
written coordinate/set implementation checks the exported certificates, and the
hash-pinned Chess.com client independently verifies legal moves, transitions,
points and terminal settlements. These checks accompany the written invariant;
they are not an exhaustive solution of unrestricted Petrified.

The usual Debug/Release, inherited component/reference, native-interface,
reproducible points/legacy benches and five source-CI gates precede one frozen
VSTC/STC/LTC panel against PR18. No coefficient tuning, training or automatic
successor is authorized. Bench nodes are not Elo.
