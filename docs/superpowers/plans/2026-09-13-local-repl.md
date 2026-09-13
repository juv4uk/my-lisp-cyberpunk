# Local Cyberpunk REPL Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide a loopback REPL for the existing persistent Cyberpunk Lisp session without evaluating outside the RED4ext game thread.

**Architecture:** A Winsock worker owns connections and a bounded FIFO of request objects. `DispatchRunningTick` drains that queue and is the only place that invokes `wsm_eval_string`; it writes a result back to the waiting connection. The queue and protocol stay C++ mechanism, while forms, values and effects remain Lisp and registered host capabilities.

**Tech Stack:** C++20, Winsock2, RED4ext, existing WSM C ABI, CMake/CTest.

**Spec:** [`docs/repl-contract.md`](../../repl-contract.md)

## Global Constraints

- Bind only `127.0.0.1`; never expose the listener to the network.
- The WSM ABI is called only on the `Running` callback thread.
- Input is UTF-8, one form per newline, ≤16 KiB; queue capacity is 32.
- Do not create a second evaluator, new Lisp syntax, fake callbacks or a generic plugin framework.
- A host error is text returned to the client; it must not terminate Cyberpunk.

---

### Task 1: Bounded request queue

**Files:**
- Create: `adapter/src/LocalReplQueue.hpp`
- Create: `adapter/tests/LocalReplQueueWitness.cpp`
- Modify: `adapter/src/CMakeLists.txt`

**Interfaces:**
- Produces `local_repl::RequestQueue`, with `bool Push(std::string, Reply)`, `std::optional<Request> Pop()`, `constexpr kMaxRequestBytes = 16384`, and `constexpr kMaxPendingRequests = 32`.
- `Reply` receives exactly one UTF-8 response string from the game thread.

- [x] **Step 1: Write a failing FIFO/bound CTest witness**

```cpp
int main() {
  local_repl::RequestQueue queue;
  if (queue.Push(std::string(local_repl::kMaxRequestBytes + 1, 'x'), reply)) return 1;
  for (size_t i = 0; i < local_repl::kMaxPendingRequests; ++i) if (!queue.Push("()", reply)) return 2;
  return queue.Push("()", reply) ? 3 : 0;
}
```

- [x] **Step 2: Run the CTest witness and confirm it fails because the queue is absent.**
- [x] **Step 3: Implement the mutex-protected FIFO.** `Push` copies the request and reply closure; `Pop` moves the oldest request. It performs no socket or WSM work.
- [x] **Step 4: Run CTest and confirm queue tests pass.**
- [x] **Step 5: Commit** `test(adapter): cover bounded local REPL queue`.

### Task 2: Loopback listener and game-thread drain

**Files:**
- Create: `adapter/src/LocalReplServer.hpp`
- Create: `adapter/src/LocalReplServer.cpp`
- Modify: `adapter/src/Main.cpp`
- Modify: `adapter/src/CMakeLists.txt`

**Interfaces:**
- Consumes `local_repl::RequestQueue`.
- Produces `LocalReplServer::Start(RequestQueue&)`, `Stop()`, and `Drain(Session*, WsmEvalStringFn, WsmFreeStringFn)`.

- [ ] **Step 1: Add a failing loopback witness.** It opens `127.0.0.1`, submits two newline forms, and asserts the game-thread drain invokes the supplied fake evaluator once per form in FIFO order.
- [ ] **Step 2: Run it and confirm it fails before the listener exists.**
- [ ] **Step 3: Implement Winsock worker.** It accepts local clients, accumulates bytes until newline, enforces 16 KiB, and enqueues each form. The reply closure writes one response plus newline; full queue and oversize requests receive explicit errors.
- [ ] **Step 4: Integrate lifecycle.** Start after runtime/session initialization; call `Drain` from `DispatchRunningTick`; stop and join before freeing the session on unload.
- [ ] **Step 5: Link `ws2_32`, build Release adapter, run CTest and the witness.**
- [ ] **Step 6: Commit** `feat(adapter): add game-thread local REPL transport`.

### Task 3: Persistent-session proof and operator runbook

**Files:**
- Create: `adapter/tests/LocalReplWitness.cpp`
- Modify: `adapter/src/CMakeLists.txt`
- Modify: `docs/install-red4ext-plugin.md`
- Modify: `docs/vertical-slice.md`
- Modify: `tasks.my`

**Interfaces:**
- The witness loads `wsm_my_lisp_cyberpunk_dll.dll`, starts the local listener, and uses one connection to evaluate a definition and a later lookup.

- [ ] **Step 1: Write the witness expectation.** Send `(визначити repl-перевірка 42)` then `repl-перевірка`; expect `42` on the second reply. Send malformed input then `repl-перевірка` again; expect an error then `42`.
- [ ] **Step 2: Run it and confirm it fails until Task 2 is integrated.**
- [ ] **Step 3: Implement only enough fixture capability/bootstrap to make the proof pass.** Do not add a second session.
- [ ] **Step 4: Add a runbook.** State listener address, sample PowerShell client, expected log lines, and that live evidence is required before marking the task done.
- [ ] **Step 5: Run full Rust tests with `--test-threads=1`, CTest, Release build, queue/listener witness, and dispatch witness.**
- [ ] **Step 6: Commit** `test(adapter): prove persistent local REPL session`.
