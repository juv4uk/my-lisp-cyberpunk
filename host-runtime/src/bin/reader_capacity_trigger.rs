use std::ffi::{CStr, CString};

use wsm_my_lisp_cyberpunk_dll::ffi::{wsm_eval_string, wsm_free_string, wsm_session_free, wsm_session_init};

fn evaluate(session: *mut wsm_my_lisp_cyberpunk_dll::ffi::Session, source: &str) -> String {
    let source = CString::new(source).expect("source has no NUL");
    let result = unsafe { wsm_eval_string(session, source.as_ptr()) };
    let text = unsafe { CStr::from_ptr(result) }.to_str().expect("result is UTF-8").to_string();
    unsafe { wsm_free_string(result) };
    text
}

fn main() {
    let session = wsm_session_init();

    for (source, expected_prefix) in [
        ("(quote)", "error: invalid form: quote takes exactly one argument"),
        ("(cond (t 1 2))", "error: invalid form: cond clause takes exactly two forms"),
        ("(car 5)", "error: type error: car expects a non-empty list"),
        ("(перше (quote ()))", "error: type error: car expects a non-empty list"),
        ("(решта 5)", "error: type error: cdr expects a non-empty list"),
        ("невідомий-символ", "error: unknown symbol"),
    ] {
        let rejected = evaluate(session, source);
        assert!(
            rejected.starts_with(expected_prefix),
            "unexpected error for {source:?}: {rejected}"
        );
        let recovery = evaluate(session, "(quote жива-сесія)");
        assert_eq!(recovery, "жива-сесія", "session did not recover after {source:?}");
    }

    let source = format!("(f {})", std::iter::repeat_n("a", 256).collect::<Vec<_>>().join(" "));
    let rejected = evaluate(session, &source);
    assert!(rejected.starts_with("error:"), "reader accepted oversized form: {rejected}");

    let recovery = evaluate(session, "(quote жива-сесія)");
    assert_eq!(recovery, "жива-сесія");
    unsafe { wsm_session_free(session) };
    println!("safe-eval-boundary-recovered");
}
