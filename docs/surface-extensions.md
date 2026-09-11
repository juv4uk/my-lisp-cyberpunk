# Surface extensions · Поверхневі розширення

**Owner 2026-09-10** — equal spellings only:

```text
.my   ↔ .мій
.wsm  ↔ .всм
.lisp ↔ .лісп
```

No new semantics, no mass rename.

## Scope in my-lisp-cyberpunk

| Area | Status |
|------|--------|
| Product scripts under `scripts/` | Already use `.мій` (диспетчер, сценарії) |
| `LoadDispatchSource` | Tries `диспетчер.мій`, then `.my` twins |
| `SurfaceExt.hpp` | Resolve/read helper for twin extensions |
| DispatchWitness / Benchmarks | Paths passed as argv — any extension works if file exists |
| WSM DLL / RED4ext | **not-applicable** — no `.my` filters |
| CI globs | **not-applicable** until CI lands |

## Fixtures

- `scripts/диспетчер.мій` — primary (Cyrillic)
- Latin twin optional: `диспетчер.my` if present is also accepted

## Related

Issue #2 · same decision as ecosystem-observer#1
