// Real subprocess OOM path witness — must not run in-process (process::exit).

use std::process::Command;

#[test]
fn oom_trigger_exits_with_wsm_fail_code() {
    let bin = env!("CARGO_BIN_EXE_oom-trigger");
    let output = Command::new(bin)
        .output()
        .expect("spawn oom-trigger");
    assert_eq!(
        output.status.code(),
        Some(97),
        "stderr: {}",
        String::from_utf8_lossy(&output.stderr)
    );
}
