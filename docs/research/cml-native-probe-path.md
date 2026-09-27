# A native, Lisp-compiled memory probe — what already exists in `cml`

Status: capability survey for the #27 vanilla epic, done by reading
`cml`'s own code and its self-authored feasibility doc directly (no
cross-session coordination spent on this) — the owner asked to reuse
existing work rather than burn limit re-deriving it.

## What this is

An answer to: could the #29 MEMORY probe eventually be one native
Lisp-compiled binary (mechanism + policy both authored in Lisp, no
PowerShell, no separate Rust/C++ host) instead of today's split
(PowerShell mechanism + `tools/observation/observation-policy.lisp`
policy, run by the interpreted `my-lisp.exe`)?

## What `cml` already has (read directly, not asked about)

`cml`'s own
[`docs/CYBERPUNK-CAPABILITY-VALUES-FEASIBILITY-2026-09-10.md`](https://github.com/juv4uk/cml/blob/main/docs/CYBERPUNK-CAPABILITY-VALUES-FEASIBILITY-2026-09-10.md)
(authored `cml-1`, 2026-09-10 — this predates this repo's own vanilla
pivot, so it wasn't written with #27 in mind, but answers the same
underlying question) confirms `src/x86_freestanding.rs` has a real,
tested mechanism:

```rust
fn platform_call_contract(func: &Ir) -> Option<(&'static str, usize, &'static str)> {
    match name.as_str() {
        "PCI-CONFIG-CAPABILITY" => Some(("pci-config-capability", 0, "wsm_pci_config_capability")),
        "PCI-CONFIG-READ16"     => Some(("pci-config-read16", 5, "wsm_pci_config_read16")),
        "MMIO-CAPABILITY"       => Some(("mmio-capability", 0, "wsm_mmio_capability")),
        "MMIO-READ32"           => Some(("mmio-read32", 2, "wsm_mmio_read32")),
        "MMIO-WRITE32"          => Some(("mmio-write32", 3, "wsm_mmio_write32")),
        _ => None,
    }
}
```

`emit_platform_call` calls the matched `wsm_*` extern import with an
opaque `RuntimeContext*` (r12) as the first argument, up to 5 further
arguments via SysV registers, and returns the raw value in `%rax` with
zero interpretation by `cml` itself.

## What would be mechanically needed for `WSM-READ-PROCESS-MEMORY`

Per `cml-1`'s own conclusion, adding a new named platform call is a
small, mechanical change **conditioned on one real prerequisite**:

1. A new match arm in `platform_call_contract`, e.g.
   `"WSM-READ-PROCESS-MEMORY" => Some(("wsm-read-process-memory", 3, "wsm_read_process_memory"))`
   (pid, address, length — 3 args, comfortably under the 5-arg limit).
2. **The real blocker**: `wsm_read_process_memory` must first be
   **ratified as part of the WSM target ABI** in `wsm-target-contract`
   and implemented in `wsm-my-lisp`/`wsm-os-lisp` — `cml` explicitly
   does not invent new runtime imports itself, the same
   ratify-then-consume discipline already applied to `TAG_BOXED`. This
   is a cross-repo ABI decision, not something this repo or `cml`
   alone can decide.
3. Arity ceiling is 5 args (fixed SysV register list), which
   `ReadProcessMemory`-shaped calls fit under.
4. This path only exists through `x86_freestanding.rs`. `cml`'s
   `c_backend.rs` has zero equivalent mechanism and doesn't consume
   `wsm-target-contract` at all — a C-backend-compiled probe would need
   a separate, symmetric mechanism built from scratch, not a reuse of
   this one.

## What this means for #29 right now

This is real, reusable ground — not a green field — but it is gated on
a cross-repo ABI ratification this repo cannot do alone, and the owner
has asked not to spend session limit coordinating that right now. The
current split architecture (PowerShell mechanism +
`observation-policy.lisp` interpreted policy, #29/#30's actual shipped
state) remains the working path. This document exists so that when
`WSM-READ-PROCESS-MEMORY` (or an equivalent) does get ratified
elsewhere, the mechanical cml-side change needed here is already
scoped and does not need to be rediscovered.

## Related

- [`docs/research/vanilla-entrypoints.md`](vanilla-entrypoints.md) (#20)
- [`docs/observation-contract-v0.md`](../observation-contract-v0.md) (#28)
- [`tools/observation/observation-policy.lisp`](../../tools/observation/observation-policy.lisp) (#29/#30 current policy layer)
