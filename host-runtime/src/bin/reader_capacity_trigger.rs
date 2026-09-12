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
    let source = format!("(f {})", std::iter::repeat_n("a", 256).collect::<Vec<_>>().join(" "));
    let rejected = evaluate(session, &source);
    assert!(rejected.starts_with("error:"), "reader accepted oversized form: {rejected}");

    let recovery = evaluate(session, "(quote жива-сесія)");
    assert_eq!(recovery, "жива-сесія");
    unsafe { wsm_session_free(session) };
    println!("reader-capacity-error-recovered");
}
