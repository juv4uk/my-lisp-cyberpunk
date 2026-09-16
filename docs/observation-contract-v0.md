# `game-observation/1` — vanilla probe observation contract, v0

Status: implements [#28](https://github.com/juv4uk/my-lisp-cyberpunk/issues/28)
(`CP-OBSERVATION-CONTRACT-V0`), a Phase B prerequisite of
[#27](https://github.com/juv4uk/my-lisp-cyberpunk/issues/27)
(`CP-VANILLA-EPIC`).

## What this is, and is not

This is a **transport/provenance envelope**, not a game API and not Lisp
semantics. It exists so that the three bounded probes (#29 MEMORY, #30
SCREEN, #31 IN-PROCESS) can each report one observed fact from a live
Cyberpunk 2077 process in one shape, so the facts can be compared to each
other (#32) without any probe having to know the others exist.

It does not interpret what a fact *means* — that stays Lisp semantic
authority, per this repo's standing rule. It carries a `fact` name and a
raw `value`; a probe never decides policy from a `game-observation/1`
record, it only produces one.

## Record shape

```lisp
(game-observation/1
  (source memory)
  (game-fingerprint "cp2077-2.31-gog-x64")
  (fact player-health-ratio)
  (value 0.82)
  (timestamp 1758000000123)
  (validity valid)
  (provenance
    (channel-revision 1)
    (offset-table-version "2026-09-16")
    (module-base 0)
    (read-duration-us 42)))
```

## Fields

| Field | Type | Meaning |
| --- | --- | --- |
| `source` | symbol, one of `memory` \| `screen` \| `in-process` | which probe produced this record |
| `game-fingerprint` | string | opaque build identity the probe checked before reading (game version/build/store, e.g. `"cp2077-2.31-gog-x64"`) — never a raw address or pointer |
| `fact` | symbol | which semantic capability this observation answers, e.g. `player-health-ratio`. Names here are provisional identifiers for the probe layer only; the eventual canonical name is whatever `my-lisp`'s own semantic registry assigns once a fact graduates past probe stage — this repo does not mint a second semantic registry (per #28's own constraint) |
| `value` | primitive or structured Lisp data | the observed value. No raw pointers, no opaque game handles — only data a canonical `my-lisp` reader can parse standalone |
| `timestamp` | integer, milliseconds | when the read happened; monotonic within one probe run is sufficient for v0, wall-clock is acceptable |
| `validity` | symbol, one of `valid` \| `unavailable` \| `unsupported-build` \| `desync` | see below |
| `provenance` | association list, channel-specific | free-form per-`source` metadata (offset table version, AOB pattern id, screen region, capture backend, etc.) — must never change what `value` means, only how it was obtained |

## `validity`

- `valid` — `value` is a genuine, sanity-checked read for this `fact` at this `timestamp`.
- `unavailable` — the probe ran but could not read *at this moment* (e.g. game not in the expected state); no fact is asserted.
- `unsupported-build` — `game-fingerprint` does not match any offset/pattern/HUD profile this probe knows. This is the fail-closed case #29 and #30 require: **an unknown build must never fall back to an old profile and return a plausible-looking `value`.**
- `desync` — a tear/consistency guard rejected the read (e.g. two halves of a multi-word read observed different generations of the underlying state).

Only `valid` records carry a `value` that downstream code (including any
future Lisp policy) may act on. The other three exist specifically so a
silent wrong answer is impossible to produce — the shared worry that came
out of the #18 brainstorm across nearly every independent proposal.

## Cross-channel equality (#32)

Two records for the same live moment, same `fact`, different `source`,
are considered corroborating when their `value`s agree within the
fact's own declared tolerance (exact equality for discrete facts,
epsilon comparison for continuous ones like `player-health-ratio`).
`source` and `provenance` differing is expected and required; `fact` and
`value` (within tolerance) are what #32 actually compares.

## Canonical `my-lisp` roundtrip

A `game-observation/1` record is plain Lisp data: it must parse under
the canonical `my-lisp` reader with no new evaluator, no new reader
syntax, and no Cyberpunk-repo-side semantic registry. This repo verifies
that with `my-lisp`'s own reference `--oracle-check` (parse-only agent
preflight, already part of the canonical CLI) against the fixtures in
[`adapter/tests/fixtures/observation/`](../adapter/tests/fixtures/observation/),
exercised by the `observation-contract-fixtures` CTest target.

## Fixtures

- [`memory-player-health-valid.lisp`](../adapter/tests/fixtures/observation/memory-player-health-valid.lisp) — MEMORY source, `valid`.
- [`screen-player-health-valid.lisp`](../adapter/tests/fixtures/observation/screen-player-health-valid.lisp) — SCREEN source, same `fact`, corroborating `value`, different `provenance` shape (#32's positive case).
- [`memory-unsupported-build.lisp`](../adapter/tests/fixtures/observation/memory-unsupported-build.lisp) — MEMORY source, `unsupported-build`, no usable `value` (negative witness required by #28's acceptance).

## Non-goals for v0

- No wire format beyond "a `.lisp` file/string a probe writes and any
  consumer reads." TCP/pipe framing is a #29/#30/#31 concern, not this
  contract's.
- No mutation, no write-path record shape.
- No decision about which probe family wins — this contract is
  deliberately neutral so #29/#30/#31 can each be judged on the same
  evidence shape.
