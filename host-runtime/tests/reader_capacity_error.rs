use std::process::Command;

#[test]
fn oversized_source_returns_an_error_and_keeps_the_session_usable() {
    let output = Command::new(env!("CARGO_BIN_EXE_reader_capacity_trigger"))
        .output()
        .expect("failed to run reader_capacity_trigger subprocess");

    assert!(
        output.status.success(),
        "oversized source terminated the subprocess: status={:?}, stdout={:?}, stderr={:?}",
        output.status,
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    assert_eq!(String::from_utf8_lossy(&output.stdout).trim(), "reader-capacity-error-recovered");
}
