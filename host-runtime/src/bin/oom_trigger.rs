// Deliberately exhausts the 4096-byte arena and exits via wsm_fail_win64.
// Spawned by tests/oom_path.rs as a subprocess.

fn main() {
    // Keep allocating cons cells until the arena OOM path fires.
    loop {
        unsafe {
            let _ = wsm_my_lisp_cyberpunk_dll::wsm_cons(
                core::ptr::null_mut(),
                1,
                2,
            );
        }
    }
}
