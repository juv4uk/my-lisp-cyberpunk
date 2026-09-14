# Normal NeuralDeck Mod Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a stable non-pausing in-game NeuralDeck that becomes the primary my-lisp REPL after the canonical embedding boundary is available.

**Architecture:** Redscript plus Codeware owns the F10 event and Ink presentation because it lives in the game's supported script/UI lifecycle. The RED4ext adapter owns only host mechanisms and the game-thread boundary for Lisp evaluation. `my-lisp` owns program semantics and behaviour; NeuralDeck never gains an evaluator.

**Tech Stack:** Cyberpunk 2077 2.31, RED4ext 1.30, Redscript, Codeware 1.20.3, C++20, Rust host runtime, canonical `my-lisp` embedding ABI.

**Spec:** `docs/neuraldeck-repl.md`

## Global Constraints

- F10 is the only visibility toggle; Tab remains a game input.
- The overlay uses `CustomPopup`, reports `IsBlocking() = false`, uses no cursor, and never invokes time dilation.
- No C++ call into compiled Redscript through `CRTTISystem::GetFunction`; it indexes native globals only.
- A submitted form is evaluated only on the game thread in the one canonical my-lisp session.
- Do not expand `host-runtime` into a second implementation of `def`, closures, or macros.
- Read-only host capabilities precede all game mutation.

---

### Task 1: Establish the correct UI ownership boundary

**Files:**
- Modify: `adapter/src/Main.cpp`
- Modify: `redscript/NeuralDeck/NeuralDeckOverlay.reds`
- Modify: `docs/neuraldeck-repl.md`
- Test: launch Cyberpunk and inspect `r6/logs/redscript_rCURRENT.log`

**Interfaces:**
- Consumes: `CallbackSystem.RegisterCallback`, `InputTarget.Key(IK_F10, IACT_Press)`, `CustomPopup.Open/Close`.
- Produces: `NeuralDeckService`, whose only user interaction is F10 and whose `OnOverlayHidden` releases the finished popup.

- [ ] **Step 1: Remove the failing C++ to compiled-Redscript RTTI call.**
- [ ] **Step 2: Add `NeuralDeckService extends ScriptableService`; register `Input/Key` for `IK_F10` and `IACT_Press`.**
- [ ] **Step 3: Create or close the non-blocking `CustomPopup` from the service.**
- [ ] **Step 4: Launch the game, press F10 twice, and capture compiler plus plugin evidence.**
- [ ] **Step 5: Commit `fix(neuraldeck): let Codeware own F10 overlay lifecycle`.**

### Task 2: Adopt the canonical persistent REPL session

**Files:**
- Modify: `host-runtime/` only to replace the temporary fixed evaluator through the upstream embedding ABI.
- Modify: `adapter/src/Main.cpp`
- Test: canonical fixture and game-thread session witness.

**Interfaces:**
- Consumes: upstream typed host capability ABI for `GameHandle` and strings.
- Produces: `evaluate(source: UTF-8) -> transcript record` using the single canonical session.

- [ ] **Step 1: Add a failing witness for `(визначити x 42)` followed by `x` in one session.**
- [ ] **Step 2: Switch the adapter to the released canonical embedding API.**
- [ ] **Step 3: Register existing read-only host primitives through the typed bridge.**
- [ ] **Step 4: Run parity and malformed-form continuation witnesses.**
- [ ] **Step 5: Commit the isolated adoption.**

### Task 3: Connect UI input and transcript

**Files:**
- Create: `redscript/NeuralDeck/NeuralDeckTranscript.reds`
- Modify: `redscript/NeuralDeck/NeuralDeckOverlay.reds`
- Modify: `adapter/src/LocalReplQueue.hpp` or its canonical replacement
- Test: bounded queue and transcript model witnesses.

**Interfaces:**
- Consumes: `submit(source)` and `pollTranscript(afterSequence)` C ABI operations.
- Produces: input field, bounded command history, and source/result/error transcript without a second session.

- [ ] **Step 1: Test a submitted UTF-8 form yields one ordered transcript record.**
- [ ] **Step 2: Add a bounded game-thread queue with explicit overload result.**
- [ ] **Step 3: Add UI text input, Enter submission, and transcript redraw.**
- [ ] **Step 4: Verify malformed source leaves the deck open and the next command succeeds.**
- [ ] **Step 5: Commit the complete vertical slice.**

### Task 4: Deep read-only engine exploration

**Files:**
- Modify: `adapter/host-operations.lisp`, `adapter/src/Main.cpp`, and capability witnesses.
- Test: live Red4ext log plus deterministic host fixture.

**Interfaces:**
- Consumes: opaque `GameHandle` values only.
- Produces: player class, position, one child handle, then one explicitly read-only method result.

- [ ] **Step 1: Close the player-class live witness.**
- [ ] **Step 2: Add world-position conversion witness before exposing values.**
- [ ] **Step 3: Add one child-walk capability.**
- [ ] **Step 4: Add one read-only RTTI invocation.**
- [ ] **Step 5: Commit each capability separately with its evidence.**

## Self-Review

- Task 1 proves the visual layer through the game-supported owner before any REPL claim.
- Task 2 removes the temporary semantic limitation before UI can expose persistent definitions.
- Task 3 makes the deck interactive without TCP becoming product infrastructure.
- Task 4 keeps deep engine access strictly read-only and opaque until its contracts are evidenced.
