//! Exercises the real wsm_car type-check path (asm/nucleus-win64.s's
//! wsm_car_type_error -> wsm_fail_win64) end to end, via a subprocess --
//! this can't be a plain #[test] in lib.rs because wsm_fail_win64 calls
//! std::process::exit(97), which would kill the whole in-process test
//! runner rather than just the one test. Mirrors oom_trigger.rs.
//!
//! Calls wsm_car directly (not through eval()): this crate's fixed-
//! dispatch evaluator only recognizes cond/quote as special forms and
//! treats every other head symbol as a host-primitive lookup, so there is
//! no Lisp-source expression like "(car 5)" that reaches wsm_car through
//! eval() here -- car/cdr are used internally by eval() itself on parts
//! of the AST, not exposed as a callable builtin. wsm_car's own type
//! safety still matters regardless of which caller reaches it.

fn main() {
    let non_cons_word = wsm_my_lisp_cyberpunk_dll::word::encode_fixnum(5);
    // Before the wsm_car_type_error fix, this dereferenced an arbitrary
    // shifted-integer "pointer" -- real UB, not just an unhandled case.
    // Unreached if the fix works: wsm_car's TAG_MASK check catches this
    // and wsm_fail_win64 exits the process first.
    let result = unsafe { wsm_my_lisp_cyberpunk_dll::wsm_car(core::ptr::null_mut(), non_cons_word) };
    println!("did not hit wsm_car's type check, got raw word: {result}");
}
