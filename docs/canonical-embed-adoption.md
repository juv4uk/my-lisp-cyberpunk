# Canonical my-lisp embed adoption

## Status

The upstream embedding boundary exists in
[`juv4uk/my-lisp`](https://github.com/juv4uk/my-lisp) at `5500ac2`:
`crates/my-lisp-embed` exports a versioned C ABI around one persistent,
canonical `my-lisp::Session`.

It is already proven to keep Ukrainian definitions and closures across C-ABI
calls:

```lisp
(визначити repl-перевірка 42)
repl-перевірка

(визначити подвоїти (функція (значення) (+ значення значення)))
(подвоїти 21)
```

The second request in each pair returns `42`. Parsing, Canon lookup, macros,
closures, evaluation, output and errors therefore come from `my-lisp`, not
from this repository. ABI v2 also proves a nullary host mechanism: Lisp calls
`(гравець-присутній?)`, C returns only an observation (`t` or `()`), and the
canonical session owns arity checking and error recovery. ABI v3 also adds
`my_lisp_embed_bind_host_handle`: Cyberpunk can bind `гравець` as
`#<host-handle cyberpunk.IScriptable>` without exposing its numeric token to
source or REPL output.

## Deliberate boundary

Do not load this DLL into the RED4ext adapter yet. The existing adapter has
three host capabilities with established contracts:

```text
запиши-лог          ()       -> ()
гравець-присутній?  ()       -> t / ()
клас                (handle) -> string
```

`my-lisp-embed` intentionally does not invent a callback registry or
Cyberpunk-specific value model. Its nullary bridge can already express the
first two mechanisms, and ABI v3 can bind the opaque game handle. The third
mechanism still requires one missing boundary: a typed unary host mechanism
that accepts only a declared host-handle kind and returns a UTF-8 string.
Replacing the current runtime before that call is canonical would produce two
divergent host contracts, which is worse than retaining the fixed-dispatch
runtime during the transition.

## Next atomic step

Extend the **upstream** embed contract with the next narrowly typed,
versioned capability: a unary host mechanism over an opaque host handle. It
must:

1. retain the existing registration rule: C++ supplies a named mechanism but
   never policy or evaluator;
2. accept only an opaque host handle of a declared kind, without exposing or
   accepting forgeable numeric tokens as Lisp data;
3. preserve `t` and `()` as canonical `my-lisp` values;
4. execute only on the RED4ext game thread; and
5. return a host-owned UTF-8 string or a Lisp error without destroying the
   session; and
6. prove `(клас гравець)` against the same canonical session.

Only after that proof may `adapter` load `my_lisp_embed.dll`, check
`my_lisp_embed_abi_version()`, and retire the parallel fixed-dispatch
evaluator. The loopback REPL transport already exists and can then use this
single session unchanged.
