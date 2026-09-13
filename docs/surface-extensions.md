# Surface extensions · Поверхневі розширення

**Owner 2026-09-10** — equal spellings only:

```text
.lisp — canonical; .my ↔ .мій are compatibility aliases
.wsm ↔ .всм remain protocol/compatibility forms
.лісп is a compatibility alias
```

No new semantics, no mass rename.

## Scope in my-lisp-cyberpunk

| Area | Status |
|------|--------|
| Product scripts under `scripts/` | Use canonical `.lisp` names |
| `LoadDispatchSource` | Tries `dispatcher.lisp` first, then compatibility aliases |
| `SurfaceExt.hpp` | Resolve/read helper for twin extensions |
| DispatchWitness / Benchmarks | Paths passed as argv — any extension works if file exists |
| WSM DLL / RED4ext | **not-applicable** — no `.my` filters |
| CI globs | **not-applicable** until CI lands |

## Fixtures

- `scripts/dispatcher.lisp` — primary (Cyrillic)
- Latin twin optional: `dispatcher.lisp` if present is also accepted

## Related

Issue #2 · same decision as ecosystem-observer#1
