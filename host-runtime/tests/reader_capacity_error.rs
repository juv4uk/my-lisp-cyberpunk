use std::process::Command;

#[test]
fn malformed_lisp_cannot_terminate_the_checked_host_session() {
    let output = Command::new(env!("CARGO_BIN_EXE_reader_capacity_trigger"))
        .output()
        .expect("failed to run reader_capacity_trigger subprocess");

    assert!(
        output.status.success(),
        "malformed Lisp terminated the subprocess: status={:?}, stdout={:?}, stderr={:?}",
        output.status,
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert_eq!(String::from_utf8_lossy(&output.stdout).trim(), "safe-eval-boundary-recovered");
}
