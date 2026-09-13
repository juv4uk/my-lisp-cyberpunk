# Canonical my-lisp embed adoption

## Status

The first upstream building block exists in
[`juv4uk/my-lisp`](https://github.com/juv4uk/my-lisp) at `1ec1843c`:
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
from this repository.

## Deliberate boundary

Do not load this DLL into the RED4ext adapter yet. The existing adapter has
three host capabilities with established contracts:

```text
запиши-лог          ()       -> ()
гравець-присутній?  ()       -> t / ()
клас                (handle) -> string
```

`my-lisp-embed` intentionally does not invent a callback registry or
Cyberpunk-specific value model. Replacing the current runtime before those
capabilities have a canonical bridge would produce two divergent host
contracts, which is worse than retaining the fixed-dispatch runtime during
the transition.

## Next atomic step

Extend the **upstream** embed contract with a narrowly typed, versioned host
capability boundary. It must:

1. register a named mechanism without giving C++ any policy or evaluator;
2. move evaluated Lisp values and opaque host handles across the ABI without
   exposing forgeable numeric tokens as Lisp data;
3. preserve `t` and `()` as canonical `my-lisp` values;
4. execute only on the RED4ext game thread; and
5. prove the three operations above against the same canonical session.

Only after that proof may `adapter` load `my_lisp_embed.dll`, check
`my_lisp_embed_abi_version()`, and retire the parallel fixed-dispatch
evaluator. The loopback REPL transport already exists and can then use this
single session unchanged.
