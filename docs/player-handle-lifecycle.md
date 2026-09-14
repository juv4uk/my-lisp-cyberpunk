# Player handle lifecycle

`гравець` is an opaque Cyberpunk capability.  Lisp never receives a REDengine
address; it receives a session-local `GameHandle` token owned by the adapter.

```text
RTTI unavailable ── preserve existing capability

confirmed absent ── bind () and release token A
      │
      ▼
present instance A ── retain A, bind opaque token A
      │
      ├── same instance A ── keep token A
      │
      └── replacement B ── retain B, bind token B, release token A
```

The observation point is `гравець-присутній?`. `PlayerHandleEpoch::Decide`
keeps an existing handle when RTTI has not answered yet, so an unavailable
lookup is never mistaken for player absence. Confirmed absence binds `()` and
removes the table entry. A replacement gets a new monotonically allocated
token; the old entry is released before the next observation completes.

`(клас гравець)` resolves only the token currently held in
`GameHandleTable`. A released token has no handle to unwrap into a new player,
so it fails through the named host-primitive error path instead of accessing a
raw engine pointer.

## Deterministic witness

`adapter/tests/DispatchWitness.cpp` verifies the decision sequence:

```text
no retained handle + A  -> BindNew
A + A                   -> Keep
A + B                   -> BindNew
A + confirmed absence   -> BindNil
A + RTTI unavailable    -> Preserve
```

## Live evidence procedure

1. Start a save and wait until the adapter log records `bound current player as opaque token=A`.
2. Return to a state where `GetPlayer` confirms absence, then return to gameplay.
3. Record the log sequence `released absent player handle` followed by
   `bound current player as opaque token=B`.
4. Confirm `B != A`, then run the read-only class scenario against the current
   `гравець` binding.

The transcript must contain only tokens and class names. It must never record
a REDengine pointer or address.
