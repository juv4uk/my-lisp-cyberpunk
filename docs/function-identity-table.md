# Function/operation identity table (ECO-CANON-1, first pass)

Closes the audit part of #3 for this repo's scope: host-runtime/adapter
function identities, hard-coded names/IDs, FFI tables, generated bindings,
copied semantic facts.

**Rule this table enforces**: language meaning/identity comes from
`my-lisp`'s Canon (`semantic-registry.wsm`, numeric ID). Target
representation (tagged-word ABI) comes from the ratified
`wsm-target-contract`. Game/host operations that have no Canon ID are
explicitly local-only, with an explicit owner — never silently treated as
if they had language-level identity.

Display/API label is not identity. Generated projections (build.rs reading
`semantic-registry.wsm` at build time) replace hand-copied spelling lists
wherever this repo consumes Canon forms.

| Canonical/local identity | Semantic ID | Formal action | Surfaces | Host/FFI/game projection | Authority owner | Status | Parity/evidence witness |
|---|---|---|---|---|---|---|---|
| `quote` | `0001` | Canon — return operand unevaluated | `quote` (en), `як-є` (uk), `svarūpa` (sa), `'` (symbolic) | `host-runtime/build.rs` generates `canon_spellings.rs` from `semantic-registry.wsm` at build time; `eval.rs` checks membership, not hardcoded `matches!` | `my-lisp` (semantics) | confirmed | `host-runtime` fail-closed build (missing/empty registry entry aborts build); `host-runtime/tests/my_lisp_fixture_parity.rs` |
| `cond` | `0007` | Canon — conditional branching, short-circuit | `cond` (en), `за-умовою` (uk), `anukrama` (sa), `?:` (symbolic) | same generated projection as `quote` | `my-lisp` (semantics) | confirmed | same |
| `String` value kind | n/a (Canon data type, not a form) | Canon — immutable text value | language-level, no host-specific spelling | `host-runtime`'s `BoxedValue::Str`, `Tag::Boxed=7` (ratified `wsm-target-contract` v3) | `my-lisp` (semantics), `wsm-target-contract` (ABI tag) | confirmed | `wsm-target-contract` migration note, `host-runtime` tests |
| `Rational` value kind | n/a (Canon data type) | Canon — exact fraction, reduced at construction | language-level | `host-runtime`'s `BoxedValue::Rational(i64,i64)`, same `Tag::Boxed` slot as String (ratified `wsm-target-contract` v4) | `my-lisp` (semantics), `wsm-target-contract` (ABI tag) | confirmed | `host-runtime/tests/my_lisp_fixture_parity.rs` §5 (5/336, 10/20→1/2); **pending**: which live RTTI call (`GetWorldPosition`) actually produces the source value — see `docs/deep-penetration-roadmap-2026-09-10.md` |
| `GameHandle` value kind | n/a — **explicitly not Canon** | host-only — opaque token into this repo's own `GameHandleTable`, never a raw engine pointer across the FFI boundary | no language-level spelling; C++-side only | `host-runtime`'s `BoxedValue::GameHandle`, `wsm_wrap_game_handle`/`wsm_unwrap_game_handle`; `adapter/src/GameHandleTable.{hpp,cpp}` owns the real `RED4ext::Handle<T>` | **this repo** (`my-lisp-cyberpunk`), by explicit design decision (`docs/deep-penetration-roadmap-2026-09-10.md` problem 2) | confirmed, live | `docs/vertical-slice.md` transcript (`token=1`); `GameHandleTable` unit-level review (2026-09-11) |
| `запиши-лог` | n/a — **explicitly not Canon** | host-only capability — write one log line, return `()` | Ukrainian only, no English/Sanskrit surface (host-invented name, not a language primitive) | registered via `wsm_register_primitive`; C++ `LogPrimitive` in `adapter/src/Main.cpp` | **this repo** | confirmed, live | `docs/vertical-slice.md` transcript |
| `гравець-присутній?` | n/a — **explicitly not Canon** | host-only capability — query player-instance presence via RTTI, return `t`/`()`; side effect: binds `гравець` on first `t` | Ukrainian only | `PlayerPresentPrimitive` in `adapter/src/Main.cpp`, polls every frame via `DispatchRunningTick` | **this repo** | confirmed, live | `docs/vertical-slice.md` transcript (`present=false→true` observed) |

## What this audit found (no drift)

- No hand-copied Canon spelling list remains in this repo's own code —
  `host-runtime/build.rs` already generates from the registry (migrated
  from `wsm-my-lisp`'s `55b3156`).
- Host-only capabilities (`запиши-лог`, `гравець-присутній?`, `GameHandle`)
  correctly have **no** semantic ID and are not claimed as Canon — this
  table makes that explicit instead of leaving it implicit.
- The one open item is not an identity/drift bug: which RTTI call produces
  player-position data (`FixedPoint` vs `Vector4`) is a live-runtime
  question, not a semantic-authority question — tracked in the
  deep-penetration roadmap, not here.

## Not done in this pass

A CI-enforced "unknown ID fails closed" test specific to this table (the
acceptance criterion's "parity/mutation tests catch drift") — the existing
`host-runtime/build.rs` already fails closed for `quote`/`cond` at build
time; a repo-wide lint that would catch a *new* hardcoded Canon spelling
being added in the future is not implemented here. Left as a follow-up,
not silently skipped.
