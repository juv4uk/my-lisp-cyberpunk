# Safe evaluation boundary

`host-runtime` executes Lisp inside the Cyberpunk process.  A malformed user
form must therefore become a Lisp failure; it must never become a process
failure.

## Boundary

```text
user .лісп / REPL input
  -> reader capacity preflight
  -> checked evaluator
  -> raw WSM nucleus
```

The reader rejects a complete form that cannot fit in the session arena before
it allocates.  The evaluator checks special-form arity and checks the argument
to `car`/`cdr` before it invokes their raw WSM operations.  The public C ABI
returns the resulting named failure as an `error:` result and leaves the
session usable for its next form.

The error categories follow the canonical `my-lisp` behavior:

| User form | Result category |
| --- | --- |
| `(quote)` | invalid form: exact arity |
| malformed `cond` clause | invalid form: exact clause arity |
| `(car 5)` or `(car (quote ()))` | type error: non-empty list required |
| unknown symbol | unknown symbol |
| source larger than the arena | reader error |

`wsm_car`, `wsm_cdr`, and `wsm_cons` remain raw machine-layer ABI operations.
Their direct misuse can still use the nucleus fail-closed path.  They are not
Lisp builtins and no source form may reach them without the reader/evaluator
checks above.  This preserves the WSM contract without allowing a script to
terminate Cyberpunk.

## Executable proof

`host-runtime/src/bin/reader_capacity_trigger.rs` sends every failure class
through the public session API in one subprocess.  After each failure it
evaluates `(quote жива-сесія)` in the *same* session.  The integration test
`host-runtime/tests/reader_capacity_error.rs` requires the subprocess to exit
successfully and emit `safe-eval-boundary-recovered`.

Run it locally with:

```powershell
cd host-runtime
cargo test malformed_lisp_cannot_terminate_the_checked_host_session
```

The Windows CI workflow runs the complete host-runtime suite, so a future path
from user Lisp to a raw fatal operation fails the build.
