//! ECO-UKRAINIAN-SOURCE-1 guard: product-owned Lisp scripts under
//! `scripts/` must have a Cyrillic-Ukrainian name, not an English/
//! abbreviated one. This is the "CI blocks English-surface regressions"
//! acceptance criterion, implemented as a `cargo test` since this repo
//! has no CI workflow yet -- run manually or wire into one later.
//!
//! Scope, deliberately narrow: only `scripts/*.my`/`*.мій` (product Lisp
//! source). Implementation files (`.cpp`/`.hpp`/`.rs`), docs, and this
//! test itself are out of scope -- ECO-UKRAINIAN-SOURCE-1 explicitly says
//! not to localize implementation files or external mandatory names.

use std::path::Path;

fn is_ascii_only_stem(stem: &str) -> bool {
    stem.chars().all(|c| c.is_ascii())
}

#[test]
fn product_scripts_have_ukrainian_names() {
    let scripts_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("scripts");
    let entries = std::fs::read_dir(&scripts_dir)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}", scripts_dir.display()));

    let mut offenders = Vec::new();
    for entry in entries {
        let entry = entry.expect("directory entry read failed");
        let path = entry.path();
        let Some(ext) = path.extension().and_then(|e| e.to_str()) else {
            continue;
        };
        if ext != "my" && ext != "мій" {
            continue; // not a product Lisp script (e.g. a stray README)
        }
        let stem = path
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or_default();
        if is_ascii_only_stem(stem) {
            offenders.push(path.display().to_string());
        }
    }

    assert!(
        offenders.is_empty(),
        "ECO-UKRAINIAN-SOURCE-1: these product scripts have ASCII-only \
         (English/abbreviated) names, not Ukrainian -- rename them:\n{}",
        offenders.join("\n")
    );
}
