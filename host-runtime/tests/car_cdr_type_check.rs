//! Exercises the real wsm_car/wsm_cdr type-check path (asm/nucleus-win64.s,
//! ported from wsm-my-lisp commit fdf9281) end to end, via a subprocess --
//! this can't be a plain #[test] in lib.rs because wsm_fail_win64 calls
//! std::process::exit(97), which would kill the whole in-process test
//! runner rather than just the one test. Mirrors tests/oom_path.rs.

use std::process::Command;

#[test]
fn car_of_non_cons_exits_97_with_type_error() {
    let output = Command::new(env!("CARGO_BIN_EXE_car-type-error-trigger"))
        .output()
        .expect("failed to run car-type-error-trigger subprocess");

    assert_eq!(
        output.status.code(),
        Some(97),
        "expected wsm_fail_win64's exit(97) for (car 5); got {:?}, stdout={:?}, stderr={:?}",
        output.status,
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("unrecoverable condition"),
        "expected wsm_fail_win64's own diagnostic message on stderr, got: {stderr}"
    );
    // ErrorCode::Type = 2, per asm/nucleus-win64.s's wsm_car_type_error
    // (movl $2, %ecx before calling wsm_fail_win64).
    assert!(
        stderr.contains("code=2"),
        "expected Type's ErrorCode=2 in the diagnostic, got: {stderr}"
    );

    // No stdout output expected: the "did not hit" println in
    // car_type_error_trigger.rs must never run if the fix works.
    assert!(
        output.stdout.is_empty(),
        "car-type-error-trigger printed to stdout, meaning (car 5) did NOT hit \
         the type check as expected: {:?}",
        String::from_utf8_lossy(&output.stdout)
    );
}
