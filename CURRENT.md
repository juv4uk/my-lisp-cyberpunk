# CURRENT — one entry point

Start here. If a doc anywhere in this repo disagrees with something
listed below, this file and the docs it points to win; anything in
`docs/archive/` is historical, not authoritative.

## Architecture / role

- [`README.md`](README.md) — repo role, dependency graph (my-lisp / host-runtime / adapter / cml).
- [`host-runtime/MIGRATION.md`](host-runtime/MIGRATION.md), [`host-runtime/VENDOR.md`](host-runtime/VENDOR.md) — Rust host embed, migrated from `wsm-my-lisp/dll` (Phase C/D both done).
- [`adapter/README.md`](adapter/README.md) — RED4ext host adapter.
- [`docs/function-identity-table.md`](docs/function-identity-table.md) — ECO-CANON-1 audit: which function/value identities are Canon (my-lisp-owned) vs host-only (this repo-owned), no drift found.

## Current status / evidence

- [`docs/vertical-slice.md`](docs/vertical-slice.md) — what's proven live in-game, expected log lines.
- [`docs/benchmarks.md`](docs/benchmarks.md) — fixed-dispatch performance evidence.
- [`docs/surface-extensions.md`](docs/surface-extensions.md) — `.my`/`.мій` equal-surface loading.
- [`docs/deep-penetration-roadmap-2026-09-10.md`](docs/deep-penetration-roadmap-2026-09-10.md) — owner's roadmap analysis; its own "Статус після трьох фіксів" table is kept current, task ladder now lives in `tasks.my`.

## Tasks

- [`tasks.my`](tasks.my) — the actual task list with `done`/`nil` status, priorities, dependencies. This is the operational source of truth for what's next, not any prose doc.

## Install / operate

- [`docs/install-red4ext-plugin.md`](docs/install-red4ext-plugin.md) — build → copy → verify checklist for a local game install.

## Archive

- [`docs/archive/`](docs/archive/) — superseded/completed docs, kept for provenance only. See its own README for what superseded what.
