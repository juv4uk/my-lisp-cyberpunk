//! Host-embeddable WSM runtime for my-lisp-cyberpunk (canonical home 2026-09-11).
//! Migrated from wsm-my-lisp/dll. Links asm/nucleus-win64.s (Microsoft x64 ABI).

#[cfg(not(windows))]
compile_error!(
    "host-runtime links asm/nucleus-win64.s (Microsoft x64) — Windows-only. "
    "SysV self-hosting lives in wsm-my-lisp asm/nucleus.s + harness."
);

pub mod eval;
pub mod ffi;
pub mod printer;
pub mod reader;
pub mod word;

core::arch::global_asm!(include_str!("../asm/nucleus-win64.s"), options(att_syntax));

unsafe extern "C" {
    #[allow(dead_code)]
    pub fn wsm_cons(context: *mut core::ffi::c_void, car: u64, cdr: u64) -> u64;
    #[allow(dead_code)]
    pub fn wsm_car(context: *mut core::ffi::c_void, pair: u64) -> u64;
    #[allow(dead_code)]
    pub fn wsm_cdr(context: *mut core::ffi::c_void, pair: u64) -> u64;
    #[allow(dead_code)]
    pub fn wsm_eq(context: *mut core::ffi::c_void, left: u64, right: u64) -> u64;
    #[allow(dead_code)]
    pub fn wsm_atom(context: *mut core::ffi::c_void, value: u64) -> u64;
    pub fn wsm_arena_reset(context: *mut core::ffi::c_void);
}

#[unsafe(no_mangle)]
pub extern "C" fn wsm_fail_win64(code: u32, a: u64, b: u64) -> ! {
    eprintln!("wsm-my-lisp-cyberpunk-dll: unrecoverable condition (code={code}, a={a}, b={b})");
    std::process::exit(97);
}

#[cfg(test)]
mod tests {
    use super::*;
    use wsm_os_target::{CANONICAL_T, NIL};

    #[test]
    fn cons_car_cdr_roundtrip() {
        unsafe {
            let pair = wsm_cons(core::ptr::null_mut(), 10, 20);
            assert_eq!(wsm_car(core::ptr::null_mut(), pair), 10);
            assert_eq!(wsm_cdr(core::ptr::null_mut(), pair), 20);
        }
    }

    #[test]
    fn eq_matches_and_distinguishes() {
        unsafe {
            assert_eq!(wsm_eq(core::ptr::null_mut(), 41, 41), CANONICAL_T);
            assert_eq!(wsm_eq(core::ptr::null_mut(), 41, 42), NIL);
        }
    }

    #[test]
    fn atom_distinguishes_cons_from_non_cons() {
        unsafe {
            let pair = wsm_cons(core::ptr::null_mut(), 1, 2);
            assert_eq!(wsm_atom(core::ptr::null_mut(), pair), NIL);
            assert_eq!(wsm_atom(core::ptr::null_mut(), CANONICAL_T), CANONICAL_T);
        }
    }
}
